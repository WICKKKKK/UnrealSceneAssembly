"""External MCP server process management for UnrealSceneAssembly."""

from __future__ import annotations

import os
import shutil
import socket
import subprocess
import sys
import threading
from dataclasses import dataclass, field
from typing import Callable, TextIO, cast

from .bridge_config import BridgeConfig


class MCPServerProcess:
    def __init__(self, config: BridgeConfig):
        self.config = config
        self.process: subprocess.Popen[str] | None = None
        self.already_running = False
        self._output_thread: threading.Thread | None = None

    @property
    def is_running(self) -> bool:
        if self.already_running:
            return True
        return self.process is not None and self.process.poll() is None

    def start(self) -> bool:
        if self.is_running:
            return True

        port_status = check_tcp_port_available(self.config.mcp_host, self.config.mcp_port)
        if not port_status.available:
            print(f"[SceneAssembly MCP] MCP HTTP port is not available at {self.config.mcp_url}: {port_status.message}")
            return False

        if not os.path.exists(self.config.server_script):
            print(f"[SceneAssembly MCP] MCP server script is missing: {self.config.server_script}")
            return False

        python_exe = find_python_executable()
        if not python_exe:
            print("[SceneAssembly MCP] Unable to find a Python executable for the external MCP server.")
            print(f"[SceneAssembly MCP] Set UESA_MCP_PYTHON or install dependencies manually: {install_hint(self.config, '<python>')}")
            return False

        dependency_status = check_dependencies(self.config)
        if not dependency_status.installed:
            print("[SceneAssembly MCP] MCP Python dependencies are not installed in the plugin-local target directory.")
            if dependency_status.missing:
                print(f"[SceneAssembly MCP] Missing packages: {', '.join(dependency_status.missing)}")
            print(f"[SceneAssembly MCP] Install them with: {install_hint(self.config, python_exe)}")
            return False

        env = self._build_environment()
        args = [python_exe, self.config.server_script]
        creationflags = getattr(subprocess, "CREATE_NO_WINDOW", 0)

        try:
            self.process = subprocess.Popen(
                args,
                cwd=self.config.server_dir,
                env=env,
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                text=True,
                encoding="utf-8",
                errors="replace",
                bufsize=1,
                creationflags=creationflags,
            )
        except Exception as exc:
            print(f"[SceneAssembly MCP] Failed to start external MCP server: {exc}")
            return False

        self._output_thread = threading.Thread(
            target=self._drain_output,
            args=(self.process.stdout,),
            name="SceneAssemblyMCPOutput",
            daemon=True,
        )
        self._output_thread.start()
        print(f"[SceneAssembly MCP] Started external MCP server at {self.config.mcp_url}.")
        return True

    def stop(self) -> None:
        self.already_running = False
        if self.process is None:
            return

        if self.process.poll() is None:
            self.process.terminate()
            try:
                self.process.wait(timeout=3.0)
            except subprocess.TimeoutExpired:
                self.process.kill()
                self.process.wait(timeout=3.0)
        self.process = None

    def _build_environment(self) -> dict[str, str]:
        env = os.environ.copy()
        python_paths = [self.config.server_dir, self.config.site_packages_dir]
        existing = env.get("PYTHONPATH")
        if existing:
            python_paths.append(existing)
        env["PYTHONPATH"] = os.pathsep.join(python_paths)
        env["PYTHONUNBUFFERED"] = "1"
        env["UESA_MCP_HOST"] = self.config.mcp_host
        env["UESA_MCP_PORT"] = str(self.config.mcp_port)
        env["UESA_MCP_PATH"] = self.config.mcp_path
        env["UESA_BRIDGE_HOST"] = self.config.bridge_host
        env["UESA_BRIDGE_PORT"] = str(self.config.bridge_port)
        env["UESA_MCP_REQUEST_TIMEOUT"] = str(self.config.request_timeout_seconds)
        return env

    def _drain_output(self, stream: TextIO | None) -> None:
        if stream is None:
            return
        try:
            for line in stream:
                print(f"[SceneAssembly MCP Server] {line.rstrip()}")
        except Exception:
            pass


@dataclass(frozen=True)
class DependencyStatus:
    installed: bool
    missing: list[str] = field(default_factory=list)
    allow_global_deps: bool = False


@dataclass(frozen=True)
class PortStatus:
    available: bool
    host: str
    port: int
    message: str


@dataclass(frozen=True)
class InstallResult:
    ok: bool
    returncode: int | None
    command: list[str]
    output: str


def check_dependencies(config: BridgeConfig) -> DependencyStatus:
    if os.environ.get("UESA_MCP_ALLOW_GLOBAL_DEPS", "").strip().lower() in {"1", "true", "yes", "on"}:
        return DependencyStatus(installed=True, allow_global_deps=True)

    required_paths = {
        "mcp": os.path.join(config.site_packages_dir, "mcp"),
    }
    if sys.platform == "win32":
        required_paths["pywin32"] = os.path.join(config.site_packages_dir, "pywin32_system32")

    missing = [name for name, path in required_paths.items() if not os.path.exists(path)]
    return DependencyStatus(installed=not missing, missing=missing)


def dependencies_installed(config: BridgeConfig) -> bool:
    return check_dependencies(config).installed


def check_tcp_port_available(host: str, port: int) -> PortStatus:
    try:
        port = int(port)
    except (TypeError, ValueError):
        return PortStatus(False, host, 0, "Port must be a number between 1 and 65535.")
    if not (1 <= port <= 65535):
        return PortStatus(False, host, port, "Port must be between 1 and 65535.")

    probe = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    try:
        probe.bind((host, port))
        return PortStatus(True, host, port, "Port is available.")
    except OSError as exc:
        return PortStatus(False, host, port, str(exc))
    finally:
        probe.close()


def install_hint(config: BridgeConfig, python_exe: str | None = None) -> str:
    python_exe = python_exe or find_python_executable() or "<python>"
    requirements = os.path.join(config.server_dir, "requirements.txt")
    return f'"{python_exe}" -m pip install -r "{requirements}" --target "{config.site_packages_dir}"'


def install_dependencies(
    config: BridgeConfig,
    python_exe: str | None = None,
    output_callback: Callable[[str], None] | None = None,
) -> InstallResult:
    python_exe = python_exe or find_python_executable()
    requirements = os.path.join(config.server_dir, "requirements.txt")
    if not python_exe:
        return InstallResult(False, None, [], "Unable to find a Python executable.")
    if not os.path.exists(requirements):
        return InstallResult(False, None, [], f"Missing requirements file: {requirements}")

    os.makedirs(config.site_packages_dir, exist_ok=True)
    command = [python_exe, "-m", "pip", "install", "-r", requirements, "--target", config.site_packages_dir, "--upgrade"]
    output_lines: list[str] = []

    creationflags = getattr(subprocess, "CREATE_NO_WINDOW", 0)
    process = subprocess.Popen(
        command,
        cwd=config.server_dir,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        encoding="utf-8",
        errors="replace",
        bufsize=1,
        creationflags=creationflags,
    )
    if process.stdout is not None:
        for line in process.stdout:
            line = line.rstrip()
            output_lines.append(line)
            if output_callback is not None:
                output_callback(line)

    returncode = process.wait()
    return InstallResult(returncode == 0, returncode, command, "\n".join(output_lines))


def find_python_executable() -> str | None:
    env_python = os.environ.get("UESA_MCP_PYTHON", "").strip().strip('"')
    if env_python and os.path.exists(env_python):
        return env_python

    current = sys.executable
    if current and os.path.exists(current) and "python" in os.path.basename(current).lower():
        return current

    engine_python = _engine_python_executable()
    if engine_python:
        return engine_python

    for name in ("python", "python3"):
        candidate = shutil.which(name)
        if candidate:
            return candidate
    return None


def _engine_python_executable() -> str | None:
    try:
        import unreal

        engine_dir = cast(str, unreal.Paths.engine_dir())
        converter = getattr(unreal.Paths, "convert_relative_path_to_full", None)
        if callable(converter):
            engine_dir = cast(str, converter(engine_dir))
    except Exception:
        return None

    candidates = [
        os.path.join(engine_dir, "Binaries", "ThirdParty", "Python3", "Win64", "python.exe"),
        os.path.join(engine_dir, "Binaries", "ThirdParty", "Python3", "Mac", "bin", "python3"),
        os.path.join(engine_dir, "Binaries", "ThirdParty", "Python3", "Linux", "bin", "python3"),
    ]
    for candidate in candidates:
        if os.path.exists(candidate):
            return candidate
    return None

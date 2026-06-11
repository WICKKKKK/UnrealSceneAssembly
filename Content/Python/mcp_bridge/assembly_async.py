"""Incremental background assembly workflow for the Slate test panel."""

from __future__ import annotations

import queue
import threading
import traceback
import uuid
from typing import Any

import unreal

from clip_retrieval import (
    candidates_from_hits,
    clip_search_assets_by_image,
    default_clip_output_fields,
    dinov3_search_assets_by_image,
)

from . import assembly_test
from .scene_handlers import (
    SceneCommandError,
    _actor_summary,
    _find_actor_by_path,
    _spawn_asset_no_transaction,
    _success,
)
from .solver_handlers import (
    _candidate_structs,
    _pick_result_index,
    _placement_result_dict,
    _settings_struct,
    _solver_library,
)


MAX_RESULTS_PER_TICK = 1


class AssemblyJob:
    def __init__(self, params: dict[str, Any]):
        self.job_id = uuid.uuid4().hex
        self.params = dict(params or {})
        self.tag = assembly_test._result_tag(self.params)
        self.base_seed: int | None = None
        self.capture_context: dict[str, Any] = {}
        self.prepared_items: list[dict[str, Any]] = []
        self.items: list[dict[str, Any]] = []
        self.log_lines: list[str] = []
        self.cleanup: dict[str, Any] = {"tag": self.tag, "deleted_count": 0}

        self.total = 0
        self.completed = 0
        self.succeeded = 0
        self.spawned_count = 0
        self.failed = 0

        self.state = "pending"
        self.message = "等待启动。"
        self.error: str | None = None
        self.traceback: str | None = None

        self._queue: queue.Queue[dict[str, Any]] = queue.Queue()
        self._cancel_event = threading.Event()
        self._worker_thread: threading.Thread | None = None
        self._tick_handle: Any = None
        self._lock = threading.RLock()
        self._worker_done = False

    @property
    def is_active(self) -> bool:
        return self.state in {"pending", "preparing", "running", "cancelling"}

    def prepare(self) -> None:
        self._set_state("preparing", "正在准备捕获数据和裁剪图像。")
        capture_context = assembly_test._capture_context(self.params)
        actors = capture_context["actors"]
        if not actors:
            raise SceneCommandError("No processable actors in the capture JSON. Capture selected Blockout actors first.")

        self.base_seed = assembly_test._base_random_seed(self.params)
        self.capture_context = capture_context
        self.prepared_items = [self._prepare_actor(index, actor) for index, actor in enumerate(actors)]
        self.total = len(self.prepared_items)

        with unreal.ScopedEditorTransaction("Scene Assembly: Cleanup Async Results"):
            deleted = assembly_test._cleanup_tagged_no_transaction(self.tag)
        self.cleanup = {"tag": self.tag, "deleted_count": deleted}
        self._append_log(f"已清理旧结果：{deleted} 个。")

    def start(self) -> None:
        if self.total <= 0:
            raise SceneCommandError("No processable actors in the capture JSON. Capture selected Blockout actors first.")
        if self._tick_handle is None:
            self._tick_handle = unreal.register_slate_post_tick_callback(self._tick)
        self._worker_thread = threading.Thread(target=self._worker, name="SceneAssemblyAsyncJob", daemon=True)
        self._set_state("running", f"后台装配任务已启动：0/{self.total}。")
        self._worker_thread.start()

    def cancel(self) -> dict[str, Any]:
        self._cancel_event.set()
        with self._lock:
            if self.state == "running":
                self.state = "cancelling"
            self.message = "正在取消：不再发起新的资产检索，已完成的结果仍会摆放。"
            self.log_lines.append(self.message)
            del self.log_lines[:-80]
        return self.status()

    def shutdown(self) -> None:
        self._cancel_event.set()
        self._unregister_tick()

    def status(self) -> dict[str, Any]:
        with self._lock:
            progress = float(self.completed) / float(self.total) if self.total > 0 else 0.0
            return _success(
                job_id=self.job_id,
                state=self.state,
                running=self.is_active,
                cancel_requested=self._cancel_event.is_set(),
                worker_done=self._worker_done,
                message=self.message,
                error=self.error,
                traceback=self.traceback,
                total=self.total,
                actor_count=self.total,
                completed=self.completed,
                progress=progress,
                succeeded=self.succeeded,
                spawned_count=self.spawned_count,
                failed=self.failed,
                cleanup=dict(self.cleanup),
                random_seed=self.base_seed,
                whitebox_only=assembly_test._bool_param(self.params, "whitebox_only", True),
                capture_json_path=self.capture_context.get("capture_json_path"),
                concept_art_path=self.capture_context.get("concept_art_path"),
                skipped_non_whitebox=[],
                items=list(self.items),
                log_lines=list(self.log_lines[-40:]),
            )

    def _prepare_actor(self, actor_index: int, actor: Any) -> dict[str, Any]:
        actor_path = actor.get_path_name()
        id_entry = self.capture_context["entry_by_path"].get(actor_path, {})
        item: dict[str, Any] = {
            "actor": _actor_summary(actor, include_bounds=True),
            "actor_path": actor_path,
            "actor_index": actor_index,
            "status": "pending",
            "results": [],
            "count": 0,
            "chosen_index": 0,
            "random_seed": None,
            "spawned": None,
        }

        try:
            bbox_source = assembly_test._crop_bbox_source(self.params)
            bbox = assembly_test._bbox_from_entry(id_entry, bbox_source)
            item["crop_bbox_source"] = bbox_source
            item["pixel_bbox"] = assembly_test._bbox_from_entry(id_entry, "pixel_bbox")
            item["full_bbox"] = assembly_test._bbox_from_entry(id_entry, "full_bbox")
            if not bbox:
                item["status"] = "no_full_bbox" if bbox_source == "full_bbox" else "no_pixel_bbox"
                item["error"] = f"Capture JSON does not contain a valid {bbox_source} for this actor."
                return {"item": item, "data_uri": None}

            data_uri = assembly_test._call_unreal_static(
                assembly_test._scene_capture_library(),
                "crop_image_region_to_base64",
                self.capture_context["concept_art_path"],
                int(self.capture_context["image_width"]),
                int(self.capture_context["image_height"]),
                int(bbox[0]),
                int(bbox[1]),
                int(bbox[2]),
                int(bbox[3]),
                max(0, assembly_test._int_param(self.params, "crop_expand_pixels", 20)),
            )
            if not data_uri:
                item["status"] = "crop_error"
                item["error"] = "Failed to crop the concept art image for this actor."
                return {"item": item, "data_uri": None}
            return {"item": item, "data_uri": data_uri}
        except Exception as exc:
            item["status"] = "error"
            item["error"] = str(exc)
            return {"item": item, "data_uri": None}

    def _worker(self) -> None:
        try:
            for prepared in self.prepared_items:
                item = dict(prepared.get("item") or {})
                data_uri = prepared.get("data_uri")
                if item.get("status") != "pending":
                    self._queue.put({"type": "item", "item": item})
                    continue
                if self._cancel_event.is_set():
                    break
                self._queue.put({"type": "item", "item": self._search_item(item, str(data_uri or ""))})
        except Exception as exc:
            self._queue.put({"type": "worker_error", "error": str(exc), "traceback": traceback.format_exc()})
        finally:
            self._queue.put({"type": "done"})

    def _search_item(self, item: dict[str, Any], data_uri: str) -> dict[str, Any]:
        try:
            candidate_limit = assembly_test._int_param(self.params, "candidate_limit", 20)
            timeout = assembly_test._float_param(self.params, "timeout", 15.0)
            retrieval_model = assembly_test._retrieval_model(self.params)
            common_search_kwargs = {
                "image_url": data_uri,
                "limit": candidate_limit,
                "project_names": assembly_test._optional_string_list(self.params.get("project_names")),
                "asset_types": assembly_test._optional_string_list(self.params.get("asset_types")),
                "filters": self.params.get("filters") if isinstance(self.params.get("filters"), dict) else None,
                "output_fields": default_clip_output_fields(include_bounding_box=True),
                "ef": assembly_test._int_param(self.params, "ef", 64),
                "timeout": timeout,
            }
            if retrieval_model == "dinov3":
                search = dinov3_search_assets_by_image(
                    **common_search_kwargs,
                    collection=assembly_test._optional_string(self.params.get("collection_dinov3") or self.params.get("dinov3_collection")),
                )
            else:
                search = clip_search_assets_by_image(
                    **common_search_kwargs,
                    score_threshold=assembly_test._float_param(self.params, "score_threshold", 0.0),
                    collection=assembly_test._optional_string(self.params.get("collection") or self.params.get("collection_clip") or self.params.get("clip_collection")),
                )

            item["retrieval_model"] = retrieval_model
            candidates, skipped = candidates_from_hits(search.get("hits") or [])
            item["search"] = assembly_test._search_summary(search, candidates, skipped)
            if not candidates:
                item["status"] = "no_candidates"
                item["error"] = "Image search returned no candidates with asset_path and bounding_box."
                return item
            item["status"] = "searched"
            item["candidates"] = candidates
            return item
        except Exception as exc:
            item["status"] = "error"
            item["error"] = str(exc)
            return item

    def _tick(self, delta_seconds: float) -> None:
        try:
            processed = 0
            while processed < MAX_RESULTS_PER_TICK:
                try:
                    message = self._queue.get_nowait()
                except queue.Empty:
                    break

                message_type = message.get("type")
                if message_type == "done":
                    self._worker_done = True
                    if self._queue.empty():
                        final_state = "cancelled" if self._cancel_event.is_set() else "done"
                        final_text = "装配任务已取消。" if final_state == "cancelled" else "装配任务已完成。"
                        self._set_state(final_state, final_text)
                        self._unregister_tick()
                        break
                    self._queue.put(message)
                    break
                if message_type == "worker_error":
                    self.error = str(message.get("error") or "Worker failed.")
                    self.traceback = str(message.get("traceback") or "")
                    self._append_log(f"后台任务异常：{self.error}")
                    self._set_state("error", self.error)
                    self._unregister_tick()
                    break
                if message_type == "item":
                    self._process_item_on_main(dict(message.get("item") or {}))
                    processed += 1
        except Exception as exc:
            self.error = str(exc)
            self.traceback = traceback.format_exc()
            self._append_log(f"主线程 Tick 异常：{self.error}")
            self._set_state("error", self.error)
            self._unregister_tick()

    def _process_item_on_main(self, item: dict[str, Any]) -> None:
        if item.get("status") == "searched":
            try:
                actor_path = str(item.get("actor_path") or "")
                actor = _find_actor_by_path(actor_path)
                settings = _settings_struct(self.params.get("settings") or assembly_test._settings_from_params(self.params))
                scene_obb = _solver_library().get_actor_obb(actor)
                candidates = item.pop("candidates", [])
                results = [_placement_result_dict(result) for result in _solver_library().solve_placement(scene_obb, _candidate_structs(candidates), settings)]
                item["results"] = results
                item["count"] = len(results)
                if not results:
                    item["status"] = "no_results"
                    item["error"] = "Solver returned no placement results."
                else:
                    self._place_best_result(item, actor, results)
            except Exception as exc:
                item.pop("candidates", None)
                item["status"] = "error"
                item["error"] = str(exc)
        else:
            item.pop("candidates", None)

        self._record_item(item)

    def _place_best_result(self, item: dict[str, Any], actor: Any, results: list[dict[str, Any]]) -> None:
        actor_index = int(item.get("actor_index") or 0)
        pick_params = dict(self.params)
        if self.base_seed is not None and len(results) > 1:
            pick_params["random_seed"] = self.base_seed + actor_index
        chosen_index, random_seed = _pick_result_index(pick_params, len(results))
        best = results[chosen_index]
        label = assembly_test._optional_string(self.params.get("label")) or f"SceneAssembly_{assembly_test._safe_label(actor.get_actor_label())}"
        item["chosen_index"] = chosen_index
        item["random_seed"] = random_seed
        spawn_params = {
            "asset_path": best["asset_path"],
            "location": best["transform"]["location"],
            "rotation": best["transform"]["rotation"],
            "scale": best["transform"]["scale"],
            "label": label,
            "tags": [self.tag],
        }
        try:
            with unreal.ScopedEditorTransaction("Scene Assembly: Place Async Result"):
                spawned_actor = _spawn_asset_no_transaction(spawn_params)
            item["spawned"] = _actor_summary(spawned_actor, include_bounds=True)
            item["status"] = "placed"
        except Exception as exc:
            item["spawned"] = None
            item["status"] = "spawn_error"
            item["error"] = str(exc)

    def _record_item(self, item: dict[str, Any]) -> None:
        status = str(item.get("status") or "unknown")
        actor_value = item.get("actor")
        actor: dict[str, Any] = actor_value if isinstance(actor_value, dict) else {}
        label = str(actor.get("label") or actor.get("name") or item.get("actor_path") or "Actor")
        with self._lock:
            self.items.append(item)
            self.completed += 1
            if status == "placed":
                self.succeeded += 1
                self.spawned_count += 1
            else:
                self.failed += 1
            self.message = f"后台装配中：{self.completed}/{self.total}，已摆放 {self.spawned_count}。"
            self.log_lines.append(f"{self.completed}/{self.total} {label}: {status}")
            del self.log_lines[:-80]

    def _set_state(self, state: str, message: str) -> None:
        with self._lock:
            self.state = state
            self.message = message

    def _append_log(self, line: str) -> None:
        with self._lock:
            self.log_lines.append(line)
            del self.log_lines[:-80]

    def _unregister_tick(self) -> None:
        tick_handle = self._tick_handle
        if tick_handle is None:
            return
        self._tick_handle = None
        try:
            unreal.unregister_slate_post_tick_callback(tick_handle)
        except Exception:
            pass


_current_job: AssemblyJob | None = None
_jobs_lock = threading.RLock()


def _job_error_response(message: str, job: AssemblyJob) -> dict[str, Any]:
    payload = job.status()
    payload["ok"] = False
    payload["status"] = "error"
    payload["message"] = message
    payload["error"] = message
    return payload


def start_async_assembly(params: dict[str, Any] | None = None) -> dict[str, Any]:
    global _current_job
    params = params or {}
    with _jobs_lock:
        if _current_job is not None and _current_job.is_active:
            return _job_error_response("An assembly job is already running.", _current_job)
        job = AssemblyJob(params)
        _current_job = job
        setattr(unreal, "_scene_assembly_async_job", job)

    try:
        job.prepare()
        job.start()
        return job.status()
    except Exception as exc:
        job.error = str(exc)
        job.traceback = traceback.format_exc()
        job._set_state("error", str(exc))
        job.shutdown()
        return _job_error_response(str(exc), job)


def poll_assembly_status() -> dict[str, Any]:
    with _jobs_lock:
        if _current_job is None:
            return _success(state="idle", running=False, total=0, completed=0, progress=0.0, spawned_count=0, failed=0, items=[], log_lines=[])
        return _current_job.status()


def cancel_assembly() -> dict[str, Any]:
    with _jobs_lock:
        if _current_job is None or not _current_job.is_active:
            return _success(state="idle", running=False, message="当前没有正在运行的装配任务。")
        return _current_job.cancel()


def shutdown_async_assembly() -> None:
    with _jobs_lock:
        if _current_job is not None:
            _current_job.shutdown()

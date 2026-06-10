#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class FUICommandList;
class SDockTab;
class SWidget;
class FSpawnTabArgs;

class FUnrealSceneAssemblyModule : public IModuleInterface
{
public:
	void StartupModule() override;
	void ShutdownModule() override;

	static FString ExecutePythonControllerCommand(const FString& FunctionCall);
	static void StopPythonMCPRuntime();

private:
	void RegisterMenus();
	void OpenMCPControlPanel();
	void OpenTestPanel();
	TSharedRef<SDockTab> SpawnMCPControlPanelTab(const FSpawnTabArgs& SpawnTabArgs);
	TSharedRef<SDockTab> SpawnTestPanelTab(const FSpawnTabArgs& SpawnTabArgs);
};

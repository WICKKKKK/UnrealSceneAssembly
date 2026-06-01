#pragma once

#include "CoreMinimal.h"
#include "Types/SlateEnums.h"
#include "Widgets/SCompoundWidget.h"

class FJsonObject;
class SEditableTextBox;

class SMCPControlPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SMCPControlPanel) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	EActiveTimerReturnType RefreshTick(double InCurrentTime, float InDeltaTime);
	void RefreshStatus();
	bool CallController(const FString& FunctionCall, TSharedPtr<FJsonObject>& OutObject);
	void ApplyResponse(const TSharedPtr<FJsonObject>& Response);
	void ApplyPortResponse(const TSharedPtr<FJsonObject>& Response, bool bForceApply);
	void AppendMessage(const FString& Message);

	FReply OnRefreshClicked();
	FReply OnCheckEnvironmentClicked();
	FReply OnInstallDependenciesClicked();
	FReply OnCheckPortClicked();
	FReply OnStartClicked();
	FReply OnStopClicked();
	void OnPortTextChanged(const FText& NewText);

	bool CanInstallDependencies() const;
	bool CanStart() const;
	bool CanStop() const;
	int32 GetPortValue() const;

	FText GetStatusText() const;
	FText GetEnvironmentText() const;
	FText GetPortStatusText() const;
	FText GetInstallLogText() const;
	FText GetAgentUrlText() const;

	TSharedPtr<SEditableTextBox> PortTextBox;

	bool bRunning = false;
	bool bDependenciesInstalled = false;
	bool bInstallRunning = false;
	bool bPortAvailable = false;
	bool bUserEditedPort = false;
	int32 LastCheckedPort = 0;
	int32 ConfigPort = 8780;
	FString EndpointUrl = TEXT("http://127.0.0.1:8780/mcp");
	FString LastMessage = TEXT("Ready.");
	FString EnvironmentMessage = TEXT("Environment not checked.");
	FString PortMessage = TEXT("Port not checked.");
	FString InstallLog;
};

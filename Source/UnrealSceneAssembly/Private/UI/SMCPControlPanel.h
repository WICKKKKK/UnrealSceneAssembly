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
	bool CheckCurrentPort(bool bAppendToLog);
	FString GetCurrentEndpointUrl() const;

	FReply OnRefreshClicked();
	FReply OnInstallDependenciesClicked();
	FReply OnCheckPortClicked();
	FReply OnPrimaryButtonClicked();
	FReply OnCopyUrlClicked();
	void OnPortTextChanged(const FText& NewText);
	void OnPortTextCommitted(const FText& NewText, ETextCommit::Type CommitType);

	bool CanInstallDependencies() const;
	bool CanToggleServer() const;
	bool CanStart() const;
	bool CanStop() const;
	int32 GetPortValue() const;

	FText GetStatusPillText() const;
	FSlateColor GetStatusPillColor() const;
	FText GetStatusSummaryText() const;
	FText GetPrimaryButtonText() const;
	FText GetPrimaryButtonTooltip() const;
	FSlateColor GetPrimaryButtonColor() const;
	FText GetEndpointUrlText() const;
	FText GetDependencyStatusText() const;
	FSlateColor GetDependencyColor() const;
	FText GetInstallButtonText() const;
	EVisibility GetInstallButtonVisibility() const;
	FText GetPortBadgeText() const;
	FSlateColor GetPortBadgeColor() const;
	FText GetPortHelpText() const;
	FText GetInstallLogText() const;

	TSharedPtr<SEditableTextBox> PortTextBox;

	bool bRunning = false;
	bool bDependenciesInstalled = false;
	bool bInstallRunning = false;
	bool bPortAvailable = false;
	bool bUserEditedPort = false;
	bool bUpdatingPortText = false;
	int32 LastCheckedPort = 0;
	int32 ConfigPort = 8780;
	FString EndpointUrl = TEXT("http://127.0.0.1:8780/mcp");
	FString LastMessage = TEXT("Ready.");
	FString EnvironmentMessage = TEXT("Checking Python dependencies...");
	FString PortMessage = TEXT("Checking port...");
	FString InstallLog = TEXT("No runtime messages.");
};

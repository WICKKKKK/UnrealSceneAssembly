#include "UI/SMCPControlPanel.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/PlatformApplicationMisc.h"
#include "Misc/DefaultValueHelper.h"
#include "Misc/MessageDialog.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"
#include "UnrealSceneAssembly.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SMCPControlPanel"

namespace
{
static const FLinearColor ReadyColor(0.18f, 0.72f, 0.38f, 1.0f);
static const FLinearColor RunningColor(0.13f, 0.56f, 0.95f, 1.0f);
static const FLinearColor WarningColor(1.0f, 0.64f, 0.16f, 1.0f);
static const FLinearColor ErrorColor(0.92f, 0.24f, 0.24f, 1.0f);
static const FLinearColor MutedColor(0.58f, 0.58f, 0.58f, 1.0f);

FString GetStringField(const TSharedPtr<FJsonObject>& Object, const TCHAR* FieldName, const FString& DefaultValue = FString())
{
	FString Value;
	return Object.IsValid() && Object->TryGetStringField(FieldName, Value) ? Value : DefaultValue;
}

bool GetBoolField(const TSharedPtr<FJsonObject>& Object, const TCHAR* FieldName, bool DefaultValue = false)
{
	bool Value = DefaultValue;
	return Object.IsValid() && Object->TryGetBoolField(FieldName, Value) ? Value : DefaultValue;
}

int32 GetIntField(const TSharedPtr<FJsonObject>& Object, const TCHAR* FieldName, int32 DefaultValue = 0)
{
	double Value = 0.0;
	return Object.IsValid() && Object->TryGetNumberField(FieldName, Value) ? static_cast<int32>(Value) : DefaultValue;
}

TSharedPtr<FJsonObject> GetObjectField(const TSharedPtr<FJsonObject>& Object, const TCHAR* FieldName)
{
	const TSharedPtr<FJsonObject>* Child = nullptr;
	return Object.IsValid() && Object->TryGetObjectField(FieldName, Child) && Child != nullptr ? *Child : nullptr;
}

FText PortToText(int32 Port)
{
	return FText::FromString(FString::FromInt(Port));
}

TSharedRef<SWidget> MakeCard(const TSharedRef<SWidget>& Content)
{
	return SNew(SBorder)
		.Padding(16.0f)
		.BorderImage(FAppStyle::GetBrush("Brushes.Recessed"))
		[
			Content
		];
}
}

void SMCPControlPanel::Construct(const FArguments& InArgs)
{
	ChildSlot
	[
		SNew(SBorder)
		.Padding(18.0f)
		.BorderImage(FAppStyle::GetBrush("Brushes.Panel"))
		[
			SNew(SScrollBox)
			+ SScrollBox::Slot()
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					.VAlign(VAlign_Center)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot()
						.AutoHeight()
						[
							SNew(STextBlock)
							.Text(LOCTEXT("Title", "Scene Assembly MCP"))
							.Font(FCoreStyle::GetDefaultFontStyle("Bold", 22))
						]
						+ SVerticalBox::Slot()
						.AutoHeight()
						.Padding(0.0f, 4.0f, 0.0f, 0.0f)
						[
							SNew(STextBlock)
							.Text(LOCTEXT("Subtitle", "Connect MCP agents to the current Unreal project."))
							.ColorAndOpacity(FSlateColor(MutedColor))
						]
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					[
						SNew(SBorder)
						.Padding(FMargin(12.0f, 6.0f))
						.BorderImage(FAppStyle::GetBrush("Brushes.Recessed"))
						.BorderBackgroundColor(FSlateColor(FLinearColor(0.16f, 0.16f, 0.16f, 1.0f)))
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot()
							.AutoWidth()
							.VAlign(VAlign_Center)
							.Padding(0.0f, 0.0f, 6.0f, 0.0f)
							[
								SNew(STextBlock)
								.Text(FText::FromString(TEXT("\x2022")))
								.Font(FCoreStyle::GetDefaultFontStyle("Bold", 11))
								.ColorAndOpacity(this, &SMCPControlPanel::GetStatusPillColor)
							]
							+ SHorizontalBox::Slot()
							.AutoWidth()
							.VAlign(VAlign_Center)
							[
								SNew(STextBlock)
								.Text(this, &SMCPControlPanel::GetStatusPillText)
								.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
								.ColorAndOpacity(FSlateColor(FLinearColor(0.92f, 0.92f, 0.92f, 1.0f)))
							]
						]
					]
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 18.0f, 0.0f, 12.0f)
				[
					MakeCard(
						SNew(SVerticalBox)
						+ SVerticalBox::Slot()
						.AutoHeight()
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot()
							.FillWidth(1.0f)
							.VAlign(VAlign_Center)
							[
								SNew(SVerticalBox)
								+ SVerticalBox::Slot()
								.AutoHeight()
								[
									SNew(STextBlock)
									.Text(LOCTEXT("ServerHeader", "MCP Server"))
									.Font(FCoreStyle::GetDefaultFontStyle("Bold", 16))
								]
								+ SVerticalBox::Slot()
								.AutoHeight()
								.Padding(0.0f, 4.0f, 0.0f, 0.0f)
								[
									SNew(STextBlock)
									.Text(this, &SMCPControlPanel::GetStatusSummaryText)
									.ColorAndOpacity(FSlateColor(MutedColor))
									.AutoWrapText(true)
								]
							]
							+ SHorizontalBox::Slot()
							.AutoWidth()
							.VAlign(VAlign_Center)
							[
								SNew(SBox)
								.MinDesiredWidth(150.0f)
								.MinDesiredHeight(36.0f)
								[
								SNew(SButton)
								.Text(this, &SMCPControlPanel::GetPrimaryButtonText)
								.TextStyle(FAppStyle::Get(), "NormalText")
								.HAlign(HAlign_Center)
								.VAlign(VAlign_Center)
								.ForegroundColor(FSlateColor(FLinearColor::White))
									.ButtonColorAndOpacity(this, &SMCPControlPanel::GetPrimaryButtonColor)
									.ToolTipText(this, &SMCPControlPanel::GetPrimaryButtonTooltip)
									.IsEnabled(this, &SMCPControlPanel::CanToggleServer)
									.OnClicked(this, &SMCPControlPanel::OnPrimaryButtonClicked)
								]
							]
						]
						+ SVerticalBox::Slot()
						.AutoHeight()
						.Padding(0.0f, 14.0f, 0.0f, 14.0f)
						[
							SNew(SSeparator)
						]
						+ SVerticalBox::Slot()
						.AutoHeight()
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot()
							.FillWidth(1.0f)
							.VAlign(VAlign_Center)
							[
								SNew(SVerticalBox)
								+ SVerticalBox::Slot()
								.AutoHeight()
								[
									SNew(STextBlock)
									.Text(LOCTEXT("EndpointLabel", "Agent endpoint"))
									.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
									.ColorAndOpacity(FSlateColor(MutedColor))
								]
								+ SVerticalBox::Slot()
								.AutoHeight()
								.Padding(0.0f, 3.0f, 0.0f, 0.0f)
								[
									SNew(STextBlock)
									.Text(this, &SMCPControlPanel::GetEndpointUrlText)
									.Font(FCoreStyle::GetDefaultFontStyle("Mono", 10))
									.AutoWrapText(true)
								]
							]
							+ SHorizontalBox::Slot()
							.AutoWidth()
							.VAlign(VAlign_Center)
							.Padding(12.0f, 0.0f, 0.0f, 0.0f)
							[
								SNew(SButton)
								.Text(LOCTEXT("CopyUrl", "Copy"))
								.OnClicked(this, &SMCPControlPanel::OnCopyUrlClicked)
							]
						]
					)
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 0.0f, 0.0f, 12.0f)
				[
					MakeCard(
						SNew(SVerticalBox)
						+ SVerticalBox::Slot()
						.AutoHeight()
						[
							SNew(STextBlock)
							.Text(LOCTEXT("SetupHeader", "Setup"))
							.Font(FCoreStyle::GetDefaultFontStyle("Bold", 16))
						]
						+ SVerticalBox::Slot()
						.AutoHeight()
						.Padding(0.0f, 14.0f, 0.0f, 0.0f)
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot()
							.FillWidth(1.0f)
							.VAlign(VAlign_Center)
							[
								SNew(SVerticalBox)
								+ SVerticalBox::Slot()
								.AutoHeight()
								[
									SNew(STextBlock)
									.Text(LOCTEXT("DepsLabel", "Python runtime"))
									.Font(FCoreStyle::GetDefaultFontStyle("Bold", 12))
								]
								+ SVerticalBox::Slot()
								.AutoHeight()
								.Padding(0.0f, 3.0f, 0.0f, 0.0f)
								[
									SNew(STextBlock)
									.Text(this, &SMCPControlPanel::GetDependencyStatusText)
									.ColorAndOpacity(this, &SMCPControlPanel::GetDependencyColor)
									.AutoWrapText(true)
								]
							]
							+ SHorizontalBox::Slot()
							.AutoWidth()
							.VAlign(VAlign_Center)
							[
								SNew(SButton)
								.Text(this, &SMCPControlPanel::GetInstallButtonText)
								.Visibility(this, &SMCPControlPanel::GetInstallButtonVisibility)
								.IsEnabled(this, &SMCPControlPanel::CanInstallDependencies)
								.OnClicked(this, &SMCPControlPanel::OnInstallDependenciesClicked)
							]
						]
						+ SVerticalBox::Slot()
						.AutoHeight()
						.Padding(0.0f, 14.0f, 0.0f, 14.0f)
						[
							SNew(SSeparator)
						]
						+ SVerticalBox::Slot()
						.AutoHeight()
						[
							SNew(SVerticalBox)
							+ SVerticalBox::Slot()
							.AutoHeight()
							[
								SNew(STextBlock)
								.Text(LOCTEXT("PortLabel", "HTTP port"))
								.Font(FCoreStyle::GetDefaultFontStyle("Bold", 12))
							]
							+ SVerticalBox::Slot()
							.AutoHeight()
							.Padding(0.0f, 8.0f, 0.0f, 0.0f)
							[
								SNew(SHorizontalBox)
								+ SHorizontalBox::Slot()
								.AutoWidth()
								.VAlign(VAlign_Center)
								.Padding(0.0f, 0.0f, 10.0f, 0.0f)
								[
									SAssignNew(PortTextBox, SEditableTextBox)
									.MinDesiredWidth(120.0f)
									.Text(PortToText(ConfigPort))
									.IsReadOnly_Lambda([this]() { return bRunning; })
									.OnTextChanged(this, &SMCPControlPanel::OnPortTextChanged)
									.OnTextCommitted(this, &SMCPControlPanel::OnPortTextCommitted)
								]
								+ SHorizontalBox::Slot()
								.AutoWidth()
								.VAlign(VAlign_Center)
								.Padding(0.0f, 0.0f, 8.0f, 0.0f)
								[
									SNew(SBorder)
									.Padding(FMargin(10.0f, 4.0f))
									.BorderImage(FAppStyle::GetBrush("Brushes.Recessed"))
									.BorderBackgroundColor(this, &SMCPControlPanel::GetPortBadgeColor)
									[
										SNew(STextBlock)
										.Text(this, &SMCPControlPanel::GetPortBadgeText)
										.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
									]
								]
								+ SHorizontalBox::Slot()
								.AutoWidth()
								.VAlign(VAlign_Center)
								[
									SNew(SButton)
									.Text(LOCTEXT("Recheck", "Recheck"))
									.IsEnabled_Lambda([this]() { return !bRunning && !bInstallRunning; })
									.OnClicked(this, &SMCPControlPanel::OnCheckPortClicked)
								]
							]
							+ SVerticalBox::Slot()
							.AutoHeight()
							.Padding(0.0f, 6.0f, 0.0f, 0.0f)
							[
								SNew(STextBlock)
								.Text(this, &SMCPControlPanel::GetPortHelpText)
								.ColorAndOpacity(FSlateColor(MutedColor))
								.AutoWrapText(true)
							]
						]
					)
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SExpandableArea)
					.InitiallyCollapsed(true)
					.HeaderContent()
					[
						SNew(STextBlock)
						.Text(LOCTEXT("RuntimeDetails", "Runtime details"))
						.Font(FCoreStyle::GetDefaultFontStyle("Bold", 12))
					]
					.BodyContent()
					[
						SNew(SBox)
						.MinDesiredHeight(150.0f)
						[
							SNew(SMultiLineEditableTextBox)
							.IsReadOnly(true)
							.AutoWrapText(true)
							.Text(this, &SMCPControlPanel::GetInstallLogText)
						]
					]
				]
			]
		]
	];

	RefreshStatus();
	RegisterActiveTimer(1.0f, FWidgetActiveTimerDelegate::CreateSP(this, &SMCPControlPanel::RefreshTick));
}

EActiveTimerReturnType SMCPControlPanel::RefreshTick(double InCurrentTime, float InDeltaTime)
{
	RefreshStatus();
	return EActiveTimerReturnType::Continue;
}

void SMCPControlPanel::RefreshStatus()
{
	TSharedPtr<FJsonObject> Response;
	if (CallController(TEXT("get_status_json()"), Response))
	{
		ApplyResponse(Response);
	}
}

bool SMCPControlPanel::CallController(const FString& FunctionCall, TSharedPtr<FJsonObject>& OutObject)
{
	const FString Json = FUnrealSceneAssemblyModule::ExecutePythonControllerCommand(FunctionCall);
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	if (!FJsonSerializer::Deserialize(Reader, OutObject) || !OutObject.IsValid())
	{
		AppendMessage(FString::Printf(TEXT("Invalid controller response: %s"), *Json));
		return false;
	}

	if (!GetBoolField(OutObject, TEXT("ok"), false))
	{
		AppendMessage(GetStringField(OutObject, TEXT("error"), GetStringField(OutObject, TEXT("message"), TEXT("Controller command failed."))));
	}
	return true;
}

void SMCPControlPanel::ApplyResponse(const TSharedPtr<FJsonObject>& Response)
{
	bRunning = GetBoolField(Response, TEXT("running"), bRunning);

	const FString Message = GetStringField(Response, TEXT("message"));
	if (!Message.IsEmpty())
	{
		LastMessage = Message;
	}

	const TSharedPtr<FJsonObject> Config = GetObjectField(Response, TEXT("config"));
	if (Config.IsValid())
	{
		ConfigPort = GetIntField(Config, TEXT("mcp_port"), ConfigPort);
		EndpointUrl = GetStringField(Config, TEXT("mcp_url"), EndpointUrl);
		if (!bUserEditedPort && PortTextBox.IsValid())
		{
			TGuardValue<bool> Guard(bUpdatingPortText, true);
			PortTextBox->SetText(PortToText(ConfigPort));
		}
	}

	const TSharedPtr<FJsonObject> Dependencies = GetObjectField(Response, TEXT("dependencies"));
	if (Dependencies.IsValid())
	{
		bDependenciesInstalled = GetBoolField(Dependencies, TEXT("installed"), bDependenciesInstalled);
		EnvironmentMessage = bDependenciesInstalled ? TEXT("Ready") : TEXT("Missing packages");
	}
	else if (Response->HasField(TEXT("installed")))
	{
		bDependenciesInstalled = GetBoolField(Response, TEXT("installed"), bDependenciesInstalled);
		EnvironmentMessage = bDependenciesInstalled ? TEXT("Ready") : TEXT("Missing packages");
	}

	const TSharedPtr<FJsonObject> Install = GetObjectField(Response, TEXT("install"));
	if (Install.IsValid())
	{
		bInstallRunning = GetBoolField(Install, TEXT("running"), bInstallRunning);
		const FString InstallMessage = GetStringField(Install, TEXT("message"));
		TArray<FString> Lines;
		const TArray<TSharedPtr<FJsonValue>>* JsonLines = nullptr;
		if (Install->TryGetArrayField(TEXT("lines"), JsonLines))
		{
			for (const TSharedPtr<FJsonValue>& Value : *JsonLines)
			{
				FString Line;
				if (Value.IsValid() && Value->TryGetString(Line))
				{
					Lines.Add(Line);
				}
			}
		}
		InstallLog = InstallMessage.IsEmpty() ? TEXT("No runtime messages.") : InstallMessage;
		if (Lines.Num() > 0)
		{
			InstallLog += TEXT("\n") + FString::Join(Lines, TEXT("\n"));
		}
	}

	ApplyPortResponse(Response, true);
}

void SMCPControlPanel::ApplyPortResponse(const TSharedPtr<FJsonObject>& Response, bool bForceApply)
{
	const TSharedPtr<FJsonObject> Port = GetObjectField(Response, TEXT("port"));
	if (!Port.IsValid())
	{
		return;
	}

	const int32 ResponsePort = GetIntField(Port, TEXT("port"), ConfigPort);
	const bool bApplyToCurrentText = bForceApply || !bUserEditedPort || ResponsePort == GetPortValue();
	if (!bApplyToCurrentText)
	{
		return;
	}

	LastCheckedPort = ResponsePort;
	bPortAvailable = GetBoolField(Port, TEXT("available"), bPortAvailable);
	PortMessage = GetStringField(Port, TEXT("message"), bPortAvailable ? TEXT("Available") : TEXT("Unavailable"));
}

void SMCPControlPanel::AppendMessage(const FString& Message)
{
	LastMessage = Message;
	if (InstallLog.IsEmpty() || InstallLog == TEXT("No runtime messages."))
	{
		InstallLog = Message;
	}
	else
	{
		InstallLog += TEXT("\n") + Message;
	}
}

bool SMCPControlPanel::CheckCurrentPort(bool bAppendToLog)
{
	const int32 Port = GetPortValue();
	TSharedPtr<FJsonObject> Response;
	if (!CallController(FString::Printf(TEXT("check_port_json(%d)"), Port), Response))
	{
		return false;
	}

	ApplyPortResponse(Response, true);
	if (bAppendToLog)
	{
		AppendMessage(FString::Printf(TEXT("Port %d: %s"), Port, *PortMessage));
	}
	return bPortAvailable;
}

FString SMCPControlPanel::GetCurrentEndpointUrl() const
{
	const int32 DisplayPort = bRunning ? ConfigPort : GetPortValue();
	return bRunning ? EndpointUrl : FString::Printf(TEXT("http://127.0.0.1:%d/mcp"), DisplayPort);
}

FReply SMCPControlPanel::OnRefreshClicked()
{
	RefreshStatus();
	return FReply::Handled();
}

FReply SMCPControlPanel::OnInstallDependenciesClicked()
{
	const FText Message = LOCTEXT("InstallPrompt", "Install MCP Python dependencies into the plugin-local site-packages directory? This may download packages using pip.");
	if (FMessageDialog::Open(EAppMsgType::YesNo, Message) != EAppReturnType::Yes)
	{
		return FReply::Handled();
	}

	TSharedPtr<FJsonObject> Response;
	if (CallController(TEXT("install_dependencies_json()"), Response))
	{
		ApplyResponse(Response);
	}
	return FReply::Handled();
}

FReply SMCPControlPanel::OnCheckPortClicked()
{
	CheckCurrentPort(true);
	return FReply::Handled();
}

FReply SMCPControlPanel::OnPrimaryButtonClicked()
{
	TSharedPtr<FJsonObject> Response;
	if (bRunning)
	{
		if (CallController(TEXT("stop_json()"), Response))
		{
			ApplyResponse(Response);
			RefreshStatus();
		}
		return FReply::Handled();
	}

	const int32 Port = GetPortValue();
	if (LastCheckedPort != Port || !bPortAvailable)
	{
		CheckCurrentPort(false);
	}

	if (CallController(FString::Printf(TEXT("start_json(%d)"), Port), Response))
	{
		bUserEditedPort = false;
		ApplyResponse(Response);
		RefreshStatus();
	}
	return FReply::Handled();
}

FReply SMCPControlPanel::OnCopyUrlClicked()
{
	FPlatformApplicationMisc::ClipboardCopy(*GetCurrentEndpointUrl());
	AppendMessage(TEXT("Agent endpoint copied to clipboard. Remember to keep .opencode/opencode.json in sync if you changed the port."));
	return FReply::Handled();
}

void SMCPControlPanel::OnPortTextChanged(const FText& NewText)
{
	if (bUpdatingPortText)
	{
		return;
	}

	bUserEditedPort = true;
	const int32 Port = GetPortValue();
	if (Port != LastCheckedPort)
	{
		bPortAvailable = false;
		PortMessage = (Port >= 1 && Port <= 65535) ? TEXT("Press Enter or click Recheck") : TEXT("Enter a port from 1 to 65535");
	}
}

void SMCPControlPanel::OnPortTextCommitted(const FText& NewText, ETextCommit::Type CommitType)
{
	if (!bRunning && !bInstallRunning)
	{
		CheckCurrentPort(false);
	}
}

bool SMCPControlPanel::CanInstallDependencies() const
{
	return !bDependenciesInstalled && !bInstallRunning && !bRunning;
}

bool SMCPControlPanel::CanToggleServer() const
{
	return bRunning || CanStart();
}

bool SMCPControlPanel::CanStart() const
{
	const int32 Port = GetPortValue();
	return !bRunning && !bInstallRunning && bDependenciesInstalled && bPortAvailable && Port >= 1 && Port <= 65535 && LastCheckedPort == Port;
}

bool SMCPControlPanel::CanStop() const
{
	return bRunning;
}

int32 SMCPControlPanel::GetPortValue() const
{
	if (!PortTextBox.IsValid())
	{
		return ConfigPort;
	}

	int32 Port = ConfigPort;
	if (!FDefaultValueHelper::ParseInt(PortTextBox->GetText().ToString(), Port))
	{
		return 0;
	}
	return Port;
}

FText SMCPControlPanel::GetStatusPillText() const
{
	if (bRunning)
	{
		return LOCTEXT("RunningPill", "RUNNING");
	}
	if (bInstallRunning)
	{
		return LOCTEXT("InstallingPill", "INSTALLING");
	}
	return LOCTEXT("StoppedPill", "STOPPED");
}

FSlateColor SMCPControlPanel::GetStatusPillColor() const
{
	if (bRunning)
	{
		return FSlateColor(RunningColor);
	}
	if (bInstallRunning)
	{
		return FSlateColor(WarningColor);
	}
	return FSlateColor(MutedColor);
}

FText SMCPControlPanel::GetStatusSummaryText() const
{
	if (bRunning)
	{
		return FText::FromString(FString::Printf(TEXT("Serving local MCP tools on port %d."), ConfigPort));
	}
	if (!bDependenciesInstalled)
	{
		return LOCTEXT("InstallNeededSummary", "Install the Python runtime before starting MCP.");
	}
	if (!bPortAvailable)
	{
		return FText::FromString(FString::Printf(TEXT("Port %d needs attention before startup."), GetPortValue()));
	}
	return FText::GetEmpty();
}

FText SMCPControlPanel::GetPrimaryButtonText() const
{
	return bRunning ? LOCTEXT("StopMCP", "Stop MCP") : LOCTEXT("StartMCP", "Start MCP");
}

FText SMCPControlPanel::GetPrimaryButtonTooltip() const
{
	if (bRunning)
	{
		return LOCTEXT("StopTooltip", "Stop the bridge and external MCP server process.");
	}
	if (!bDependenciesInstalled)
	{
		return LOCTEXT("StartNeedsDeps", "Install the Python runtime first.");
	}
	if (!bPortAvailable)
	{
		return LOCTEXT("StartNeedsPort", "Choose an available port first.");
	}
	return LOCTEXT("StartTooltip", "Start the bridge and external MCP server process.");
}

FSlateColor SMCPControlPanel::GetPrimaryButtonColor() const
{
	return FSlateColor(bRunning ? ErrorColor : ReadyColor);
}

FText SMCPControlPanel::GetEndpointUrlText() const
{
	return FText::FromString(GetCurrentEndpointUrl());
}

FText SMCPControlPanel::GetDependencyStatusText() const
{
	if (bInstallRunning)
	{
		return LOCTEXT("DependenciesInstalling", "Installing dependencies...");
	}
	return bDependenciesInstalled ? LOCTEXT("DependenciesReady", "Ready") : LOCTEXT("DependenciesMissing", "Missing packages");
}

FSlateColor SMCPControlPanel::GetDependencyColor() const
{
	if (bInstallRunning)
	{
		return FSlateColor(WarningColor);
	}
	return FSlateColor(bDependenciesInstalled ? ReadyColor : ErrorColor);
}

FText SMCPControlPanel::GetInstallButtonText() const
{
	return bInstallRunning ? LOCTEXT("InstallingButton", "Installing...") : LOCTEXT("InstallButton", "Install Runtime");
}

EVisibility SMCPControlPanel::GetInstallButtonVisibility() const
{
	return bDependenciesInstalled ? EVisibility::Collapsed : EVisibility::Visible;
}

FText SMCPControlPanel::GetPortBadgeText() const
{
	if (bRunning)
	{
		return LOCTEXT("PortInUse", "IN USE");
	}
	const int32 Port = GetPortValue();
	if (Port < 1 || Port > 65535)
	{
		return LOCTEXT("PortInvalid", "INVALID");
	}
	return bPortAvailable ? LOCTEXT("PortAvailable", "AVAILABLE") : LOCTEXT("PortBlocked", "CHECK");
}

FSlateColor SMCPControlPanel::GetPortBadgeColor() const
{
	if (bRunning || bPortAvailable)
	{
		return FSlateColor(ReadyColor);
	}
	const int32 Port = GetPortValue();
	return FSlateColor((Port >= 1 && Port <= 65535) ? WarningColor : ErrorColor);
}

FText SMCPControlPanel::GetPortHelpText() const
{
	if (bRunning)
	{
		return FText::FromString(FString::Printf(TEXT("Port %d is currently used by this MCP server."), ConfigPort));
	}
	if (bPortAvailable)
	{
		return FText::FromString(FString::Printf(TEXT("Port %d is available. Update .opencode/opencode.json if you change it."), GetPortValue()));
	}
	return FText::FromString(FString::Printf(TEXT("Port %d: %s"), GetPortValue(), *PortMessage));
}

FText SMCPControlPanel::GetInstallLogText() const
{
	return FText::FromString(InstallLog);
}

#undef LOCTEXT_NAMESPACE

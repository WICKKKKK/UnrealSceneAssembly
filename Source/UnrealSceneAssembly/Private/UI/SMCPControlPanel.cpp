#include "UI/SMCPControlPanel.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
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
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SMCPControlPanel"

namespace
{
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
}

void SMCPControlPanel::Construct(const FArguments& InArgs)
{
	ChildSlot
	[
		SNew(SBorder)
		.Padding(16.0f)
		.BorderImage(FAppStyle::GetBrush("Brushes.Panel"))
		[
			SNew(SScrollBox)
			+ SScrollBox::Slot()
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(STextBlock)
					.Text(LOCTEXT("Title", "Scene Assembly MCP"))
					.Font(FCoreStyle::GetDefaultFontStyle("Bold", 20))
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 4.0f, 0.0f, 12.0f)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("Subtitle", "Manage the local MCP bridge and FastMCP server for the currently open Unreal project."))
					.AutoWrapText(true)
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 0.0f, 0.0f, 12.0f)
				[
					SNew(SBorder)
					.Padding(10.0f)
					.BorderImage(FAppStyle::GetBrush("Brushes.Recessed"))
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot()
						.AutoHeight()
						[
							SNew(STextBlock)
							.Text(this, &SMCPControlPanel::GetStatusText)
							.AutoWrapText(true)
						]
						+ SVerticalBox::Slot()
						.AutoHeight()
						.Padding(0.0f, 8.0f, 0.0f, 0.0f)
						[
							SNew(STextBlock)
							.Text(this, &SMCPControlPanel::GetAgentUrlText)
							.AutoWrapText(true)
						]
					]
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 0.0f, 0.0f, 8.0f)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("EnvironmentHeader", "Python Environment"))
					.Font(FCoreStyle::GetDefaultFontStyle("Bold", 14))
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(STextBlock)
					.Text(this, &SMCPControlPanel::GetEnvironmentText)
					.AutoWrapText(true)
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 8.0f, 0.0f, 16.0f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(0.0f, 0.0f, 8.0f, 0.0f)
					[
						SNew(SButton)
						.Text(LOCTEXT("CheckEnv", "Check Environment"))
						.OnClicked(this, &SMCPControlPanel::OnCheckEnvironmentClicked)
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(SButton)
						.Text(LOCTEXT("InstallDeps", "Install Dependencies"))
						.IsEnabled(this, &SMCPControlPanel::CanInstallDependencies)
						.OnClicked(this, &SMCPControlPanel::OnInstallDependenciesClicked)
					]
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SSeparator)
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 16.0f, 0.0f, 8.0f)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("PortHeader", "MCP HTTP Port"))
					.Font(FCoreStyle::GetDefaultFontStyle("Bold", 14))
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					.Padding(0.0f, 0.0f, 8.0f, 0.0f)
					[
						SAssignNew(PortTextBox, SEditableTextBox)
						.MinDesiredWidth(120.0f)
						.Text(FText::FromString(FString::FromInt(ConfigPort)))
						.IsReadOnly_Lambda([this]() { return bRunning; })
						.OnTextChanged(this, &SMCPControlPanel::OnPortTextChanged)
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					[
						SNew(SButton)
						.Text(LOCTEXT("CheckPort", "Check Port"))
						.IsEnabled_Lambda([this]() { return !bRunning && !bInstallRunning; })
						.OnClicked(this, &SMCPControlPanel::OnCheckPortClicked)
					]
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 8.0f, 0.0f, 16.0f)
				[
					SNew(STextBlock)
					.Text(this, &SMCPControlPanel::GetPortStatusText)
					.AutoWrapText(true)
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SSeparator)
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 16.0f, 0.0f, 8.0f)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("ControlsHeader", "Controls"))
					.Font(FCoreStyle::GetDefaultFontStyle("Bold", 14))
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(0.0f, 0.0f, 8.0f, 0.0f)
					[
						SNew(SButton)
						.Text(LOCTEXT("Start", "Start MCP"))
						.IsEnabled(this, &SMCPControlPanel::CanStart)
						.OnClicked(this, &SMCPControlPanel::OnStartClicked)
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(0.0f, 0.0f, 8.0f, 0.0f)
					[
						SNew(SButton)
						.Text(LOCTEXT("Stop", "Stop MCP"))
						.IsEnabled(this, &SMCPControlPanel::CanStop)
						.OnClicked(this, &SMCPControlPanel::OnStopClicked)
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(SButton)
						.Text(LOCTEXT("Refresh", "Refresh"))
						.OnClicked(this, &SMCPControlPanel::OnRefreshClicked)
					]
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 12.0f, 0.0f, 6.0f)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("InstallLogHeader", "Install / Runtime Log"))
					.Font(FCoreStyle::GetDefaultFontStyle("Bold", 14))
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SBox)
					.MinDesiredWidth(600.0f)
					.MinDesiredHeight(160.0f)
					[
						SNew(SMultiLineEditableTextBox)
						.IsReadOnly(true)
						.AutoWrapText(true)
						.Text(this, &SMCPControlPanel::GetInstallLogText)
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
			PortTextBox->SetText(FText::FromString(FString::FromInt(ConfigPort)));
		}
	}

	const TSharedPtr<FJsonObject> Dependencies = GetObjectField(Response, TEXT("dependencies"));
	if (Dependencies.IsValid())
	{
		bDependenciesInstalled = GetBoolField(Dependencies, TEXT("installed"), bDependenciesInstalled);
		EnvironmentMessage = bDependenciesInstalled ? TEXT("Dependencies are installed.") : TEXT("Dependencies are missing. Use Install Dependencies before starting MCP.");
	}
	else if (Response->HasField(TEXT("installed")))
	{
		bDependenciesInstalled = GetBoolField(Response, TEXT("installed"), bDependenciesInstalled);
		EnvironmentMessage = bDependenciesInstalled ? TEXT("Dependencies are installed.") : TEXT("Dependencies are missing. Use Install Dependencies before starting MCP.");
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
		InstallLog = InstallMessage;
		if (Lines.Num() > 0)
		{
			InstallLog += TEXT("\n") + FString::Join(Lines, TEXT("\n"));
		}
	}

	ApplyPortResponse(Response, false);
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
	PortMessage = GetStringField(Port, TEXT("message"), bPortAvailable ? TEXT("Port is available.") : TEXT("Port is not available."));
}

void SMCPControlPanel::AppendMessage(const FString& Message)
{
	LastMessage = Message;
	if (InstallLog.IsEmpty())
	{
		InstallLog = Message;
	}
	else
	{
		InstallLog += TEXT("\n") + Message;
	}
}

FReply SMCPControlPanel::OnRefreshClicked()
{
	RefreshStatus();
	return FReply::Handled();
}

FReply SMCPControlPanel::OnCheckEnvironmentClicked()
{
	TSharedPtr<FJsonObject> Response;
	if (CallController(TEXT("check_environment_json()"), Response))
	{
		ApplyResponse(Response);
		AppendMessage(EnvironmentMessage);
	}
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
	const int32 Port = GetPortValue();
	TSharedPtr<FJsonObject> Response;
	if (CallController(FString::Printf(TEXT("check_port_json(%d)"), Port), Response))
	{
		ApplyPortResponse(Response, true);
		AppendMessage(FString::Printf(TEXT("Port %d: %s"), Port, *PortMessage));
	}
	return FReply::Handled();
}

FReply SMCPControlPanel::OnStartClicked()
{
	const int32 Port = GetPortValue();
	TSharedPtr<FJsonObject> Response;
	if (CallController(FString::Printf(TEXT("start_json(%d)"), Port), Response))
	{
		bUserEditedPort = false;
		ApplyResponse(Response);
		RefreshStatus();
	}
	return FReply::Handled();
}

FReply SMCPControlPanel::OnStopClicked()
{
	TSharedPtr<FJsonObject> Response;
	if (CallController(TEXT("stop_json()"), Response))
	{
		ApplyResponse(Response);
		RefreshStatus();
	}
	return FReply::Handled();
}

void SMCPControlPanel::OnPortTextChanged(const FText& NewText)
{
	bUserEditedPort = true;
	const int32 Port = GetPortValue();
	if (Port != LastCheckedPort)
	{
		bPortAvailable = false;
		PortMessage = TEXT("Click Check Port before starting MCP.");
	}
}

bool SMCPControlPanel::CanInstallDependencies() const
{
	return !bDependenciesInstalled && !bInstallRunning && !bRunning;
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

FText SMCPControlPanel::GetStatusText() const
{
	return FText::FromString(FString::Printf(TEXT("Status: %s\n%s"), bRunning ? TEXT("Running") : TEXT("Stopped"), *LastMessage));
}

FText SMCPControlPanel::GetEnvironmentText() const
{
	return FText::FromString(FString::Printf(TEXT("%s%s"), bInstallRunning ? TEXT("Installing dependencies...\n") : TEXT(""), *EnvironmentMessage));
}

FText SMCPControlPanel::GetPortStatusText() const
{
	if (bRunning)
	{
		return FText::FromString(FString::Printf(TEXT("Port %d is in use by the running MCP server."), ConfigPort));
	}
	return FText::FromString(FString::Printf(TEXT("Port %d: %s"), GetPortValue(), *PortMessage));
}

FText SMCPControlPanel::GetInstallLogText() const
{
	return FText::FromString(InstallLog);
}

FText SMCPControlPanel::GetAgentUrlText() const
{
	const int32 DisplayPort = bRunning ? ConfigPort : GetPortValue();
	const FString Url = bRunning ? EndpointUrl : FString::Printf(TEXT("http://127.0.0.1:%d/mcp"), DisplayPort);
	return FText::FromString(FString::Printf(TEXT("Agent URL: %s\nIf you change the port here, update .opencode/opencode.json manually to the same URL."), *Url));
}

#undef LOCTEXT_NAMESPACE

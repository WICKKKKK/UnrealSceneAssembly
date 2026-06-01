#include "UnrealSceneAssembly.h"

#include "Dom/JsonObject.h"
#include "Framework/Docking/TabManager.h"
#include "Interfaces/IPluginManager.h"
#include "PythonScriptTypes.h"
#include "IPythonScriptPlugin.h"
#include "Misc/Base64.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Styling/AppStyle.h"
#include "ToolMenus.h"
#include "UI/SMCPControlPanel.h"
#include "Widgets/Docking/SDockTab.h"

namespace
{
static const FName SceneAssemblyMCPTabName(TEXT("SceneAssemblyMCP"));
static bool bStoppingPythonMCPRuntime = false;

FString MakeJsonError(const FString& ErrorMessage)
{
	const TSharedRef<FJsonObject> JsonObject = MakeShared<FJsonObject>();
	JsonObject->SetBoolField(TEXT("ok"), false);
	JsonObject->SetStringField(TEXT("error"), ErrorMessage);

	FString Output;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Output);
	FJsonSerializer::Serialize(JsonObject, Writer);
	return Output;
}

FString MakePythonStringLiteral(const FString& Value)
{
	FString Escaped = Value;
	Escaped.ReplaceInline(TEXT("\\"), TEXT("\\\\"));
	Escaped.ReplaceInline(TEXT("'"), TEXT("\\'"));
	Escaped.ReplaceInline(TEXT("\r"), TEXT("\\r"));
	Escaped.ReplaceInline(TEXT("\n"), TEXT("\\n"));
	return FString::Printf(TEXT("'%s'"), *Escaped);
}

bool ExtractPythonReprString(const FString& Value, FString& OutString)
{
	if (Value.Len() < 2)
	{
		return false;
	}

	const TCHAR Quote = Value[0];
	if ((Quote != TEXT('\'') && Quote != TEXT('"')) || Value[Value.Len() - 1] != Quote)
	{
		return false;
	}

	OutString = Value.Mid(1, Value.Len() - 2);
	return true;
}

bool DecodeBase64Utf8(const FString& Encoded, FString& OutDecoded)
{
	TArray<uint8> Bytes;
	if (!FBase64::Decode(Encoded, Bytes))
	{
		return false;
	}

	Bytes.Add(0);
	OutDecoded = UTF8_TO_TCHAR(reinterpret_cast<const ANSICHAR*>(Bytes.GetData()));
	return true;
}

FString GetPluginPythonDirectory()
{
	const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("UnrealSceneAssembly"));
	if (Plugin.IsValid())
	{
		return FPaths::ConvertRelativePathToFull(FPaths::Combine(Plugin->GetContentDir(), TEXT("Python")));
	}

	return FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectPluginsDir(), TEXT("UnrealSceneAssembly/Content/Python")));
}
}

#define LOCTEXT_NAMESPACE "FUnrealSceneAssemblyModule"

void FUnrealSceneAssemblyModule::StartupModule()
{
	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(SceneAssemblyMCPTabName, FOnSpawnTab::CreateRaw(this, &FUnrealSceneAssemblyModule::SpawnMCPControlPanelTab))
		.SetDisplayName(LOCTEXT("SceneAssemblyMCPTabTitle", "Scene Assembly MCP"))
		.SetTooltipText(LOCTEXT("SceneAssemblyMCPTabTooltip", "Manage the UnrealSceneAssembly MCP server."))
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Settings"))
		.SetMenuType(ETabSpawnerMenuType::Hidden);

	UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FUnrealSceneAssemblyModule::RegisterMenus));
}

void FUnrealSceneAssemblyModule::ShutdownModule()
{
	StopPythonMCPRuntime();
	UToolMenus::UnRegisterStartupCallback(this);
	UToolMenus::UnregisterOwner(this);
	FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(SceneAssemblyMCPTabName);
}

FString FUnrealSceneAssemblyModule::ExecutePythonControllerCommand(const FString& FunctionCall)
{
	IPythonScriptPlugin* PythonPlugin = FModuleManager::GetModulePtr<IPythonScriptPlugin>(TEXT("PythonScriptPlugin"));
	if (PythonPlugin == nullptr || !PythonPlugin->IsPythonAvailable())
	{
		return MakeJsonError(TEXT("PythonScriptPlugin is not available."));
	}

	const FString PythonDirLiteral = MakePythonStringLiteral(GetPluginPythonDirectory());
	const FString ControllerExpression = FString::Printf(
		TEXT("((__import__('sys').path.insert(0, %s) if %s not in __import__('sys').path else None), __import__('mcp_bridge.controller', fromlist=['']).get_controller(%s), __import__('mcp_bridge.controller', fromlist=['']).%s)[2]"),
		*PythonDirLiteral,
		*PythonDirLiteral,
		*PythonDirLiteral,
		*FunctionCall);
	const FString Command = FString::Printf(TEXT("__import__('base64').b64encode((%s).encode('utf-8')).decode('ascii')"), *ControllerExpression);

	FPythonCommandEx PythonCommand;
	PythonCommand.Command = Command;
	PythonCommand.ExecutionMode = EPythonCommandExecutionMode::EvaluateStatement;
	PythonCommand.FileExecutionScope = EPythonFileExecutionScope::Public;

	if (!PythonPlugin->ExecPythonCommandEx(PythonCommand))
	{
		return MakeJsonError(PythonCommand.CommandResult.IsEmpty() ? TEXT("Python controller command failed.") : PythonCommand.CommandResult);
	}

	if (PythonCommand.CommandResult.IsEmpty())
	{
		return MakeJsonError(TEXT("Python controller returned an empty response."));
	}

	FString EncodedResult;
	if (!ExtractPythonReprString(PythonCommand.CommandResult, EncodedResult))
	{
		return MakeJsonError(FString::Printf(TEXT("Python controller returned an unexpected response: %s"), *PythonCommand.CommandResult));
	}

	FString JsonResult;
	if (!DecodeBase64Utf8(EncodedResult, JsonResult))
	{
		return MakeJsonError(TEXT("Failed to decode Python controller response."));
	}

	return JsonResult;
}

void FUnrealSceneAssemblyModule::StopPythonMCPRuntime()
{
	if (bStoppingPythonMCPRuntime)
	{
		return;
	}

	TGuardValue<bool> Guard(bStoppingPythonMCPRuntime, true);
	ExecutePythonControllerCommand(TEXT("stop_json()"));
}

void FUnrealSceneAssemblyModule::RegisterMenus()
{
	FToolMenuOwnerScoped OwnerScoped(this);

	if (UToolMenu* Menu = UToolMenus::Get()->ExtendMenu(TEXT("LevelEditor.MainMenu.Window")))
	{
		FToolMenuSection& Section = Menu->FindOrAddSection(TEXT("WindowLayout"));
		Section.AddMenuEntry(
			TEXT("SceneAssemblyMCP"),
			LOCTEXT("SceneAssemblyMCPMenuLabel", "Scene Assembly MCP"),
			LOCTEXT("SceneAssemblyMCPMenuTooltip", "Open the Scene Assembly MCP control panel."),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Settings"),
			FUIAction(FExecuteAction::CreateRaw(this, &FUnrealSceneAssemblyModule::OpenMCPControlPanel)));
	}

	if (UToolMenu* ToolbarMenu = UToolMenus::Get()->ExtendMenu(TEXT("LevelEditor.LevelEditorToolBar.User")))
	{
		FToolMenuSection& Section = ToolbarMenu->FindOrAddSection(TEXT("SceneAssembly"));
		Section.AddEntry(FToolMenuEntry::InitToolBarButton(
			TEXT("SceneAssemblyMCPToolbarButton"),
			FUIAction(FExecuteAction::CreateRaw(this, &FUnrealSceneAssemblyModule::OpenMCPControlPanel)),
			LOCTEXT("SceneAssemblyMCPToolbarLabel", "MCP"),
			LOCTEXT("SceneAssemblyMCPToolbarTooltip", "Open the Scene Assembly MCP control panel."),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Settings")));
	}
}

void FUnrealSceneAssemblyModule::OpenMCPControlPanel()
{
	FGlobalTabmanager::Get()->TryInvokeTab(SceneAssemblyMCPTabName);
}

TSharedRef<SDockTab> FUnrealSceneAssemblyModule::SpawnMCPControlPanelTab(const FSpawnTabArgs& SpawnTabArgs)
{
	return SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		[
			SNew(SMCPControlPanel)
		];
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FUnrealSceneAssemblyModule, UnrealSceneAssembly)

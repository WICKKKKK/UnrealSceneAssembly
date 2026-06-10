#include "BlockoutModule.h"

#include "BlockoutToolsInterface.h"

#define LOCTEXT_NAMESPACE "FBlockoutModule"

void FBlockoutModule::StartupModule()
{
	FBlockoutToolsInterface::Get().RegisterBlockoutInterface();
}

void FBlockoutModule::ShutdownModule()
{
	FBlockoutToolsInterface::Get().UnregisterBlockoutInterface();
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FBlockoutModule, Blockout);

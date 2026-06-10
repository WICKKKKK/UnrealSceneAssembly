#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class BLOCKOUT_API FBlockoutModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};

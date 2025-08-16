#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

#include "VrPiano554Delegates.h"

DECLARE_LOG_CATEGORY_EXTERN(LogVrPiano554, Log, All);

class VRPIANO554_API FVRPIANO554Module : public IModuleInterface
{
public:
    /** IModuleInterface implementation */
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;
};
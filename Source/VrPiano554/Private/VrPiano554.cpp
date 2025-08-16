#include "VrPiano554.h"
#include "Modules/ModuleManager.h"

DEFINE_LOG_CATEGORY(LogVrPiano554);

void FVRPIANO554Module::StartupModule()
{
    // This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
    UE_LOG(LogVrPiano554, Log, TEXT("VrPiano554 module has started."));
}

void FVRPIANO554Module::ShutdownModule()
{
    // This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
    // we call this function before unloading the module.
    UE_LOG(LogVrPiano554, Log, TEXT("VrPiano554 module has shut down."));
}

IMPLEMENT_PRIMARY_GAME_MODULE(FVRPIANO554Module, VrPiano554, "VrPiano554");
using UnrealBuildTool;
using System.Collections.Generic;

public class VrPiano554Target : TargetRules
{
    public VrPiano554Target(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Game;
        DefaultBuildSettings = BuildSettingsVersion.V5;
        IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_5; // Zaktualizuj do Unreal5_5
        ExtraModuleNames.Add("VrPiano554");
    }
}
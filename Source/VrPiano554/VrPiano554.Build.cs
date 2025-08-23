using UnrealBuildTool;
using System.IO;

public class VrPiano554 : ModuleRules
{
    public VrPiano554(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "HeadMountedDisplay", "XRBase", "Sockets", "Networking", "Json", "JsonUtilities", "UMG", "Slate", "SlateCore" });

        if (Target.bBuildEditor == true)
        {
            PublicDependencyModuleNames.Add("UnrealEd");
        }

        PrivateDependencyModuleNames.AddRange(new string[] {  });

        // PublicIncludePaths.Add(Path.Combine(ModuleDirectory, "ThirdParty/MidiFileLib/src"));
    }
}

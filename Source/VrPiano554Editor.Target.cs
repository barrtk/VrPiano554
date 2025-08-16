using UnrealBuildTool;
using System.Collections.Generic;

public class VrPiano554EditorTarget : TargetRules
{
	public VrPiano554EditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.V5;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_5;
		ExtraModuleNames.Add("VrPiano554");
	}
}
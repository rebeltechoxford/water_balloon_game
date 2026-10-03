using UnrealBuildTool;
using System.Collections.Generic;
public class ClankaEditorTarget : TargetRules
{
    public ClankaEditorTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Editor;
        DefaultBuildSettings = BuildSettingsVersion.V7;
        IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
        ExtraModuleNames.Add("Clanka");
    }
}

using UnrealBuildTool;
using System.Collections.Generic;
public class ClankaTarget : TargetRules
{
    public ClankaTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Game;
        DefaultBuildSettings = BuildSettingsVersion.V7;
        IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
        ExtraModuleNames.Add("Clanka");
    }
}

using UnrealBuildTool;

public class ReporterTarget : TargetRules
{
    public ReporterTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Game;
        DefaultBuildSettings = BuildSettingsVersion.Latest;
        ExtraModuleNames.Add("Reporter");
    }
}

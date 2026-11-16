using UnrealBuildTool;

public class ReporterEditorTarget : TargetRules
{
    public ReporterEditorTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Editor;
        DefaultBuildSettings = BuildSettingsVersion.Latest;
        ExtraModuleNames.Add("Reporter");
    }
}

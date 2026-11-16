using UnrealBuildTool;

public class Reporter : ModuleRules
{
    public Reporter(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine", "InputCore", "Paper2D", "Slate", "SlateCore" });
    }
}

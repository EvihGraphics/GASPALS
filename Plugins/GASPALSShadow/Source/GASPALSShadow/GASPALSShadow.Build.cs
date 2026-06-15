using UnrealBuildTool;

public class GASPALSShadow : ModuleRules
{
    public GASPALSShadow(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(
            new[]
            {
                "Core",
                "CoreUObject",
                "Engine",
                "NNE",
                "PhysicsCore",
                "RenderCore",
                "RHI"
            });

        PrivateDependencyModuleNames.AddRange(
            new[]
            {
                "ImageWrapper",
                "Json",
                "JsonUtilities"
            });
    }
}

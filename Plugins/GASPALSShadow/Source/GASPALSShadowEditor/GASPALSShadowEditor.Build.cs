using UnrealBuildTool;

public class GASPALSShadowEditor : ModuleRules
{
    public GASPALSShadowEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(
            new[]
            {
                "Core",
                "CoreUObject",
                "Engine",
                "GASPALSShadow"
            });

        PrivateDependencyModuleNames.AddRange(
            new[]
            {
                "AnimationCore",
                "AssetRegistry",
                "Json",
                "JsonUtilities",
                "MeshConversion",
                "MeshDescription",
                "PhysicsCore",
                "PhysicsUtilities",
                "SkeletalMeshDescription",
                "SkeletalMeshUtilitiesCommon",
                "UnrealEd"
            });
    }
}

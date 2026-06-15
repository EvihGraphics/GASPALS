#pragma once

#include "CoreMinimal.h"

struct FMimicKitSourceRigAssetBuildSummary
{
    FString PackageDirectory;
    FString AssetRoot;
    FString SkeletalMeshPath;
    FString SkeletonPath;
    FString PhysicsAssetPath;
    FString ReportFile;
    FString SpecFile;
    FString RuntimeContractFile;

    int32 BodyCount = 0;
    int32 BoneCount = 0;
    int32 DofSize = 0;
    int32 PhysicsBodyCount = 0;
    int32 PhysicsConstraintCount = 0;
    bool bUsdHasUsdSkel = false;
    bool bGeneratedFromMjcfPrimitiveSpec = false;
    bool bSkeletalMeshOk = false;
    bool bSkeletonOk = false;
    bool bPhysicsAssetOk = false;
    bool bSourceAssetsBuilt = false;

    FString Error;
};

class FGASPALSMimicKitSourceRigAssetBuilder
{
public:
    static bool BuildSourceRigAssets(
        const FString& PackageDirectory,
        const FString& AssetRoot,
        FMimicKitSourceRigAssetBuildSummary& OutSummary,
        FString& OutError);
};

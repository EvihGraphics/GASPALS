#if WITH_DEV_AUTOMATION_TESTS

#include "GASPALSMimicKitSourceRigAssetBuilder.h"

#include "Engine/SkeletalMesh.h"
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "Animation/Skeleton.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGASPALSMimicKitSourceRigAssetBuildTest,
    "GASPALSShadow.MimicKitSourceRigAssetBuild",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGASPALSMimicKitSourceRigAssetBuildTest::RunTest(const FString& Parameters)
{
    FString PackageDir;
    if (!FParse::Value(FCommandLine::Get(), TEXT("MimicKitPackageDir="), PackageDir))
    {
        AddInfo(TEXT("Skipping MimicKit source rig asset build; pass -MimicKitPackageDir=... to enable it."));
        return true;
    }

    FString AssetRoot;
    FParse::Value(FCommandLine::Get(), TEXT("MimicKitAssetOut="), AssetRoot);
    if (AssetRoot.IsEmpty())
    {
        AssetRoot = TEXT("/Game/MimicKit/SwordShield");
    }

    FString Error;
    FMimicKitSourceRigAssetBuildSummary Summary;
    const bool bBuilt = FGASPALSMimicKitSourceRigAssetBuilder::BuildSourceRigAssets(PackageDir, AssetRoot, Summary, Error);

    TestTrue(FString::Printf(TEXT("source rig asset build: %s"), *Error), bBuilt);
    TestTrue(TEXT("source assets built"), Summary.bSourceAssetsBuilt);
    TestTrue(TEXT("skeletal mesh ok"), Summary.bSkeletalMeshOk);
    TestTrue(TEXT("skeleton ok"), Summary.bSkeletonOk);
    TestTrue(TEXT("physics asset ok"), Summary.bPhysicsAssetOk);
    TestTrue(TEXT("body count includes MimicKit source rig"), Summary.BodyCount >= 17);
    TestEqual(TEXT("bone count"), Summary.BoneCount, 17);
    TestEqual(TEXT("dof size"), Summary.DofSize, 31);
    TestTrue(TEXT("physics body count"), Summary.PhysicsBodyCount > 0);
    TestTrue(TEXT("build report exists"), FPaths::FileExists(Summary.ReportFile));

    const FString MeshObjectPath = Summary.SkeletalMeshPath + TEXT(".SK_MimicKit_SwordShield");
    const FString SkeletonObjectPath = Summary.SkeletonPath + TEXT(".Skeleton_MimicKit_SwordShield");
    const FString PhysicsObjectPath = Summary.PhysicsAssetPath + TEXT(".PA_MimicKit_SwordShield");
    TestNotNull(TEXT("generated source SkeletalMesh loads"), LoadObject<USkeletalMesh>(nullptr, *MeshObjectPath));
    TestNotNull(TEXT("generated source Skeleton loads"), LoadObject<USkeleton>(nullptr, *SkeletonObjectPath));
    TestNotNull(TEXT("generated source PhysicsAsset loads"), LoadObject<UPhysicsAsset>(nullptr, *PhysicsObjectPath));
    return bBuilt;
}

#endif

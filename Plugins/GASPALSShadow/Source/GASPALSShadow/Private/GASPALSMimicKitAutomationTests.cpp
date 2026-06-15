#if WITH_DEV_AUTOMATION_TESTS

#include "GASPALSMimicKitPackageLoader.h"
#include "GASPALSMimicKitTraceReplay.h"
#include "GASPALSMimicKitVisualReplay.h"
#include "GASPALSMimicKitVisualReplayCapture.h"

#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"

namespace
{
struct FMimicKitExpectedBrain
{
    const TCHAR* BrainName;
    int32 ObservationDim;
    int32 ActionDim;
    int32 PolicyHz;
    int32 PhysicsHz;
};

const FMimicKitExpectedBrain ExpectedBrains[] = {
    {TEXT("WalkBrain"), 160, 31, 30, 240},
    {TEXT("TurnBrain"), 163, 31, 30, 240},
    {TEXT("StopBrain"), 163, 31, 30, 240},
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGASPALSMimicKitPackageLoadTest,
    "GASPALSShadow.MimicKitPackageLoad",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGASPALSMimicKitPackageLoadTest::RunTest(const FString& Parameters)
{
    for (const FMimicKitExpectedBrain& Expected : ExpectedBrains)
    {
        FString Error;
        FGASPALSMimicKitPackageSummary Summary;
        const bool bLoaded = FGASPALSMimicKitPackageLoader::LoadPackageByBrainName(Expected.BrainName, Summary, Error);

        TestTrue(FString::Printf(TEXT("%s package loads: %s"), Expected.BrainName, *Error), bLoaded);
        if (!bLoaded)
        {
            continue;
        }

        TestEqual(FString::Printf(TEXT("%s brain name"), Expected.BrainName), Summary.BrainName, FString(Expected.BrainName));
        TestEqual(FString::Printf(TEXT("%s obs dim"), Expected.BrainName), Summary.ObservationDim, Expected.ObservationDim);
        TestEqual(FString::Printf(TEXT("%s action dim"), Expected.BrainName), Summary.ActionDim, Expected.ActionDim);
        TestEqual(FString::Printf(TEXT("%s policy hz"), Expected.BrainName), Summary.PolicyHz, Expected.PolicyHz);
        TestEqual(FString::Printf(TEXT("%s physics hz"), Expected.BrainName), Summary.PhysicsHz, Expected.PhysicsHz);
        TestEqual(FString::Printf(TEXT("%s trace rows"), Expected.BrainName), Summary.TraceRows, 300);
        TestTrue(FString::Printf(TEXT("%s parity threshold passed"), Expected.BrainName), Summary.bTraceThresholdPassed);
        TestTrue(FString::Printf(TEXT("%s package ready"), Expected.BrainName), Summary.bPackageReady);
        TestTrue(FString::Printf(TEXT("%s normalization stats sanitized"), Expected.BrainName), Summary.bNormalizationStatsSanitized);
    }

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGASPALSMimicKitTraceReplayTest,
    "GASPALSShadow.AI4AnimationTraceReplay",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGASPALSMimicKitTraceReplayTest::RunTest(const FString& Parameters)
{
    for (const FMimicKitExpectedBrain& Expected : ExpectedBrains)
    {
        FString Error;
        FGASPALSMimicKitPackageSummary Package;
        if (!FGASPALSMimicKitPackageLoader::LoadPackageByBrainName(Expected.BrainName, Package, Error))
        {
            AddError(FString::Printf(TEXT("%s package failed before trace replay: %s"), Expected.BrainName, *Error));
            continue;
        }

        FGASPALSMimicKitTraceReplaySummary Replay;
        const bool bReplayed = FGASPALSMimicKitTraceReplay::ReplayTrace(Package, Replay, Error);

        TestTrue(FString::Printf(TEXT("%s trace replay: %s"), Expected.BrainName, *Error), bReplayed);
        TestEqual(FString::Printf(TEXT("%s replay rows"), Expected.BrainName), Replay.RowCount, 300);
        TestEqual(FString::Printf(TEXT("%s replay obs dim"), Expected.BrainName), Replay.ObservationDim, Expected.ObservationDim);
        TestEqual(FString::Printf(TEXT("%s replay action dim"), Expected.BrainName), Replay.ActionDim, Expected.ActionDim);
        TestTrue(FString::Printf(TEXT("%s rows finite"), Expected.BrainName), Replay.bAllRowsFinite);
        TestTrue(FString::Printf(TEXT("%s dimensions match"), Expected.BrainName), Replay.bDimensionsMatch);
        TestTrue(FString::Printf(TEXT("%s threshold"), Expected.BrainName), Replay.MaxAbsDiff <= Package.ParityThreshold);
    }

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGASPALSMimicKitVisualReplayLoadTest,
    "GASPALSShadow.MimicKitVisualReplayLoad",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGASPALSMimicKitVisualReplayLoadTest::RunTest(const FString& Parameters)
{
    for (const FMimicKitExpectedBrain& Expected : ExpectedBrains)
    {
        FString Error;
        FGASPALSMimicKitPackageSummary Package;
        if (!FGASPALSMimicKitPackageLoader::LoadPackageByBrainName(Expected.BrainName, Package, Error))
        {
            AddError(FString::Printf(TEXT("%s package failed before visual replay: %s"), Expected.BrainName, *Error));
            continue;
        }

        FGASPALSMimicKitVisualReplaySummary Replay;
        const bool bValidated = FGASPALSMimicKitVisualReplay::ValidateReplay(Package, Replay, Error);

        TestTrue(FString::Printf(TEXT("%s visual replay: %s"), Expected.BrainName, *Error), bValidated);
        TestEqual(FString::Printf(TEXT("%s visual rows"), Expected.BrainName), Replay.RowCount, 300);
        TestEqual(FString::Printf(TEXT("%s visual policy hz"), Expected.BrainName), Replay.PolicyHz, Expected.PolicyHz);
        TestEqual(FString::Printf(TEXT("%s visual physics hz"), Expected.BrainName), Replay.PhysicsHz, Expected.PhysicsHz);
        TestEqual(FString::Printf(TEXT("%s visual dof dim"), Expected.BrainName), Replay.DofPosDim, Package.DofSize);
        TestEqual(FString::Printf(TEXT("%s visual action dim"), Expected.BrainName), Replay.ActionDim, Expected.ActionDim);
        TestTrue(FString::Printf(TEXT("%s visual rows finite"), Expected.BrainName), Replay.bAllRowsFinite);
        TestTrue(FString::Printf(TEXT("%s visual dimensions match"), Expected.BrainName), Replay.bDimensionsMatch);
        TestTrue(FString::Printf(TEXT("%s visual frames monotonic"), Expected.BrainName), Replay.bFramesMonotonic);
    }

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGASPALSMimicKitVisualReplayCaptureTest,
    "GASPALSShadow.MimicKitVisualReplayCapture",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGASPALSMimicKitVisualReplayCaptureTest::RunTest(const FString& Parameters)
{
    FString PackageDir;
    FString CaptureOut;
    if (!FParse::Value(FCommandLine::Get(), TEXT("MimicKitPackageDir="), PackageDir)
        || !FParse::Value(FCommandLine::Get(), TEXT("MimicKitCaptureOut="), CaptureOut))
    {
        AddInfo(TEXT("Skipping MimicKit visual replay capture; pass -MimicKitPackageDir=... and -MimicKitCaptureOut=... to enable it."));
        return true;
    }

    int32 FrameStride = 5;
    int32 Width = 960;
    int32 Height = 540;
    FParse::Value(FCommandLine::Get(), TEXT("MimicKitCaptureStride="), FrameStride);
    FParse::Value(FCommandLine::Get(), TEXT("MimicKitCaptureWidth="), Width);
    FParse::Value(FCommandLine::Get(), TEXT("MimicKitCaptureHeight="), Height);

    FString Error;
    FGASPALSMimicKitPackageSummary Package;
    const bool bLoaded = FGASPALSMimicKitVisualReplayCapture::LoadVisualPackage(PackageDir, Package, Error);
    TestTrue(FString::Printf(TEXT("capture package loads: %s"), *Error), bLoaded);
    if (!bLoaded)
    {
        return false;
    }

    FGASPALSMimicKitVisualCaptureSummary Capture;
    const bool bCaptured = FGASPALSMimicKitVisualReplayCapture::CaptureDebugGeometry(
        Package,
        CaptureOut,
        FrameStride,
        Width,
        Height,
        Capture,
        Error);

    TestTrue(FString::Printf(TEXT("visual replay debug capture: %s"), *Error), bCaptured);
    TestTrue(TEXT("transient world created"), Capture.bTransientWorldCreated);
    TestEqual(TEXT("capture source rows"), Capture.SourceRows, 300);
    TestEqual(TEXT("capture frame stride"), Capture.FrameStride, FrameStride);
    TestTrue(TEXT("capture frames written"), Capture.CaptureFrames > 0);
    TestTrue(TEXT("capture meta exists"), FPaths::FileExists(Capture.MetaFile));
    TestTrue(
        TEXT("capture PNG frame exists"),
        FPaths::FileExists(FPaths::Combine(CaptureOut, TEXT("frames/frame_000000.png"))));
    return bCaptured;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGASPALSMimicKitSkeletalVisualReplayCaptureTest,
    "GASPALSShadow.MimicKitSkeletalVisualReplayCapture",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGASPALSMimicKitSkeletalVisualReplayCaptureTest::RunTest(const FString& Parameters)
{
    FString PackageDir;
    FString CaptureOut;
    if (!FParse::Value(FCommandLine::Get(), TEXT("MimicKitPackageDir="), PackageDir)
        || !FParse::Value(FCommandLine::Get(), TEXT("MimicKitCaptureOut="), CaptureOut))
    {
        AddInfo(TEXT("Skipping MimicKit skeletal visual replay capture; pass -MimicKitPackageDir=... and -MimicKitCaptureOut=... to enable it."));
        return true;
    }

    int32 FrameStride = 5;
    int32 Width = 960;
    int32 Height = 540;
    FParse::Value(FCommandLine::Get(), TEXT("MimicKitCaptureStride="), FrameStride);
    FParse::Value(FCommandLine::Get(), TEXT("MimicKitCaptureWidth="), Width);
    FParse::Value(FCommandLine::Get(), TEXT("MimicKitCaptureHeight="), Height);

    FString Error;
    FGASPALSMimicKitPackageSummary Package;
    const bool bLoaded = FGASPALSMimicKitVisualReplayCapture::LoadVisualPackage(PackageDir, Package, Error);
    TestTrue(FString::Printf(TEXT("skeletal capture package loads: %s"), *Error), bLoaded);
    if (!bLoaded)
    {
        return false;
    }

    FGASPALSMimicKitVisualCaptureSummary Capture;
    const bool bCaptured = FGASPALSMimicKitVisualReplayCapture::CaptureSkeletalReplay(
        Package,
        CaptureOut,
        FrameStride,
        Width,
        Height,
        Capture,
        Error);

    TestTrue(FString::Printf(TEXT("skeletal visual replay capture: %s"), *Error), bCaptured);
    TestTrue(TEXT("transient world created"), Capture.bTransientWorldCreated);
    TestEqual(TEXT("capture mode"), Capture.CaptureMode, FString(TEXT("skeletal_replay")));
    TestEqual(TEXT("capture source rows"), Capture.SourceRows, 300);
    TestEqual(TEXT("capture frame stride"), Capture.FrameStride, FrameStride);
    TestTrue(TEXT("dof_pos applied"), Capture.bDofPosApplied);
    TestTrue(TEXT("skeletal pose frames written"), Capture.SkeletalPoseFrames > 0);
    TestTrue(TEXT("bone map hash present"), !Capture.BoneMapHash.IsEmpty());
    TestTrue(TEXT("basis transform hash present"), !Capture.BasisTransformHash.IsEmpty());
    TestTrue(TEXT("capture frames written"), Capture.CaptureFrames > 0);
    TestTrue(TEXT("capture meta exists"), FPaths::FileExists(Capture.MetaFile));
    TestTrue(
        TEXT("skeletal capture PNG frame exists"),
        FPaths::FileExists(FPaths::Combine(CaptureOut, TEXT("frames/frame_000000.png"))));
    return bCaptured;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGASPALSMimicKitSourceCharacterReplayCaptureTest,
    "GASPALSShadow.MimicKitSourceCharacterReplayCapture",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGASPALSMimicKitSourceCharacterReplayCaptureTest::RunTest(const FString& Parameters)
{
    FString PackageDir;
    FString CaptureOut;
    if (!FParse::Value(FCommandLine::Get(), TEXT("MimicKitPackageDir="), PackageDir)
        || !FParse::Value(FCommandLine::Get(), TEXT("MimicKitCaptureOut="), CaptureOut))
    {
        AddInfo(TEXT("Skipping MimicKit source character replay capture; pass -MimicKitPackageDir=... and -MimicKitCaptureOut=... to enable it."));
        return true;
    }

    int32 FrameStride = 5;
    int32 Width = 960;
    int32 Height = 540;
    FParse::Value(FCommandLine::Get(), TEXT("MimicKitCaptureStride="), FrameStride);
    FParse::Value(FCommandLine::Get(), TEXT("MimicKitCaptureWidth="), Width);
    FParse::Value(FCommandLine::Get(), TEXT("MimicKitCaptureHeight="), Height);

    FString Error;
    FGASPALSMimicKitPackageSummary Package;
    const bool bLoaded = FGASPALSMimicKitVisualReplayCapture::LoadVisualPackage(PackageDir, Package, Error);
    TestTrue(FString::Printf(TEXT("source character capture package loads: %s"), *Error), bLoaded);
    if (!bLoaded)
    {
        return false;
    }

    FGASPALSMimicKitVisualCaptureSummary Capture;
    const bool bCaptured = FGASPALSMimicKitVisualReplayCapture::CaptureSourceCharacterReplay(
        Package,
        CaptureOut,
        FrameStride,
        Width,
        Height,
        Capture,
        Error);

    TestTrue(FString::Printf(TEXT("source character visual replay capture: %s"), *Error), bCaptured);
    TestEqual(TEXT("capture mode"), Capture.CaptureMode, FString(TEXT("source_character_skeletalmesh_replay")));
    TestEqual(TEXT("render source"), Capture.RenderSource, FString(TEXT("ue_scene_capture_render_target")));
    TestEqual(TEXT("driver component"), Capture.DriverComponent, FString(TEXT("UPoseableMeshComponent")));
    TestTrue(TEXT("loaded MimicKit source skeletal mesh"), Capture.bLoadedSkeletalMesh);
    TestTrue(TEXT("poseable component created"), Capture.bPoseableComponentCreated);
    TestTrue(TEXT("source asset imported to UE"), Capture.bMimicKitSourceAssetImportedToUE);
    TestEqual(TEXT("source target fallback renderer empty"), Capture.FallbackRenderer, FString());
    TestTrue(TEXT("source target mesh path"), Capture.TargetMesh.StartsWith(TEXT("/Game/MimicKit/SwordShield/")));
    TestEqual(TEXT("capture source rows"), Capture.SourceRows, 300);
    TestEqual(TEXT("capture frame stride"), Capture.FrameStride, FrameStride);
    TestTrue(TEXT("dof_pos applied"), Capture.bDofPosApplied);
    TestTrue(TEXT("source skeletal pose frames written"), Capture.SkeletalPoseFrames > 0);
    TestTrue(TEXT("capture frames written"), Capture.CaptureFrames > 0);
    TestTrue(TEXT("capture meta exists"), FPaths::FileExists(Capture.MetaFile));
    TestTrue(
        TEXT("source character capture PNG frame exists"),
        FPaths::FileExists(FPaths::Combine(CaptureOut, TEXT("frames/frame_000000.png"))));
    return bCaptured;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGASPALSMimicKitLivePolicyChaosClosureTest,
    "GASPALSShadow.MimicKitLivePolicyChaosClosure",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGASPALSMimicKitLivePolicyChaosClosureTest::RunTest(const FString& Parameters)
{
    FString PackageDir;
    FString RuntimeOut;
    if (!FParse::Value(FCommandLine::Get(), TEXT("MimicKitPackageDir="), PackageDir)
        || !FParse::Value(FCommandLine::Get(), TEXT("MimicKitRuntimeOut="), RuntimeOut))
    {
        AddInfo(TEXT("Skipping MimicKit live policy/Chaos closure; pass -MimicKitPackageDir=... and -MimicKitRuntimeOut=... to enable it."));
        return true;
    }

    FString SourceCaptureDir;
    FParse::Value(FCommandLine::Get(), TEXT("MimicKitSourceCaptureDir="), SourceCaptureDir);
    if (SourceCaptureDir.IsEmpty())
    {
        SourceCaptureDir = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("MimicKitVisualCaptures/StopBrain_long01_probe_source_character_closure"));
    }

    double RuntimeSeconds = 10.0;
    FParse::Value(FCommandLine::Get(), TEXT("MimicKitRuntimeSeconds="), RuntimeSeconds);

    FString Error;
    FGASPALSMimicKitPackageSummary Package;
    const bool bLoaded = FGASPALSMimicKitVisualReplayCapture::LoadVisualPackage(PackageDir, Package, Error);
    TestTrue(FString::Printf(TEXT("runtime closure package loads: %s"), *Error), bLoaded);
    if (!bLoaded)
    {
        return false;
    }

    FGASPALSMimicKitRuntimeClosureSummary Runtime;
    const bool bClosed = FGASPALSMimicKitVisualReplayCapture::RunLivePolicyChaosClosure(
        Package,
        RuntimeOut,
        SourceCaptureDir,
        RuntimeSeconds,
        Runtime,
        Error);

    TestTrue(FString::Printf(TEXT("live policy/Chaos closure: %s"), *Error), bClosed);
    TestEqual(TEXT("runtime backend"), Runtime.RuntimeBackend, FString(TEXT("NNERuntimeORT")));
    TestEqual(TEXT("runtime observation dim"), Runtime.ObservationDim, 163);
    TestEqual(TEXT("runtime action dim"), Runtime.ActionDim, 31);
    TestEqual(TEXT("runtime policy hz"), Runtime.PolicyHz, 30);
    TestEqual(TEXT("runtime physics hz"), Runtime.PhysicsHz, 240);
    TestEqual(TEXT("runtime observation source"), Runtime.ObservationSource, FString(TEXT("ue_physics_actor_state")));
    TestEqual(TEXT("runtime observation filled dim"), Runtime.ObservationFilledDim, 163);
    TestEqual(TEXT("runtime obs layout contract"), Runtime.ObsLayoutContract, FString(TEXT("compute_char_obs_plus_task_steering")));
    TestEqual(TEXT("runtime initial state source"), Runtime.InitialStateSource, FString(TEXT("visual_replay_frame0_only")));
    TestEqual(TEXT("runtime contact source"), Runtime.ContactMeasurementSource, FString(TEXT("chaos_contact_query")));
    TestEqual(TEXT("runtime contact query method"), Runtime.ContactQueryMethod, FString(TEXT("body_overlap_or_shape_sweep_against_ground")));
    TestTrue(TEXT("runtime joint drive backend present"), !Runtime.JointDriveBackend.IsEmpty());
    TestTrue(TEXT("onnx loaded from package"), Runtime.bOnnxLoadedFromPackage);
    TestTrue(TEXT("policy inference ran"), Runtime.bPolicyInferenceRan);
    TestFalse(TEXT("trace fallback not used"), Runtime.bTraceFallbackUsed);
    TestTrue(TEXT("physics simulated"), Runtime.bPhysicsSimulated);
    TestTrue(TEXT("joint drive applied"), Runtime.bJointDriveApplied);
    TestTrue(TEXT("physics substeps ran"), Runtime.PhysicsSubsteps >= 2400);
    TestTrue(TEXT("pre-policy settle ran"), Runtime.PrePolicySettleSubsteps > 0);
    TestTrue(TEXT("real contact events observed"), Runtime.ContactEvents > 0);
    TestTrue(TEXT("actions finite"), Runtime.bActionsFinite);
    TestTrue(TEXT("root finite"), Runtime.bRootFinite);
    TestTrue(TEXT("right foot body validated"), Runtime.bRightFootBodyValidated);
    TestTrue(TEXT("left foot body validated"), Runtime.bLeftFootBodyValidated);
    TestTrue(TEXT("ground alignment source present"), !Runtime.GroundAlignmentSource.IsEmpty());
    TestTrue(TEXT("foot contacts observed"), Runtime.bFootContactsObserved);
    TestTrue(TEXT("sliding under threshold"), Runtime.bSlidingUnderThreshold);
    TestTrue(TEXT("live policy control pass"), Runtime.bLivePolicyControlPass);
    TestTrue(TEXT("chaos contact validated"), Runtime.bChaosContactValidated);
    TestTrue(TEXT("runtime report exists"), FPaths::FileExists(Runtime.RuntimeReportFile));
    TestTrue(TEXT("combined closure report exists"), FPaths::FileExists(Runtime.CombinedReportFile));
    return bClosed;
}

#endif

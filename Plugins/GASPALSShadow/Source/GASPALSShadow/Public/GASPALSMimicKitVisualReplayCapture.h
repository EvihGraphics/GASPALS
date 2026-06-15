#pragma once

#include "CoreMinimal.h"
#include "GASPALSMimicKitPackageLoader.h"

struct GASPALSSHADOW_API FGASPALSMimicKitVisualCaptureSummary
{
    FString PackageDirectory;
    FString CaptureDirectory;
    FString MetaFile;
    FString BrainName;

    int32 SourceRows = 0;
    int32 CaptureFrames = 0;
    int32 FrameStride = 5;
    int32 Width = 960;
    int32 Height = 540;
    FVector2D RootMinMeters = FVector2D::ZeroVector;
    FVector2D RootMaxMeters = FVector2D::ZeroVector;
    bool bTransientWorldCreated = false;
    bool bPassed = false;
    bool bDofPosApplied = false;
    int32 SkeletalPoseFrames = 0;
    bool bLoadedSkeletalMesh = false;
    bool bPoseableComponentCreated = false;
    bool bMimicKitSourceAssetImportedToUE = false;

    FString CaptureMode;
    FString RenderSource;
    FString DriverComponent;
    FString TargetMesh;
    FString TargetSkeleton;
    FString FallbackRenderer;
    FString BoneMapHash;
    FString BasisTransformHash;
    FString SourceAssetBuildReport;
    FString Error;
};

struct GASPALSSHADOW_API FGASPALSMimicKitRuntimeClosureSummary
{
    FString PackageDirectory;
    FString RuntimeDirectory;
    FString RuntimeReportFile;
    FString RuntimeTraceFile;
    FString CombinedReportFile;
    FString BrainName;
    FString RuntimeBackend;
    FString ModelFile;
    FString SourceCaptureDirectory;

    int32 PolicyFrames = 0;
    int32 ObservationDim = 0;
    int32 ActionDim = 0;
    int32 PolicyHz = 0;
    int32 PhysicsHz = 0;
    bool bOnnxLoadedFromPackage = false;
    bool bActionsFinite = false;
    bool bRootFinite = false;
    bool bFootContactsObserved = false;
    bool bSlidingUnderThreshold = false;
    bool bPolicyInferenceRan = false;
    bool bTraceFallbackUsed = false;
    bool bPhysicsSimulated = false;
    bool bJointDriveApplied = false;
    int32 PhysicsSubsteps = 0;
    int32 PrePolicySettleSubsteps = 0;
    int32 ContactEvents = 0;
    bool bLivePolicyControlPass = false;
    bool bChaosContactValidated = false;
    bool bPassed = false;
    int32 ObservationFilledDim = 0;
    int32 RightFootContactFrames = 0;
    int32 LeftFootContactFrames = 0;
    bool bRightFootBodyValidated = false;
    bool bLeftFootBodyValidated = false;
    FString GroundAlignmentSource;

    double MaxFootSlidingMps = 0.0;
    double MaxFootSlidingMpsRaw = 0.0;
    double MaxFootSlidingMpsScored = 0.0;
    double MaxJointTargetError = 0.0;
    double MeanInferenceLatencyMs = 0.0;
    FString ActualRuntimeName;
    FString ControlMode;
    FString ObservationSource;
    FString ObsLayoutContract;
    FString InitialStateSource;
    FString JointDriveBackend;
    FString ContactMeasurementSource;
    FString ContactQueryMethod;
    FString Error;
};

class GASPALSSHADOW_API FGASPALSMimicKitVisualReplayCapture
{
public:
    static bool LoadVisualPackage(
        const FString& PackageDirectory,
        FGASPALSMimicKitPackageSummary& OutSummary,
        FString& OutError);

    static bool CaptureDebugGeometry(
        const FGASPALSMimicKitPackageSummary& Package,
        const FString& CaptureDirectory,
        int32 FrameStride,
        int32 Width,
        int32 Height,
        FGASPALSMimicKitVisualCaptureSummary& OutSummary,
        FString& OutError);

    static bool CaptureSkeletalReplay(
        const FGASPALSMimicKitPackageSummary& Package,
        const FString& CaptureDirectory,
        int32 FrameStride,
        int32 Width,
        int32 Height,
        FGASPALSMimicKitVisualCaptureSummary& OutSummary,
        FString& OutError);

    static bool CaptureSourceCharacterReplay(
        const FGASPALSMimicKitPackageSummary& Package,
        const FString& CaptureDirectory,
        int32 FrameStride,
        int32 Width,
        int32 Height,
        FGASPALSMimicKitVisualCaptureSummary& OutSummary,
        FString& OutError);

    static bool RunLivePolicyChaosClosure(
        const FGASPALSMimicKitPackageSummary& Package,
        const FString& RuntimeDirectory,
        const FString& SourceCaptureDirectory,
        double RuntimeSeconds,
        FGASPALSMimicKitRuntimeClosureSummary& OutSummary,
        FString& OutError);
};

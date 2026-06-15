#pragma once

#include "CoreMinimal.h"

class FJsonObject;

struct GASPALSSHADOW_API FGASPALSMimicKitPackageSummary
{
    FString PackageDirectory;
    FString BrainName;
    FString EnvName;
    FString ModelFileName;
    FString CoordinateBasis;
    FString UnitScale;
    FString ControlMode;

    int32 ObservationDim = 0;
    int32 ActionDim = 0;
    int32 PolicyHz = 0;
    int32 PhysicsHz = 0;
    int32 BodyCount = 0;
    int32 JointCount = 0;
    int32 DofSize = 0;

    int32 TraceRows = 0;
    int32 TraceObsDim = 0;
    int32 TraceActionDim = 0;
    int32 VisualReplayRows = 0;
    int32 VisualReplayDofDim = 0;
    double ParityMaxAbs = 0.0;
    double ParityMeanAbs = 0.0;
    double ParityThreshold = 0.0;
    bool bTraceThresholdPassed = false;
    bool bVisualReplayReady = false;
    bool bNormalizationStatsSanitized = false;
    bool bPackageReady = false;

    TArray<FString> MissingFiles;
    TArray<FString> Warnings;
};

class GASPALSSHADOW_API FGASPALSMimicKitPackageLoader
{
public:
    static FString GetDefaultPackageRoot();
    static FString GetDefaultPackageDirectory(const FString& BrainName);
    static bool LoadPackageByBrainName(const FString& BrainName, FGASPALSMimicKitPackageSummary& OutSummary, FString& OutError);
    static bool LoadPackage(const FString& PackageDirectory, FGASPALSMimicKitPackageSummary& OutSummary, FString& OutError);

private:
    static bool LoadJsonObjectFromFile(const FString& FilePath, TSharedPtr<FJsonObject>& OutObject, FString& OutError, bool bAllowNonFiniteTokens = false, bool* bOutSanitized = nullptr);
    static bool HasRequiredFiles(const FString& PackageDirectory, TArray<FString>& OutMissingFiles);
};

#pragma once

#include "CoreMinimal.h"
#include "GASPALSMimicKitPackageLoader.h"

struct GASPALSSHADOW_API FGASPALSMimicKitVisualReplaySummary
{
    FString PackageDirectory;
    FString MetaFile;
    FString ReplayFile;
    FString BrainName;

    int32 RowCount = 0;
    int32 ExpectedRows = 0;
    int32 PolicyHz = 0;
    int32 PhysicsHz = 0;
    int32 DofSize = 0;
    int32 DofPosDim = 0;
    int32 ActionDim = 0;
    bool bAllRowsFinite = true;
    bool bDimensionsMatch = true;
    bool bFramesMonotonic = true;
    bool bPassed = false;

    FString Error;
};

class GASPALSSHADOW_API FGASPALSMimicKitVisualReplay
{
public:
    static bool ValidateReplay(const FGASPALSMimicKitPackageSummary& Package, FGASPALSMimicKitVisualReplaySummary& OutSummary, FString& OutError);

private:
    static bool IsFiniteNumberArray(const TSharedPtr<FJsonObject>& Row, const FString& FieldName, int32 ExpectedDim);
};

#pragma once

#include "CoreMinimal.h"
#include "GASPALSMimicKitPackageLoader.h"

struct GASPALSSHADOW_API FGASPALSMimicKitTraceReplaySummary
{
    FString PackageDirectory;
    FString TraceFile;
    FString BrainName;

    int32 RowCount = 0;
    int32 ExpectedRows = 0;
    int32 ObservationDim = 0;
    int32 ActionDim = 0;
    double MaxAbsDiff = 0.0;
    bool bAllRowsFinite = true;
    bool bDimensionsMatch = true;
    bool bPassed = false;

    FString Error;
};

class GASPALSSHADOW_API FGASPALSMimicKitTraceReplay
{
public:
    static bool ReplayTrace(const FGASPALSMimicKitPackageSummary& Package, FGASPALSMimicKitTraceReplaySummary& OutSummary, FString& OutError);

private:
    static bool IsFiniteNumberArray(const TSharedPtr<FJsonObject>& Row, const FString& FieldName, int32 ExpectedDim);
};

#include "GASPALSMimicKitTraceReplay.h"

#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
FString TracePathForPackage(const FString& PackageDirectory)
{
    return FPaths::ConvertRelativePathToFull(FPaths::Combine(PackageDirectory, TEXT("ai4animation_trace/onnx_trace.jsonl")));
}

bool ParseJsonLine(const FString& Line, TSharedPtr<FJsonObject>& OutObject)
{
    const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Line);
    return FJsonSerializer::Deserialize(Reader, OutObject) && OutObject.IsValid();
}

double GetDoubleField(const TSharedPtr<FJsonObject>& Object, const FString& FieldName)
{
    double Value = 0.0;
    return Object.IsValid() && Object->TryGetNumberField(FieldName, Value) ? Value : 0.0;
}
}

bool FGASPALSMimicKitTraceReplay::ReplayTrace(const FGASPALSMimicKitPackageSummary& Package, FGASPALSMimicKitTraceReplaySummary& OutSummary, FString& OutError)
{
    OutSummary = FGASPALSMimicKitTraceReplaySummary();
    OutSummary.PackageDirectory = Package.PackageDirectory;
    OutSummary.TraceFile = TracePathForPackage(Package.PackageDirectory);
    OutSummary.BrainName = Package.BrainName;
    OutSummary.ExpectedRows = Package.TraceRows;
    OutSummary.ObservationDim = Package.ObservationDim;
    OutSummary.ActionDim = Package.ActionDim;

    FString RawTrace;
    if (!FFileHelper::LoadFileToString(RawTrace, *OutSummary.TraceFile))
    {
        OutError = FString::Printf(TEXT("Could not read trace file: %s"), *OutSummary.TraceFile);
        OutSummary.Error = OutError;
        return false;
    }

    TArray<FString> Lines;
    RawTrace.ParseIntoArrayLines(Lines, false);
    for (const FString& Line : Lines)
    {
        if (Line.TrimStartAndEnd().IsEmpty())
        {
            continue;
        }

        TSharedPtr<FJsonObject> Row;
        if (!ParseJsonLine(Line, Row))
        {
            OutSummary.bAllRowsFinite = false;
            OutSummary.bDimensionsMatch = false;
            OutError = FString::Printf(TEXT("Could not parse trace row %d."), OutSummary.RowCount);
            OutSummary.Error = OutError;
            return false;
        }

        const bool bObsOk = IsFiniteNumberArray(Row, TEXT("obs"), Package.ObservationDim);
        const bool bActionOk = IsFiniteNumberArray(Row, TEXT("action"), Package.ActionDim);
        const bool bRefActionOk = IsFiniteNumberArray(Row, TEXT("ref_action"), Package.ActionDim);
        const bool bRootPosOk = IsFiniteNumberArray(Row, TEXT("ref_root_pos"), 3);
        const bool bRootRotOk = IsFiniteNumberArray(Row, TEXT("ref_root_rot"), 4);

        OutSummary.bAllRowsFinite = OutSummary.bAllRowsFinite && bObsOk && bActionOk && bRefActionOk && bRootPosOk && bRootRotOk;
        OutSummary.bDimensionsMatch = OutSummary.bDimensionsMatch && bObsOk && bActionOk && bRefActionOk;
        OutSummary.MaxAbsDiff = FMath::Max(OutSummary.MaxAbsDiff, GetDoubleField(Row, TEXT("max_abs_diff_vs_torch_ref")));
        OutSummary.RowCount += 1;
    }

    OutSummary.bPassed =
        OutSummary.RowCount == Package.TraceRows
        && OutSummary.bAllRowsFinite
        && OutSummary.bDimensionsMatch
        && Package.bTraceThresholdPassed
        && OutSummary.MaxAbsDiff <= Package.ParityThreshold;

    if (!OutSummary.bPassed)
    {
        OutError = FString::Printf(
            TEXT("Trace replay failed: rows=%d expected=%d finite=%s dims=%s max_abs=%g threshold=%g"),
            OutSummary.RowCount,
            Package.TraceRows,
            OutSummary.bAllRowsFinite ? TEXT("true") : TEXT("false"),
            OutSummary.bDimensionsMatch ? TEXT("true") : TEXT("false"),
            OutSummary.MaxAbsDiff,
            Package.ParityThreshold);
        OutSummary.Error = OutError;
        return false;
    }

    OutError.Reset();
    return true;
}

bool FGASPALSMimicKitTraceReplay::IsFiniteNumberArray(const TSharedPtr<FJsonObject>& Row, const FString& FieldName, int32 ExpectedDim)
{
    const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
    if (!Row.IsValid() || !Row->TryGetArrayField(FieldName, Values) || Values->Num() != ExpectedDim)
    {
        return false;
    }

    for (const TSharedPtr<FJsonValue>& Value : *Values)
    {
        double Number = 0.0;
        if (!Value.IsValid() || !Value->TryGetNumber(Number) || !FMath::IsFinite(Number))
        {
            return false;
        }
    }
    return true;
}

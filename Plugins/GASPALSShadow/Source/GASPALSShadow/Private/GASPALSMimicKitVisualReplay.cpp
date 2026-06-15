#include "GASPALSMimicKitVisualReplay.h"

#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
FString VisualMetaPathForPackage(const FString& PackageDirectory)
{
    return FPaths::ConvertRelativePathToFull(FPaths::Combine(PackageDirectory, TEXT("visual_replay/pose_dof_meta.json")));
}

FString VisualReplayPathForPackage(const FString& PackageDirectory)
{
    return FPaths::ConvertRelativePathToFull(FPaths::Combine(PackageDirectory, TEXT("visual_replay/pose_dof_replay.jsonl")));
}

bool ParseJsonObject(const FString& Text, TSharedPtr<FJsonObject>& OutObject)
{
    const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
    return FJsonSerializer::Deserialize(Reader, OutObject) && OutObject.IsValid();
}

bool LoadJsonObject(const FString& FilePath, TSharedPtr<FJsonObject>& OutObject, FString& OutError)
{
    FString Text;
    if (!FFileHelper::LoadFileToString(Text, *FilePath))
    {
        OutError = FString::Printf(TEXT("Could not read JSON file: %s"), *FilePath);
        return false;
    }
    if (!ParseJsonObject(Text, OutObject))
    {
        OutError = FString::Printf(TEXT("Could not parse JSON file: %s"), *FilePath);
        return false;
    }
    return true;
}

int32 GetIntField(const TSharedPtr<FJsonObject>& Object, const FString& FieldName)
{
    double Value = 0.0;
    return Object.IsValid() && Object->TryGetNumberField(FieldName, Value) ? static_cast<int32>(Value) : 0;
}

bool ParseJsonLine(const FString& Line, TSharedPtr<FJsonObject>& OutObject)
{
    return ParseJsonObject(Line, OutObject);
}
}

bool FGASPALSMimicKitVisualReplay::ValidateReplay(
    const FGASPALSMimicKitPackageSummary& Package,
    FGASPALSMimicKitVisualReplaySummary& OutSummary,
    FString& OutError)
{
    OutSummary = FGASPALSMimicKitVisualReplaySummary();
    OutSummary.PackageDirectory = Package.PackageDirectory;
    OutSummary.MetaFile = VisualMetaPathForPackage(Package.PackageDirectory);
    OutSummary.ReplayFile = VisualReplayPathForPackage(Package.PackageDirectory);
    OutSummary.BrainName = Package.BrainName;

    TSharedPtr<FJsonObject> Meta;
    if (!LoadJsonObject(OutSummary.MetaFile, Meta, OutError))
    {
        OutSummary.Error = OutError;
        return false;
    }

    OutSummary.ExpectedRows = GetIntField(Meta, TEXT("row_count"));
    OutSummary.PolicyHz = GetIntField(Meta, TEXT("policy_hz"));
    OutSummary.PhysicsHz = GetIntField(Meta, TEXT("physics_hz"));
    OutSummary.DofSize = GetIntField(Meta, TEXT("dof_size"));
    OutSummary.DofPosDim = GetIntField(Meta, TEXT("dof_pos_dim"));
    OutSummary.ActionDim = GetIntField(Meta, TEXT("action_dim"));

    FString RawReplay;
    if (!FFileHelper::LoadFileToString(RawReplay, *OutSummary.ReplayFile))
    {
        OutError = FString::Printf(TEXT("Could not read visual replay file: %s"), *OutSummary.ReplayFile);
        OutSummary.Error = OutError;
        return false;
    }

    int32 PreviousFrame = -1;
    TArray<FString> Lines;
    RawReplay.ParseIntoArrayLines(Lines, false);
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
            OutError = FString::Printf(TEXT("Could not parse visual replay row %d."), OutSummary.RowCount);
            OutSummary.Error = OutError;
            return false;
        }

        const int32 Frame = GetIntField(Row, TEXT("frame"));
        OutSummary.bFramesMonotonic = OutSummary.bFramesMonotonic && Frame > PreviousFrame;
        PreviousFrame = Frame;

        const bool bRootPosOk = IsFiniteNumberArray(Row, TEXT("root_pos_m"), 3);
        const bool bRootRotOk = IsFiniteNumberArray(Row, TEXT("root_rot_xyzw"), 4);
        const bool bDofOk = IsFiniteNumberArray(Row, TEXT("dof_pos"), Package.DofSize);
        const bool bActionOk = IsFiniteNumberArray(Row, TEXT("policy_action"), Package.ActionDim);
        const bool bRootVelOk = !Row->HasField(TEXT("root_vel_mps")) || IsFiniteNumberArray(Row, TEXT("root_vel_mps"), 3);
        const bool bRootAngVelOk = !Row->HasField(TEXT("root_ang_vel_radps")) || IsFiniteNumberArray(Row, TEXT("root_ang_vel_radps"), 3);
        const bool bDofVelOk = !Row->HasField(TEXT("dof_vel")) || IsFiniteNumberArray(Row, TEXT("dof_vel"), Package.DofSize);

        OutSummary.bAllRowsFinite =
            OutSummary.bAllRowsFinite
            && bRootPosOk
            && bRootRotOk
            && bDofOk
            && bActionOk
            && bRootVelOk
            && bRootAngVelOk
            && bDofVelOk;
        OutSummary.bDimensionsMatch = OutSummary.bDimensionsMatch && bDofOk && bActionOk;
        OutSummary.RowCount += 1;
    }

    OutSummary.bPassed =
        OutSummary.RowCount == OutSummary.ExpectedRows
        && OutSummary.ExpectedRows > 0
        && OutSummary.PolicyHz == Package.PolicyHz
        && OutSummary.PhysicsHz == Package.PhysicsHz
        && OutSummary.DofSize == Package.DofSize
        && OutSummary.DofPosDim == Package.DofSize
        && OutSummary.ActionDim == Package.ActionDim
        && OutSummary.bAllRowsFinite
        && OutSummary.bDimensionsMatch
        && OutSummary.bFramesMonotonic;

    if (!OutSummary.bPassed)
    {
        OutError = FString::Printf(
            TEXT("Visual replay failed: rows=%d expected=%d finite=%s dims=%s frames=%s dof=%d/%d action=%d/%d hz=%d/%d"),
            OutSummary.RowCount,
            OutSummary.ExpectedRows,
            OutSummary.bAllRowsFinite ? TEXT("true") : TEXT("false"),
            OutSummary.bDimensionsMatch ? TEXT("true") : TEXT("false"),
            OutSummary.bFramesMonotonic ? TEXT("true") : TEXT("false"),
            OutSummary.DofPosDim,
            Package.DofSize,
            OutSummary.ActionDim,
            Package.ActionDim,
            OutSummary.PolicyHz,
            Package.PolicyHz);
        OutSummary.Error = OutError;
        return false;
    }

    OutError.Reset();
    return true;
}

bool FGASPALSMimicKitVisualReplay::IsFiniteNumberArray(const TSharedPtr<FJsonObject>& Row, const FString& FieldName, int32 ExpectedDim)
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

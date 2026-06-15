#include "GASPALSMimicKitPackageLoader.h"

#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
const TCHAR* RequiredPackageFiles[] = {
    TEXT("brain_manifest.json"),
    TEXT("export_package_manifest.json"),
    TEXT("schema.json"),
    TEXT("joint_order.json"),
    TEXT("normalization_stats.json"),
    TEXT("obs_action_spec.yaml"),
    TEXT("visual_alignment_contract.json"),
    TEXT("skeletal_mapping_contract.json"),
    TEXT("visual_replay/pose_dof_meta.json"),
    TEXT("visual_replay/pose_dof_replay.jsonl"),
    TEXT("ai4animation_trace/trace_meta.json"),
    TEXT("ai4animation_trace/onnx_trace.jsonl"),
};

FString JoinPackagePath(const FString& PackageDirectory, const FString& RelativePath)
{
    return FPaths::ConvertRelativePathToFull(FPaths::Combine(PackageDirectory, RelativePath));
}

TSharedPtr<FJsonObject> GetObjectField(const TSharedPtr<FJsonObject>& Object, const FString& FieldName)
{
    if (!Object.IsValid())
    {
        return nullptr;
    }
    const TSharedPtr<FJsonObject>* Child = nullptr;
    return Object->TryGetObjectField(FieldName, Child) ? *Child : nullptr;
}

FString GetStringField(const TSharedPtr<FJsonObject>& Object, const FString& FieldName)
{
    FString Value;
    return Object.IsValid() && Object->TryGetStringField(FieldName, Value) ? Value : FString();
}

int32 GetIntField(const TSharedPtr<FJsonObject>& Object, const FString& FieldName)
{
    double Value = 0.0;
    return Object.IsValid() && Object->TryGetNumberField(FieldName, Value) ? static_cast<int32>(Value) : 0;
}

double GetDoubleField(const TSharedPtr<FJsonObject>& Object, const FString& FieldName)
{
    double Value = 0.0;
    return Object.IsValid() && Object->TryGetNumberField(FieldName, Value) ? Value : 0.0;
}

bool GetBoolField(const TSharedPtr<FJsonObject>& Object, const FString& FieldName)
{
    bool Value = false;
    return Object.IsValid() && Object->TryGetBoolField(FieldName, Value) ? Value : false;
}

int32 GetArrayCount(const TSharedPtr<FJsonObject>& Object, const FString& FieldName)
{
    const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
    return Object.IsValid() && Object->TryGetArrayField(FieldName, Values) ? Values->Num() : 0;
}
}

FString FGASPALSMimicKitPackageLoader::GetDefaultPackageRoot()
{
    return FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("MimicKitPackages")));
}

FString FGASPALSMimicKitPackageLoader::GetDefaultPackageDirectory(const FString& BrainName)
{
    return FPaths::Combine(GetDefaultPackageRoot(), BrainName);
}

bool FGASPALSMimicKitPackageLoader::LoadPackageByBrainName(const FString& BrainName, FGASPALSMimicKitPackageSummary& OutSummary, FString& OutError)
{
    return LoadPackage(GetDefaultPackageDirectory(BrainName), OutSummary, OutError);
}

bool FGASPALSMimicKitPackageLoader::LoadPackage(const FString& PackageDirectory, FGASPALSMimicKitPackageSummary& OutSummary, FString& OutError)
{
    OutSummary = FGASPALSMimicKitPackageSummary();
    OutSummary.PackageDirectory = FPaths::ConvertRelativePathToFull(PackageDirectory);

    HasRequiredFiles(OutSummary.PackageDirectory, OutSummary.MissingFiles);
    if (!OutSummary.MissingFiles.IsEmpty())
    {
        OutError = FString::Printf(TEXT("MimicKit package is missing %d required files."), OutSummary.MissingFiles.Num());
        return false;
    }

    TSharedPtr<FJsonObject> BrainManifest;
    if (!LoadJsonObjectFromFile(JoinPackagePath(OutSummary.PackageDirectory, TEXT("brain_manifest.json")), BrainManifest, OutError))
    {
        return false;
    }

    OutSummary.BrainName = GetStringField(BrainManifest, TEXT("brain_name"));
    OutSummary.ModelFileName = GetStringField(BrainManifest, TEXT("model"));
    OutSummary.PolicyHz = GetIntField(BrainManifest, TEXT("policy_hz"));
    OutSummary.PhysicsHz = GetIntField(BrainManifest, TEXT("physics_hz"));
    OutSummary.ObservationDim = GetIntField(BrainManifest, TEXT("observation_dim"));
    OutSummary.ActionDim = GetIntField(BrainManifest, TEXT("action_dim"));
    OutSummary.CoordinateBasis = GetStringField(BrainManifest, TEXT("coordinate_basis"));
    OutSummary.UnitScale = GetStringField(BrainManifest, TEXT("unit_scale"));

    if (OutSummary.CoordinateBasis.Contains(TEXT("pending")))
    {
        OutSummary.Warnings.Add(TEXT("coordinate_basis is still pending explicit contract; live UE observation building must remain blocked."));
    }

    if (!OutSummary.ModelFileName.IsEmpty() && !FPaths::FileExists(JoinPackagePath(OutSummary.PackageDirectory, OutSummary.ModelFileName)))
    {
        OutSummary.MissingFiles.Add(OutSummary.ModelFileName);
    }

    TSharedPtr<FJsonObject> Schema;
    if (!LoadJsonObjectFromFile(JoinPackagePath(OutSummary.PackageDirectory, TEXT("schema.json")), Schema, OutError))
    {
        return false;
    }
    const TSharedPtr<FJsonObject> Env = GetObjectField(Schema, TEXT("env"));
    OutSummary.EnvName = GetStringField(Env, TEXT("env_name"));
    OutSummary.ControlMode = GetStringField(Env, TEXT("control_mode"));
    if (OutSummary.ObservationDim != GetIntField(GetObjectField(Schema, TEXT("model")), TEXT("obs_dim")))
    {
        OutSummary.Warnings.Add(TEXT("brain_manifest observation_dim differs from schema.model.obs_dim."));
    }

    TSharedPtr<FJsonObject> JointOrder;
    if (!LoadJsonObjectFromFile(JoinPackagePath(OutSummary.PackageDirectory, TEXT("joint_order.json")), JointOrder, OutError))
    {
        return false;
    }
    OutSummary.BodyCount = GetArrayCount(JointOrder, TEXT("body_order"));
    OutSummary.JointCount = GetArrayCount(JointOrder, TEXT("joints"));
    OutSummary.DofSize = GetIntField(JointOrder, TEXT("dof_size"));

    bool bSanitizedNormalizationStats = false;
    TSharedPtr<FJsonObject> NormalizationStats;
    if (!LoadJsonObjectFromFile(JoinPackagePath(OutSummary.PackageDirectory, TEXT("normalization_stats.json")), NormalizationStats, OutError, true, &bSanitizedNormalizationStats))
    {
        return false;
    }
    OutSummary.bNormalizationStatsSanitized = bSanitizedNormalizationStats;
    if (bSanitizedNormalizationStats)
    {
        OutSummary.Warnings.Add(TEXT("normalization_stats.json contained non-standard Infinity/NaN tokens and was sanitized for shadow validation."));
    }

    TSharedPtr<FJsonObject> PackageManifest;
    if (!LoadJsonObjectFromFile(JoinPackagePath(OutSummary.PackageDirectory, TEXT("export_package_manifest.json")), PackageManifest, OutError))
    {
        return false;
    }

    TSharedPtr<FJsonObject> TraceMeta;
    if (!LoadJsonObjectFromFile(JoinPackagePath(OutSummary.PackageDirectory, TEXT("ai4animation_trace/trace_meta.json")), TraceMeta, OutError))
    {
        return false;
    }
    OutSummary.TraceRows = GetIntField(TraceMeta, TEXT("row_count"));
    OutSummary.TraceObsDim = GetIntField(TraceMeta, TEXT("obs_dim"));
    OutSummary.TraceActionDim = GetIntField(TraceMeta, TEXT("action_dim"));
    OutSummary.ParityMaxAbs = GetDoubleField(TraceMeta, TEXT("parity_max_abs"));
    OutSummary.ParityMeanAbs = GetDoubleField(TraceMeta, TEXT("parity_mean_abs"));
    OutSummary.ParityThreshold = GetDoubleField(TraceMeta, TEXT("max_abs_threshold"));
    OutSummary.bTraceThresholdPassed = GetBoolField(TraceMeta, TEXT("threshold_passed"));

    const FString VisualReplayMetaPath = JoinPackagePath(OutSummary.PackageDirectory, TEXT("visual_replay/pose_dof_meta.json"));
    const FString VisualReplayRowsPath = JoinPackagePath(OutSummary.PackageDirectory, TEXT("visual_replay/pose_dof_replay.jsonl"));
    if (FPaths::FileExists(VisualReplayMetaPath) && FPaths::FileExists(VisualReplayRowsPath))
    {
        TSharedPtr<FJsonObject> VisualReplayMeta;
        if (!LoadJsonObjectFromFile(VisualReplayMetaPath, VisualReplayMeta, OutError))
        {
            return false;
        }
        OutSummary.VisualReplayRows = GetIntField(VisualReplayMeta, TEXT("row_count"));
        OutSummary.VisualReplayDofDim = GetIntField(VisualReplayMeta, TEXT("dof_pos_dim"));
        OutSummary.bVisualReplayReady =
            OutSummary.VisualReplayRows > 0
            && OutSummary.VisualReplayDofDim == OutSummary.DofSize;
        if (!OutSummary.bVisualReplayReady)
        {
            OutSummary.Warnings.Add(TEXT("visual replay sidecar exists but failed package summary checks."));
        }
    }
    else
    {
        OutSummary.Warnings.Add(TEXT("visual replay sidecar is missing; UE visual bridge validation cannot be accepted."));
    }

    if (OutSummary.TraceObsDim != OutSummary.ObservationDim)
    {
        OutSummary.Warnings.Add(TEXT("trace obs_dim differs from brain manifest observation_dim."));
    }
    if (OutSummary.TraceActionDim != OutSummary.ActionDim)
    {
        OutSummary.Warnings.Add(TEXT("trace action_dim differs from brain manifest action_dim."));
    }

    OutSummary.bPackageReady =
        OutSummary.MissingFiles.IsEmpty()
        && !OutSummary.BrainName.IsEmpty()
        && OutSummary.ObservationDim > 0
        && OutSummary.ActionDim > 0
        && OutSummary.PolicyHz > 0
        && OutSummary.PhysicsHz > 0
        && OutSummary.TraceRows > 0
        && OutSummary.bTraceThresholdPassed;

    if (!OutSummary.bPackageReady)
    {
        OutError = TEXT("MimicKit package failed readiness checks.");
        return false;
    }

    OutError.Reset();
    return true;
}

bool FGASPALSMimicKitPackageLoader::LoadJsonObjectFromFile(const FString& FilePath, TSharedPtr<FJsonObject>& OutObject, FString& OutError, bool bAllowNonFiniteTokens, bool* bOutSanitized)
{
    FString Text;
    if (!FFileHelper::LoadFileToString(Text, *FilePath))
    {
        OutError = FString::Printf(TEXT("Could not read JSON file: %s"), *FilePath);
        return false;
    }

    bool bSanitized = false;
    auto TryParse = [&OutObject](const FString& JsonText) -> bool
    {
        const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonText);
        return FJsonSerializer::Deserialize(Reader, OutObject) && OutObject.IsValid();
    };

    if (!TryParse(Text))
    {
        if (!bAllowNonFiniteTokens)
        {
            OutError = FString::Printf(TEXT("Could not parse JSON file: %s"), *FilePath);
            return false;
        }

        FString Sanitized = Text;
        bSanitized = Sanitized.Contains(TEXT("Infinity")) || Sanitized.Contains(TEXT("NaN"));
        Sanitized = Sanitized.Replace(TEXT("-Infinity"), TEXT("0.0"));
        Sanitized = Sanitized.Replace(TEXT("Infinity"), TEXT("0.0"));
        Sanitized = Sanitized.Replace(TEXT("NaN"), TEXT("0.0"));

        if (!bSanitized || !TryParse(Sanitized))
        {
            OutError = FString::Printf(TEXT("Could not parse sanitized JSON file: %s"), *FilePath);
            return false;
        }
    }

    if (bOutSanitized)
    {
        *bOutSanitized = bSanitized;
    }
    return true;
}

bool FGASPALSMimicKitPackageLoader::HasRequiredFiles(const FString& PackageDirectory, TArray<FString>& OutMissingFiles)
{
    OutMissingFiles.Reset();
    for (const TCHAR* RelativePath : RequiredPackageFiles)
    {
        if (!FPaths::FileExists(JoinPackagePath(PackageDirectory, RelativePath)))
        {
            OutMissingFiles.Add(RelativePath);
        }
    }
    return OutMissingFiles.IsEmpty();
}

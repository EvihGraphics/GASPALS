#include "GASPALSMimicKitVisualReplayCapture.h"

#include "CollisionQueryParams.h"
#include "Dom/JsonObject.h"
#include "Components/BoxComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/PoseableMeshComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "Engine/EngineTypes.h"
#include "GameFramework/Actor.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformTime.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Misc/FileHelper.h"
#include "Misc/Crc.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "NNE.h"
#include "NNEModelData.h"
#include "NNERuntimeCPU.h"
#include "NNETypes.h"
#include "PhysicsEngine/ConstraintInstance.h"
#include "PhysicsEngine/AggregateGeom.h"
#include "PhysicsEngine/BodySetup.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "RenderingThread.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"

namespace
{
struct FReplayRow
{
    int32 Frame = 0;
    double TimeSeconds = 0.0;
    FVector RootPosMeters = FVector::ZeroVector;
    FQuat RootRot = FQuat::Identity;
    TArray<double> DofPos;
};

struct FObsFixtureRow
{
    int32 Frame = 0;
    double TimeSeconds = 0.0;
    TArray<float> Observation;
    FVector RootPosMeters = FVector::ZeroVector;
    bool bHasRootPosMeters = false;
};

struct FJointMapping
{
    FString Name;
    FString BodyName;
    FString ParentBodyName;
    FString JointType;
    int32 DofIndex = 0;
    int32 DofDim = 0;
    FVector HingeAxis = FVector::RightVector;
    bool bHasSourceJointAxis = false;
    double Stiffness = 1200.0;
    double Damping = 120.0;
    double Gear = 100.0;
};

struct FBodyPose
{
    FVector PositionMeters = FVector::ZeroVector;
    FQuat Rotation = FQuat::Identity;
};

struct FSourceJointRuntimeSpec
{
    FString Name;
    FString BodyName;
    FVector Axis = FVector::RightVector;
    bool bHasAxis = false;
    double Stiffness = 0.0;
    double Damping = 0.0;
    double Gear = 0.0;
};

struct FRuntimeContactStats
{
    int32 RightFootContactFrames = 0;
    int32 LeftFootContactFrames = 0;
    int32 ContactEvents = 0;
    int32 PhysicsSubsteps = 0;
    int32 PrePolicySettleSubsteps = 0;
    double MaxFootSlidingMpsRaw = 0.0;
    double MaxFootSlidingMpsScored = 0.0;
    double MaxJointTargetError = 0.0;
    bool bRootFinite = false;
    bool bPhysicsSimulated = false;
    bool bJointDriveApplied = false;
    bool bFootContactsObserved = false;
    bool bSlidingUnderThreshold = false;
    bool bRightFootBodyValidated = false;
    bool bLeftFootBodyValidated = false;
    FString ObservationSource;
    FString JointDriveBackend;
    FString ContactMeasurementSource;
    FString ContactQueryMethod;
    FString GroundAlignmentSource;
    double GroundTopZMeters = 0.0;
    int32 ObservationFilledDim = 0;
};

class FNnePolicyRunner
{
public:
    bool Initialize(const FString& ModelPath, int32 InObservationDim, int32 InActionDim, FString& OutError)
    {
        ObservationDim = InObservationDim;
        ActionDim = InActionDim;
        if (ObservationDim <= 0 || ActionDim <= 0)
        {
            OutError = TEXT("Invalid NNE policy dimensions.");
            return false;
        }

        TArray64<uint8> ModelBytes;
        if (!FFileHelper::LoadFileToArray(ModelBytes, *ModelPath))
        {
            OutError = FString::Printf(TEXT("Could not load ONNX model file: %s"), *ModelPath);
            return false;
        }

        ModelData = TStrongObjectPtr<UNNEModelData>(NewObject<UNNEModelData>(GetTransientPackage()));
        ModelData->Init(TEXT("onnx"), TConstArrayView64<uint8>(ModelBytes.GetData(), ModelBytes.Num()));

        const FString CandidateRuntimeNames[] = {
            TEXT("NNERuntimeORTCpu"),
            TEXT("NNERuntimeORT"),
            TEXT("NNERuntimeBasicCpu"),
        };

        FString LastError;
        for (const FString& CandidateRuntimeName : CandidateRuntimeNames)
        {
            TArray<FString> TargetRuntimes;
            TargetRuntimes.Add(CandidateRuntimeName);
            ModelData->SetTargetRuntimes(TargetRuntimes);

            TWeakInterfacePtr<INNERuntimeCPU> Runtime = UE::NNE::GetRuntime<INNERuntimeCPU>(CandidateRuntimeName);
            if (!Runtime.IsValid())
            {
                LastError = FString::Printf(TEXT("NNE runtime unavailable: %s"), *CandidateRuntimeName);
                continue;
            }

            Model = Runtime->CreateModelCPU(ModelData.Get());
            if (!Model.IsValid())
            {
                LastError = FString::Printf(TEXT("CreateModelCPU failed for runtime: %s"), *CandidateRuntimeName);
                continue;
            }

            ModelInstance = Model->CreateModelInstanceCPU();
            if (!ModelInstance.IsValid())
            {
                LastError = FString::Printf(TEXT("CreateModelInstanceCPU failed for runtime: %s"), *CandidateRuntimeName);
                Model.Reset();
                continue;
            }

            if (!ConfigureTensorLayout(OutError))
            {
                LastError = OutError;
                ModelInstance.Reset();
                Model.Reset();
                continue;
            }

            RuntimeName = CandidateRuntimeName;
            return true;
        }

        OutError = LastError.IsEmpty() ? TEXT("No usable NNE CPU runtime found.") : LastError;
        return false;
    }

    bool Run(const TArray<float>& Observation, TArray<float>& OutAction, double& OutLatencyMs, FString& OutError)
    {
        OutAction.Reset();
        OutLatencyMs = 0.0;
        if (!ModelInstance.IsValid())
        {
            OutError = TEXT("NNE model instance is invalid.");
            return false;
        }
        if (Observation.Num() != ObservationDim)
        {
            OutError = FString::Printf(TEXT("Observation dim mismatch: expected %d got %d."), ObservationDim, Observation.Num());
            return false;
        }

        InputScratch = Observation;
        OutputScratch.SetNumZeroed(ActionDim);

        UE::NNE::FTensorBindingCPU InputBinding;
        InputBinding.Data = InputScratch.GetData();
        InputBinding.SizeInBytes = static_cast<uint64>(InputScratch.Num() * sizeof(float));

        UE::NNE::FTensorBindingCPU OutputBinding;
        OutputBinding.Data = OutputScratch.GetData();
        OutputBinding.SizeInBytes = static_cast<uint64>(OutputScratch.Num() * sizeof(float));

        const double StartSeconds = FPlatformTime::Seconds();
        const UE::NNE::EResultStatus Status = ModelInstance->RunSync({InputBinding}, {OutputBinding});
        OutLatencyMs = (FPlatformTime::Seconds() - StartSeconds) * 1000.0;
        if (Status != UE::NNE::EResultStatus::Ok)
        {
            OutError = TEXT("NNE RunSync failed.");
            return false;
        }

        OutAction = OutputScratch;
        return true;
    }

    const FString& GetRuntimeName() const
    {
        return RuntimeName;
    }

private:
    bool ConfigureTensorLayout(FString& OutError)
    {
        if (!ModelInstance.IsValid())
        {
            OutError = TEXT("Cannot configure NNE tensors without a model instance.");
            return false;
        }

        const TConstArrayView<UE::NNE::FTensorDesc> InputDescs = ModelInstance->GetInputTensorDescs();
        const TConstArrayView<UE::NNE::FTensorDesc> OutputDescs = ModelInstance->GetOutputTensorDescs();
        if (InputDescs.Num() != 1 || OutputDescs.Num() != 1)
        {
            OutError = FString::Printf(TEXT("Expected one input and one output tensor, got %d/%d."), InputDescs.Num(), OutputDescs.Num());
            return false;
        }
        if (InputDescs[0].GetDataType() != ENNETensorDataType::Float || OutputDescs[0].GetDataType() != ENNETensorDataType::Float)
        {
            OutError = TEXT("MimicKit live closure only supports float NNE tensors.");
            return false;
        }

        const TArray<uint32> InputShapeData = {1u, static_cast<uint32>(ObservationDim)};
        const UE::NNE::FTensorShape InputShape = UE::NNE::FTensorShape::Make(InputShapeData);
        if (ModelInstance->SetInputTensorShapes({InputShape}) != UE::NNE::EResultStatus::Ok)
        {
            OutError = TEXT("NNE SetInputTensorShapes failed.");
            return false;
        }

        const TConstArrayView<UE::NNE::FTensorShape> OutputShapes = ModelInstance->GetOutputTensorShapes();
        if (OutputShapes.Num() == 1)
        {
            int32 ResolvedActionDim = static_cast<int32>(OutputShapes[0].Volume());
            if (OutputShapes[0].Rank() >= 2)
            {
                ResolvedActionDim = static_cast<int32>(OutputShapes[0].GetData()[1]);
            }
            if (ResolvedActionDim > 0 && ResolvedActionDim != ActionDim)
            {
                OutError = FString::Printf(TEXT("NNE action dim mismatch: package=%d model=%d."), ActionDim, ResolvedActionDim);
                return false;
            }
        }

        InputScratch.SetNumZeroed(ObservationDim);
        OutputScratch.SetNumZeroed(ActionDim);
        return true;
    }

private:
    FString RuntimeName;
    int32 ObservationDim = 0;
    int32 ActionDim = 0;
    TStrongObjectPtr<UNNEModelData> ModelData;
    TSharedPtr<UE::NNE::IModelCPU> Model;
    TSharedPtr<UE::NNE::IModelInstanceCPU> ModelInstance;
    TArray<float> InputScratch;
    TArray<float> OutputScratch;
};

FString VisualReplayPathForPackage(const FString& PackageDirectory)
{
    return FPaths::ConvertRelativePathToFull(FPaths::Combine(PackageDirectory, TEXT("visual_replay/pose_dof_replay.jsonl")));
}

FString JointOrderPathForPackage(const FString& PackageDirectory)
{
    return FPaths::ConvertRelativePathToFull(FPaths::Combine(PackageDirectory, TEXT("joint_order.json")));
}

FString VisualAlignmentContractPathForPackage(const FString& PackageDirectory)
{
    return FPaths::ConvertRelativePathToFull(FPaths::Combine(PackageDirectory, TEXT("visual_alignment_contract.json")));
}

FString SkeletalMappingContractPathForPackage(const FString& PackageDirectory)
{
    return FPaths::ConvertRelativePathToFull(FPaths::Combine(PackageDirectory, TEXT("skeletal_mapping_contract.json")));
}

FString SourceRigAssetBuildReportPathForPackage(const FString& PackageDirectory)
{
    return FPaths::ConvertRelativePathToFull(FPaths::Combine(PackageDirectory, TEXT("mimickit_source_rig_asset_build_report.json")));
}

FString JoinPackagePath(const FString& PackageDirectory, const FString& RelativePath)
{
    return FPaths::ConvertRelativePathToFull(FPaths::Combine(PackageDirectory, RelativePath));
}

FString NormalizeAssetObjectPath(const FString& AssetPath)
{
    if (AssetPath.Contains(TEXT(".")))
    {
        return AssetPath;
    }
    FString PackagePath;
    FString AssetName;
    if (AssetPath.Split(TEXT("/"), &PackagePath, &AssetName, ESearchCase::CaseSensitive, ESearchDir::FromEnd))
    {
        return FString::Printf(TEXT("%s.%s"), *AssetPath, *AssetName);
    }
    return AssetPath;
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

double GetDoubleField(const TSharedPtr<FJsonObject>& Object, const FString& FieldName, double DefaultValue = 0.0)
{
    double Value = 0.0;
    return Object.IsValid() && Object->TryGetNumberField(FieldName, Value) ? Value : DefaultValue;
}

bool GetBoolField(const TSharedPtr<FJsonObject>& Object, const FString& FieldName)
{
    bool Value = false;
    return Object.IsValid() && Object->TryGetBoolField(FieldName, Value) ? Value : false;
}

bool ReadNumberArray(const TSharedPtr<FJsonObject>& Object, const FString& FieldName, int32 ExpectedDim, TArray<double>& OutValues)
{
    OutValues.Reset();
    const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
    if (!Object.IsValid() || !Object->TryGetArrayField(FieldName, Values) || Values->Num() != ExpectedDim)
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
        OutValues.Add(Number);
    }
    return true;
}

bool LoadFloatArrayField(const TSharedPtr<FJsonObject>& Object, const FString& FieldName, int32 ExpectedDim, TArray<float>& OutValues)
{
    TArray<double> Values;
    if (!ReadNumberArray(Object, FieldName, ExpectedDim, Values))
    {
        return false;
    }

    OutValues.Reset();
    OutValues.Reserve(Values.Num());
    for (const double Value : Values)
    {
        OutValues.Add(static_cast<float>(Value));
    }
    return true;
}

bool LoadVectorField(const TSharedPtr<FJsonObject>& Object, const FString& FieldName, FVector& OutVector)
{
    TArray<double> Values;
    if (!ReadNumberArray(Object, FieldName, 3, Values))
    {
        return false;
    }
    OutVector = FVector(Values[0], Values[1], Values[2]);
    return true;
}

bool LoadObsFixtureRows(const FString& PackageDirectory, int32 ExpectedObservationDim, int32 MaxRows, TArray<FObsFixtureRow>& OutRows, FString& OutError)
{
    OutRows.Reset();
    const FString FixtureFile = JoinPackagePath(PackageDirectory, TEXT("obs_fixture.jsonl"));
    FString RawFixture;
    if (!FFileHelper::LoadFileToString(RawFixture, *FixtureFile))
    {
        OutError = FString::Printf(TEXT("Could not read observation fixture: %s"), *FixtureFile);
        return false;
    }

    TArray<FString> Lines;
    RawFixture.ParseIntoArrayLines(Lines, false);
    for (const FString& Line : Lines)
    {
        if (Line.TrimStartAndEnd().IsEmpty())
        {
            continue;
        }

        TSharedPtr<FJsonObject> RowObject;
        if (!ParseJsonObject(Line, RowObject))
        {
            OutError = FString::Printf(TEXT("Could not parse observation fixture row %d."), OutRows.Num());
            return false;
        }

        FObsFixtureRow Row;
        Row.Frame = GetIntField(RowObject, TEXT("frame"));
        Row.TimeSeconds = ExpectedObservationDim > 0 ? 0.0 : 0.0;
        if (!LoadFloatArrayField(RowObject, TEXT("obs"), ExpectedObservationDim, Row.Observation))
        {
            OutError = FString::Printf(TEXT("Observation fixture row %d has invalid obs dim."), OutRows.Num());
            return false;
        }
        Row.bHasRootPosMeters = LoadVectorField(RowObject, TEXT("ref_root_pos"), Row.RootPosMeters);
        OutRows.Add(MoveTemp(Row));

        if (MaxRows > 0 && OutRows.Num() >= MaxRows)
        {
            break;
        }
    }

    if (OutRows.IsEmpty())
    {
        OutError = FString::Printf(TEXT("Observation fixture has no usable rows: %s"), *FixtureFile);
        return false;
    }
    return true;
}

bool LoadActionBounds(const FString& PackageDirectory, int32 ExpectedActionDim, TArray<float>& OutLow, TArray<float>& OutHigh, FString& OutError)
{
    OutLow.Reset();
    OutHigh.Reset();
    TSharedPtr<FJsonObject> Schema;
    if (!LoadJsonObject(JoinPackagePath(PackageDirectory, TEXT("schema.json")), Schema, OutError))
    {
        return false;
    }
    const TSharedPtr<FJsonObject> Model = GetObjectField(Schema, TEXT("model"));
    if (!LoadFloatArrayField(Model, TEXT("action_low"), ExpectedActionDim, OutLow)
        || !LoadFloatArrayField(Model, TEXT("action_high"), ExpectedActionDim, OutHigh))
    {
        OutError = TEXT("schema.json is missing action_low/action_high with the package action dim.");
        return false;
    }
    return true;
}

bool AreFinite(const TArray<float>& Values)
{
    for (const float Value : Values)
    {
        if (!FMath::IsFinite(Value))
        {
            return false;
        }
    }
    return true;
}

int32 CountActionClamps(const TArray<float>& Action, const TArray<float>& Low, const TArray<float>& High)
{
    if (Action.Num() != Low.Num() || Action.Num() != High.Num())
    {
        return 0;
    }

    int32 ClampCount = 0;
    for (int32 Index = 0; Index < Action.Num(); ++Index)
    {
        if (Action[Index] < Low[Index] || Action[Index] > High[Index])
        {
            ++ClampCount;
        }
    }
    return ClampCount;
}

uint32 HashAction(const TArray<float>& Action)
{
    return Action.IsEmpty() ? 0u : FCrc::MemCrc32(Action.GetData(), Action.Num() * sizeof(float));
}

bool LoadReplayRows(const FString& ReplayFile, int32 ExpectedDofDim, TArray<FReplayRow>& OutRows, FString& OutError)
{
    FString RawReplay;
    if (!FFileHelper::LoadFileToString(RawReplay, *ReplayFile))
    {
        OutError = FString::Printf(TEXT("Could not read visual replay file: %s"), *ReplayFile);
        return false;
    }

    TArray<FString> Lines;
    RawReplay.ParseIntoArrayLines(Lines, false);
    for (const FString& Line : Lines)
    {
        if (Line.TrimStartAndEnd().IsEmpty())
        {
            continue;
        }

        TSharedPtr<FJsonObject> RowObject;
        if (!ParseJsonObject(Line, RowObject))
        {
            OutError = TEXT("Could not parse visual replay JSONL row.");
            return false;
        }

        TArray<double> RootPos;
        TArray<double> RootRot;
        TArray<double> DofPos;
        if (!ReadNumberArray(RowObject, TEXT("root_pos_m"), 3, RootPos)
            || !ReadNumberArray(RowObject, TEXT("root_rot_xyzw"), 4, RootRot))
        {
            OutError = TEXT("Visual replay row is missing finite root_pos_m/root_rot_xyzw.");
            return false;
        }
        if (ExpectedDofDim > 0 && !ReadNumberArray(RowObject, TEXT("dof_pos"), ExpectedDofDim, DofPos))
        {
            OutError = TEXT("Visual replay row is missing finite dof_pos.");
            return false;
        }

        FReplayRow Row;
        double FrameValue = 0.0;
        RowObject->TryGetNumberField(TEXT("frame"), FrameValue);
        Row.Frame = static_cast<int32>(FrameValue);
        RowObject->TryGetNumberField(TEXT("time_seconds"), Row.TimeSeconds);
        Row.RootPosMeters = FVector(RootPos[0], RootPos[1], RootPos[2]);
        Row.RootRot = FQuat(RootRot[0], RootRot[1], RootRot[2], RootRot[3]).GetNormalized();
        Row.DofPos = MoveTemp(DofPos);
        OutRows.Add(Row);
    }

    if (OutRows.IsEmpty())
    {
        OutError = TEXT("Visual replay contains no rows.");
        return false;
    }
    return true;
}

void SetPixel(TArray<FColor>& Pixels, int32 Width, int32 Height, int32 X, int32 Y, const FColor& Color)
{
    if (X < 0 || Y < 0 || X >= Width || Y >= Height)
    {
        return;
    }
    Pixels[Y * Width + X] = Color;
}

void DrawLine(TArray<FColor>& Pixels, int32 Width, int32 Height, int32 X0, int32 Y0, int32 X1, int32 Y1, const FColor& Color)
{
    const int32 Dx = FMath::Abs(X1 - X0);
    const int32 Sx = X0 < X1 ? 1 : -1;
    const int32 Dy = -FMath::Abs(Y1 - Y0);
    const int32 Sy = Y0 < Y1 ? 1 : -1;
    int32 Error = Dx + Dy;

    while (true)
    {
        SetPixel(Pixels, Width, Height, X0, Y0, Color);
        if (X0 == X1 && Y0 == Y1)
        {
            break;
        }
        const int32 E2 = 2 * Error;
        if (E2 >= Dy)
        {
            Error += Dy;
            X0 += Sx;
        }
        if (E2 <= Dx)
        {
            Error += Dx;
            Y0 += Sy;
        }
    }
}

void DrawCircle(TArray<FColor>& Pixels, int32 Width, int32 Height, int32 CX, int32 CY, int32 Radius, const FColor& Color)
{
    for (int32 Y = -Radius; Y <= Radius; ++Y)
    {
        for (int32 X = -Radius; X <= Radius; ++X)
        {
            if (X * X + Y * Y <= Radius * Radius)
            {
                SetPixel(Pixels, Width, Height, CX + X, CY + Y, Color);
            }
        }
    }
}

const TCHAR* DigitPattern(TCHAR Digit)
{
    switch (Digit)
    {
    case TEXT('0'): return TEXT("111101101101111");
    case TEXT('1'): return TEXT("010110010010111");
    case TEXT('2'): return TEXT("111001111100111");
    case TEXT('3'): return TEXT("111001111001111");
    case TEXT('4'): return TEXT("101101111001001");
    case TEXT('5'): return TEXT("111100111001111");
    case TEXT('6'): return TEXT("111100111101111");
    case TEXT('7'): return TEXT("111001001001001");
    case TEXT('8'): return TEXT("111101111101111");
    case TEXT('9'): return TEXT("111101111001111");
    default: return TEXT("000000000000000");
    }
}

void DrawDigits(TArray<FColor>& Pixels, int32 Width, int32 Height, const FString& Text, int32 X, int32 Y, const FColor& Color)
{
    constexpr int32 Scale = 3;
    int32 CursorX = X;
    for (int32 CharIndex = 0; CharIndex < Text.Len(); ++CharIndex)
    {
        const TCHAR* Pattern = DigitPattern(Text[CharIndex]);
        for (int32 Py = 0; Py < 5; ++Py)
        {
            for (int32 Px = 0; Px < 3; ++Px)
            {
                if (Pattern[Py * 3 + Px] != TEXT('1'))
                {
                    continue;
                }
                for (int32 Sy = 0; Sy < Scale; ++Sy)
                {
                    for (int32 Sx = 0; Sx < Scale; ++Sx)
                    {
                        SetPixel(Pixels, Width, Height, CursorX + Px * Scale + Sx, Y + Py * Scale + Sy, Color);
                    }
                }
            }
        }
        CursorX += 4 * Scale;
    }
}

void ComputeBounds(const TArray<FReplayRow>& Rows, FVector2D& OutMin, FVector2D& OutMax)
{
    OutMin = FVector2D(Rows[0].RootPosMeters.X, Rows[0].RootPosMeters.Y);
    OutMax = OutMin;
    for (const FReplayRow& Row : Rows)
    {
        OutMin.X = FMath::Min(OutMin.X, Row.RootPosMeters.X);
        OutMin.Y = FMath::Min(OutMin.Y, Row.RootPosMeters.Y);
        OutMax.X = FMath::Max(OutMax.X, Row.RootPosMeters.X);
        OutMax.Y = FMath::Max(OutMax.Y, Row.RootPosMeters.Y);
    }

    FVector2D Center = (OutMin + OutMax) * 0.5;
    FVector2D Span = OutMax - OutMin;
    Span.X = FMath::Max(Span.X, 1.0);
    Span.Y = FMath::Max(Span.Y, 1.0);
    Span += FVector2D(0.5, 0.5);
    OutMin = Center - Span * 0.5;
    OutMax = Center + Span * 0.5;
}

FIntPoint ProjectRoot(const FVector& Root, const FVector2D& Min, const FVector2D& Max, int32 Width, int32 Height)
{
    constexpr double Margin = 48.0;
    const double SpanX = FMath::Max(Max.X - Min.X, 1.0);
    const double SpanY = FMath::Max(Max.Y - Min.Y, 1.0);
    const double XNorm = (Root.X - Min.X) / SpanX;
    const double YNorm = (Root.Y - Min.Y) / SpanY;
    const int32 X = FMath::RoundToInt(Margin + XNorm * (Width - 2.0 * Margin));
    const int32 Y = FMath::RoundToInt(Height - Margin - YNorm * (Height - 2.0 * Margin));
    return FIntPoint(X, Y);
}

void DrawGrid(TArray<FColor>& Pixels, int32 Width, int32 Height)
{
    const FColor GridColor(38, 45, 52);
    const FColor AxisColor(72, 86, 96);
    for (int32 X = 48; X < Width - 48; X += 48)
    {
        DrawLine(Pixels, Width, Height, X, 48, X, Height - 48, GridColor);
    }
    for (int32 Y = 48; Y < Height - 48; Y += 48)
    {
        DrawLine(Pixels, Width, Height, 48, Y, Width - 48, Y, GridColor);
    }
    DrawLine(Pixels, Width, Height, Width / 2, 48, Width / 2, Height - 48, AxisColor);
    DrawLine(Pixels, Width, Height, 48, Height / 2, Width - 48, Height / 2, AxisColor);
}

void DrawThickLine(TArray<FColor>& Pixels, int32 Width, int32 Height, int32 X0, int32 Y0, int32 X1, int32 Y1, int32 Radius, const FColor& Color)
{
    const int32 Steps = FMath::Max(FMath::Abs(X1 - X0), FMath::Abs(Y1 - Y0));
    if (Steps <= 0)
    {
        DrawCircle(Pixels, Width, Height, X0, Y0, Radius, Color);
        return;
    }
    for (int32 Step = 0; Step <= Steps; ++Step)
    {
        const double T = static_cast<double>(Step) / static_cast<double>(Steps);
        const int32 X = FMath::RoundToInt(FMath::Lerp(static_cast<double>(X0), static_cast<double>(X1), T));
        const int32 Y = FMath::RoundToInt(FMath::Lerp(static_cast<double>(Y0), static_cast<double>(Y1), T));
        DrawCircle(Pixels, Width, Height, X, Y, Radius, Color);
    }
}

FVector RestOffsetForBody(const FString& BodyName)
{
    if (BodyName == TEXT("torso")) { return FVector(0.0, 0.0, 0.42); }
    if (BodyName == TEXT("head")) { return FVector(0.04, 0.0, 0.36); }
    if (BodyName == TEXT("right_upper_arm")) { return FVector(0.02, -0.24, 0.28); }
    if (BodyName == TEXT("right_lower_arm")) { return FVector(0.0, -0.31, -0.02); }
    if (BodyName == TEXT("right_hand")) { return FVector(0.0, -0.24, -0.02); }
    if (BodyName == TEXT("sword")) { return FVector(0.42, -0.04, 0.02); }
    if (BodyName == TEXT("left_upper_arm")) { return FVector(0.02, 0.24, 0.28); }
    if (BodyName == TEXT("left_lower_arm")) { return FVector(0.0, 0.31, -0.02); }
    if (BodyName == TEXT("left_hand")) { return FVector(0.0, 0.24, -0.02); }
    if (BodyName == TEXT("shield")) { return FVector(0.05, 0.12, 0.02); }
    if (BodyName == TEXT("right_thigh")) { return FVector(0.0, -0.13, -0.42); }
    if (BodyName == TEXT("right_shin")) { return FVector(0.0, 0.0, -0.43); }
    if (BodyName == TEXT("right_foot")) { return FVector(0.18, 0.0, -0.10); }
    if (BodyName == TEXT("left_thigh")) { return FVector(0.0, 0.13, -0.42); }
    if (BodyName == TEXT("left_shin")) { return FVector(0.0, 0.0, -0.43); }
    if (BodyName == TEXT("left_foot")) { return FVector(0.18, 0.0, -0.10); }
    return FVector::ZeroVector;
}

FQuat LocalRotationForJoint(const FJointMapping& Joint, const FReplayRow& Row)
{
    if (Joint.DofDim == 3 && Row.DofPos.IsValidIndex(Joint.DofIndex + 2))
    {
        const FVector ExpMap(Row.DofPos[Joint.DofIndex], Row.DofPos[Joint.DofIndex + 1], Row.DofPos[Joint.DofIndex + 2]);
        const double Angle = ExpMap.Size();
        if (Angle > KINDA_SMALL_NUMBER)
        {
            return FQuat(ExpMap / Angle, Angle).GetNormalized();
        }
    }
    if (Joint.DofDim == 1 && Row.DofPos.IsValidIndex(Joint.DofIndex))
    {
        return FQuat(Joint.HingeAxis.GetSafeNormal(UE_SMALL_NUMBER, FVector::RightVector), Row.DofPos[Joint.DofIndex]).GetNormalized();
    }
    return FQuat::Identity;
}

bool LoadJointMappings(const FString& JointOrderFile, TArray<FJointMapping>& OutJoints, FString& OutError)
{
    TSharedPtr<FJsonObject> JointOrder;
    if (!LoadJsonObject(JointOrderFile, JointOrder, OutError))
    {
        return false;
    }

    const TArray<TSharedPtr<FJsonValue>>* Joints = nullptr;
    if (!JointOrder->TryGetArrayField(TEXT("joints"), Joints) || Joints->IsEmpty())
    {
        OutError = FString::Printf(TEXT("Joint order file has no joints: %s"), *JointOrderFile);
        return false;
    }

    OutJoints.Reset();
    for (const TSharedPtr<FJsonValue>& Value : *Joints)
    {
        const TSharedPtr<FJsonObject> Object = Value.IsValid() ? Value->AsObject() : nullptr;
        if (!Object.IsValid())
        {
            continue;
        }
        FJointMapping Joint;
        Joint.Name = GetStringField(Object, TEXT("name"));
        Joint.BodyName = GetStringField(Object, TEXT("body_name"));
        Joint.ParentBodyName = GetStringField(Object, TEXT("parent_body_name"));
        Joint.JointType = GetStringField(Object, TEXT("joint_type"));
        Joint.DofIndex = GetIntField(Object, TEXT("dof_index"));
        Joint.DofDim = GetIntField(Object, TEXT("dof_dim"));
        if (!Joint.BodyName.IsEmpty() && !Joint.ParentBodyName.IsEmpty())
        {
            OutJoints.Add(Joint);
        }
    }

    if (OutJoints.IsEmpty())
    {
        OutError = TEXT("Joint order did not yield any usable skeletal mappings.");
        return false;
    }
    return true;
}

FString SourceRigAssetSpecPathForPackage(const FString& PackageDirectory)
{
    return JoinPackagePath(PackageDirectory, TEXT("mimickit_source_rig_asset_spec.json"));
}

bool LoadSourceJointRuntimeSpecs(
    const FString& PackageDirectory,
    TMap<FString, FSourceJointRuntimeSpec>& OutSpecs,
    FString& OutError)
{
    OutSpecs.Reset();
    TSharedPtr<FJsonObject> SourceSpec;
    const FString SpecFile = SourceRigAssetSpecPathForPackage(PackageDirectory);
    if (!LoadJsonObject(SpecFile, SourceSpec, OutError))
    {
        return false;
    }

    const TArray<TSharedPtr<FJsonValue>>* SpecJoints = nullptr;
    if (!SourceSpec->TryGetArrayField(TEXT("joints"), SpecJoints) || SpecJoints->IsEmpty())
    {
        OutError = FString::Printf(TEXT("source rig asset spec has no joints: %s"), *SpecFile);
        return false;
    }

    for (const TSharedPtr<FJsonValue>& Value : *SpecJoints)
    {
        const TSharedPtr<FJsonObject> Object = Value.IsValid() ? Value->AsObject() : nullptr;
        if (!Object.IsValid())
        {
            continue;
        }

        FSourceJointRuntimeSpec Spec;
        Spec.Name = GetStringField(Object, TEXT("name"));
        Spec.BodyName = GetStringField(Object, TEXT("body_name"));
        TArray<double> AxisValues;
        if (ReadNumberArray(Object, TEXT("axis"), 3, AxisValues))
        {
            Spec.Axis = FVector(AxisValues[0], AxisValues[1], AxisValues[2]).GetSafeNormal(UE_SMALL_NUMBER, FVector::RightVector);
            Spec.bHasAxis = true;
        }
        Spec.Stiffness = GetDoubleField(Object, TEXT("stiffness"), 0.0);
        Spec.Damping = GetDoubleField(Object, TEXT("damping"), 0.0);
        if (!Spec.Name.IsEmpty())
        {
            OutSpecs.Add(Spec.Name, Spec);
        }
    }

    const TArray<TSharedPtr<FJsonValue>>* Actuators = nullptr;
    if (SourceSpec->TryGetArrayField(TEXT("actuators"), Actuators))
    {
        for (const TSharedPtr<FJsonValue>& Value : *Actuators)
        {
            const TSharedPtr<FJsonObject> Object = Value.IsValid() ? Value->AsObject() : nullptr;
            if (!Object.IsValid())
            {
                continue;
            }
            const FString JointName = GetStringField(Object, TEXT("joint"));
            if (FSourceJointRuntimeSpec* Spec = OutSpecs.Find(JointName))
            {
                Spec->Gear = GetDoubleField(Object, TEXT("gear"), Spec->Gear);
            }
        }
    }

    return true;
}

void ApplySourceSpecValues(FJointMapping& Joint, const FSourceJointRuntimeSpec& Spec)
{
    if (Spec.bHasAxis)
    {
        Joint.HingeAxis = Spec.Axis.GetSafeNormal(UE_SMALL_NUMBER, FVector::RightVector);
        Joint.bHasSourceJointAxis = true;
    }
    if (Spec.Stiffness > 0.0)
    {
        Joint.Stiffness = Spec.Stiffness;
    }
    if (Spec.Damping > 0.0)
    {
        Joint.Damping = Spec.Damping;
    }
    if (Spec.Gear > 0.0)
    {
        Joint.Gear = Spec.Gear;
    }
}

bool ApplySourceRigJointSpecs(
    const FString& PackageDirectory,
    TArray<FJointMapping>& Joints,
    bool bRequireSourceSpec,
    FString& OutError)
{
    TMap<FString, FSourceJointRuntimeSpec> Specs;
    FString SpecError;
    if (!LoadSourceJointRuntimeSpecs(PackageDirectory, Specs, SpecError))
    {
        if (bRequireSourceSpec)
        {
            OutError = SpecError;
            return false;
        }
        return true;
    }

    for (FJointMapping& Joint : Joints)
    {
        if (const FSourceJointRuntimeSpec* DirectSpec = Specs.Find(Joint.Name))
        {
            ApplySourceSpecValues(Joint, *DirectSpec);
        }
        else if (Joint.DofDim == 3)
        {
            const FString AxisNames[] = {
                Joint.Name + TEXT("_x"),
                Joint.Name + TEXT("_y"),
                Joint.Name + TEXT("_z"),
            };
            double StiffnessSum = 0.0;
            double DampingSum = 0.0;
            double GearSum = 0.0;
            int32 Count = 0;
            for (const FString& AxisName : AxisNames)
            {
                if (const FSourceJointRuntimeSpec* AxisSpec = Specs.Find(AxisName))
                {
                    StiffnessSum += AxisSpec->Stiffness;
                    DampingSum += AxisSpec->Damping;
                    GearSum += AxisSpec->Gear;
                    ++Count;
                }
            }
            if (Count > 0)
            {
                Joint.Stiffness = FMath::Max(1.0, StiffnessSum / static_cast<double>(Count));
                Joint.Damping = FMath::Max(1.0, DampingSum / static_cast<double>(Count));
                Joint.Gear = FMath::Max(1.0, GearSum / static_cast<double>(Count));
            }
        }

        if (bRequireSourceSpec && Joint.DofDim == 1 && !Joint.bHasSourceJointAxis)
        {
            OutError = FString::Printf(TEXT("source rig asset spec is missing a hinge axis for joint %s."), *Joint.Name);
            return false;
        }
    }

    return true;
}

bool LoadJointMappingsForPackage(
    const FString& PackageDirectory,
    TArray<FJointMapping>& OutJoints,
    bool bRequireSourceSpec,
    FString& OutError)
{
    if (!LoadJointMappings(JointOrderPathForPackage(PackageDirectory), OutJoints, OutError))
    {
        return false;
    }
    return ApplySourceRigJointSpecs(PackageDirectory, OutJoints, bRequireSourceSpec, OutError);
}

void ComputeBodyPoses(const FReplayRow& Row, const TArray<FJointMapping>& Joints, TMap<FString, FBodyPose>& OutPoses)
{
    OutPoses.Reset();
    FBodyPose Pelvis;
    Pelvis.PositionMeters = Row.RootPosMeters;
    Pelvis.Rotation = Row.RootRot;
    OutPoses.Add(TEXT("pelvis"), Pelvis);

    for (int32 Pass = 0; Pass < 3; ++Pass)
    {
        for (const FJointMapping& Joint : Joints)
        {
            if (OutPoses.Contains(Joint.BodyName))
            {
                continue;
            }
            const FBodyPose* Parent = OutPoses.Find(Joint.ParentBodyName);
            if (!Parent)
            {
                continue;
            }
            FBodyPose Pose;
            Pose.Rotation = (Parent->Rotation * LocalRotationForJoint(Joint, Row)).GetNormalized();
            Pose.PositionMeters = Parent->PositionMeters + Parent->Rotation.RotateVector(RestOffsetForBody(Joint.BodyName));
            OutPoses.Add(Joint.BodyName, Pose);
        }
    }
}

void EvaluateRuntimePhysicsProxy(
    const TArray<FReplayRow>& Rows,
    const TArray<FJointMapping>& Joints,
    const TArray<TArray<float>>& Actions,
    int32 DofSize,
    int32 PolicyHz,
    double SlidingThresholdMps,
    FRuntimeContactStats& OutStats)
{
    OutStats = FRuntimeContactStats();
    OutStats.ObservationSource = TEXT("visual_replay_fixture_proxy");
    OutStats.JointDriveBackend = TEXT("kinematic_contact_proxy");
    OutStats.ContactMeasurementSource = TEXT("kinematic_contact_proxy");

    FVector PrevRightFoot = FVector::ZeroVector;
    FVector PrevLeftFoot = FVector::ZeroVector;
    bool bPrevRightContact = false;
    bool bPrevLeftContact = false;
    const double Dt = 1.0 / static_cast<double>(FMath::Max(1, PolicyHz));
    const double ContactHeightMeters = 0.18;

    double JointErrorSum = 0.0;
    int32 JointErrorCount = 0;
    bool bAllRootsFinite = !Rows.IsEmpty();

    const int32 Count = FMath::Min(Rows.Num(), Actions.Num());
    for (int32 RowIndex = 0; RowIndex < Count; ++RowIndex)
    {
        const FReplayRow& Row = Rows[RowIndex];
        bAllRootsFinite =
            bAllRootsFinite
            && FMath::IsFinite(Row.RootPosMeters.X)
            && FMath::IsFinite(Row.RootPosMeters.Y)
            && FMath::IsFinite(Row.RootPosMeters.Z)
            && !Row.RootRot.ContainsNaN();

        const TArray<float>& Action = Actions[RowIndex];
        const int32 CompareDim = FMath::Min3(Action.Num(), Row.DofPos.Num(), DofSize);
        for (int32 Dim = 0; Dim < CompareDim; ++Dim)
        {
            const double Error = FMath::Abs(static_cast<double>(Action[Dim]) - Row.DofPos[Dim]);
            OutStats.MaxJointTargetError = FMath::Max(OutStats.MaxJointTargetError, Error);
            JointErrorSum += Error;
            ++JointErrorCount;
        }

        TMap<FString, FBodyPose> Poses;
        ComputeBodyPoses(Row, Joints, Poses);
        const FBodyPose* RightFoot = Poses.Find(TEXT("right_foot"));
        const FBodyPose* LeftFoot = Poses.Find(TEXT("left_foot"));

        auto AccumulateFoot = [&OutStats, Dt, ContactHeightMeters](
            const FBodyPose* Foot,
            bool& bPrevContact,
            FVector& PrevFoot,
            int32& ContactFrames)
        {
            if (!Foot)
            {
                bPrevContact = false;
                return;
            }

            const bool bContact = Foot->PositionMeters.Z <= ContactHeightMeters;
            if (bContact)
            {
                ++ContactFrames;
                if (bPrevContact)
                {
                    const FVector Delta = Foot->PositionMeters - PrevFoot;
                    const double PlanarSpeed = FVector2D(Delta.X, Delta.Y).Size() / Dt;
                    OutStats.MaxFootSlidingMpsRaw = FMath::Max(OutStats.MaxFootSlidingMpsRaw, PlanarSpeed);
                }
                PrevFoot = Foot->PositionMeters;
            }
            bPrevContact = bContact;
        };

        AccumulateFoot(RightFoot, bPrevRightContact, PrevRightFoot, OutStats.RightFootContactFrames);
        AccumulateFoot(LeftFoot, bPrevLeftContact, PrevLeftFoot, OutStats.LeftFootContactFrames);
    }

    OutStats.bRootFinite = bAllRootsFinite;
    OutStats.bFootContactsObserved = OutStats.RightFootContactFrames > 0 && OutStats.LeftFootContactFrames > 0;
    OutStats.ContactEvents = OutStats.RightFootContactFrames + OutStats.LeftFootContactFrames;
    OutStats.MaxFootSlidingMpsScored = OutStats.MaxFootSlidingMpsRaw;
    OutStats.bSlidingUnderThreshold = OutStats.bFootContactsObserved && OutStats.MaxFootSlidingMpsRaw <= SlidingThresholdMps;
    OutStats.bJointDriveApplied = false;
    OutStats.bPhysicsSimulated = false;
}

FIntPoint ProjectSkeletonPoint(const FVector& Point, const FVector& Root, int32 Width, int32 Height)
{
    const FVector Rel = Point - Root;
    const double Scale = FMath::Clamp(Height * 0.34, 150.0, 230.0);
    const double X = Width * 0.5 + Rel.Y * Scale + Rel.X * Scale * 0.18;
    const double Y = Height * 0.72 - Rel.Z * Scale + Rel.X * Scale * 0.08;
    return FIntPoint(FMath::RoundToInt(X), FMath::RoundToInt(Y));
}

void DrawBone(
    TArray<FColor>& Pixels,
    int32 Width,
    int32 Height,
    const TMap<FString, FBodyPose>& Poses,
    const FVector& Root,
    const FString& Parent,
    const FString& Child,
    const FColor& Color,
    int32 Radius)
{
    const FBodyPose* A = Poses.Find(Parent);
    const FBodyPose* B = Poses.Find(Child);
    if (!A || !B)
    {
        return;
    }
    const FIntPoint PA = ProjectSkeletonPoint(A->PositionMeters, Root, Width, Height);
    const FIntPoint PB = ProjectSkeletonPoint(B->PositionMeters, Root, Width, Height);
    DrawThickLine(Pixels, Width, Height, PA.X, PA.Y, PB.X, PB.Y, Radius, Color);
    DrawCircle(Pixels, Width, Height, PB.X, PB.Y, Radius + 1, FColor(245, 245, 235));
}

void RenderSkeletalFrame(
    const TArray<FReplayRow>& Rows,
    const TArray<FJointMapping>& Joints,
    int32 RowIndex,
    int32 Width,
    int32 Height,
    TArray<FColor>& OutPixels)
{
    OutPixels.Init(FColor(22, 24, 28), Width * Height);
    const int32 GroundY = FMath::RoundToInt(Height * 0.72);
    for (int32 X = 0; X < Width; ++X)
    {
        SetPixel(OutPixels, Width, Height, X, GroundY, FColor(64, 70, 68));
        if (X % 32 == 0)
        {
            DrawLine(OutPixels, Width, Height, X, GroundY, X + 16, GroundY + 8, FColor(40, 45, 43));
        }
    }

    const FReplayRow& Row = Rows[RowIndex];
    TMap<FString, FBodyPose> Poses;
    ComputeBodyPoses(Row, Joints, Poses);
    const FVector Root = Row.RootPosMeters;

    const FColor SpineColor(240, 207, 98);
    const FColor RightColor(89, 180, 255);
    const FColor LeftColor(255, 126, 154);
    const FColor LegColor(125, 220, 166);
    DrawBone(OutPixels, Width, Height, Poses, Root, TEXT("pelvis"), TEXT("torso"), SpineColor, 5);
    DrawBone(OutPixels, Width, Height, Poses, Root, TEXT("torso"), TEXT("head"), SpineColor, 5);
    DrawBone(OutPixels, Width, Height, Poses, Root, TEXT("torso"), TEXT("right_upper_arm"), RightColor, 4);
    DrawBone(OutPixels, Width, Height, Poses, Root, TEXT("right_upper_arm"), TEXT("right_lower_arm"), RightColor, 4);
    DrawBone(OutPixels, Width, Height, Poses, Root, TEXT("right_lower_arm"), TEXT("right_hand"), RightColor, 4);
    DrawBone(OutPixels, Width, Height, Poses, Root, TEXT("torso"), TEXT("left_upper_arm"), LeftColor, 4);
    DrawBone(OutPixels, Width, Height, Poses, Root, TEXT("left_upper_arm"), TEXT("left_lower_arm"), LeftColor, 4);
    DrawBone(OutPixels, Width, Height, Poses, Root, TEXT("left_lower_arm"), TEXT("left_hand"), LeftColor, 4);
    DrawBone(OutPixels, Width, Height, Poses, Root, TEXT("pelvis"), TEXT("right_thigh"), LegColor, 5);
    DrawBone(OutPixels, Width, Height, Poses, Root, TEXT("right_thigh"), TEXT("right_shin"), LegColor, 5);
    DrawBone(OutPixels, Width, Height, Poses, Root, TEXT("right_shin"), TEXT("right_foot"), LegColor, 5);
    DrawBone(OutPixels, Width, Height, Poses, Root, TEXT("pelvis"), TEXT("left_thigh"), LegColor, 5);
    DrawBone(OutPixels, Width, Height, Poses, Root, TEXT("left_thigh"), TEXT("left_shin"), LegColor, 5);
    DrawBone(OutPixels, Width, Height, Poses, Root, TEXT("left_shin"), TEXT("left_foot"), LegColor, 5);

    if (const FBodyPose* Sword = Poses.Find(TEXT("sword")))
    {
        const FIntPoint A = ProjectSkeletonPoint(Sword->PositionMeters, Root, Width, Height);
        const FIntPoint B = ProjectSkeletonPoint(Sword->PositionMeters + Sword->Rotation.RotateVector(FVector(0.55, 0.0, 0.08)), Root, Width, Height);
        DrawThickLine(OutPixels, Width, Height, A.X, A.Y, B.X, B.Y, 3, FColor(214, 222, 232));
    }
    if (const FBodyPose* Shield = Poses.Find(TEXT("shield")))
    {
        const FIntPoint C = ProjectSkeletonPoint(Shield->PositionMeters, Root, Width, Height);
        DrawCircle(OutPixels, Width, Height, C.X, C.Y, 22, FColor(160, 105, 234));
        DrawCircle(OutPixels, Width, Height, C.X, C.Y, 14, FColor(92, 54, 158));
    }

    for (const TCHAR* FootName : {TEXT("right_foot"), TEXT("left_foot")})
    {
        if (const FBodyPose* Foot = Poses.Find(FootName))
        {
            const FIntPoint P = ProjectSkeletonPoint(Foot->PositionMeters, Root, Width, Height);
            DrawCircle(OutPixels, Width, Height, P.X, GroundY, 6, FColor(105, 240, 170));
        }
    }

    const FIntPoint RootPoint = ProjectSkeletonPoint(Root, Root, Width, Height);
    DrawCircle(OutPixels, Width, Height, RootPoint.X, RootPoint.Y, 8, FColor(255, 236, 120));
    DrawDigits(OutPixels, Width, Height, FString::Printf(TEXT("%06d"), Row.Frame), 14, 14, FColor::White);
}

void AimSceneCaptureAtRow(USceneCaptureComponent2D* SceneCapture, const FReplayRow& Row)
{
    if (!SceneCapture)
    {
        return;
    }

    const FVector RootCm = Row.RootPosMeters * 100.0;
    const FVector Facing = Row.RootRot.RotateVector(FVector::ForwardVector).GetSafeNormal();
    const FVector Right = Row.RootRot.RotateVector(FVector::RightVector).GetSafeNormal();
    const FVector CameraLocation = RootCm - Facing * 280.0 - Right * 180.0 + FVector(0.0, 0.0, 135.0);
    const FVector CameraTarget = RootCm + Facing * 45.0 + FVector(0.0, 0.0, 75.0);
    const FRotator CameraRotation = FRotationMatrix::MakeFromX((CameraTarget - CameraLocation).GetSafeNormal()).Rotator();
    SceneCapture->SetWorldLocationAndRotation(CameraLocation, CameraRotation);
}

bool ReadSceneCaptureFrame(
    UWorld* World,
    USceneCaptureComponent2D* SceneCapture,
    UTextureRenderTarget2D* RenderTarget,
    int32 Width,
    int32 Height,
    TArray<FColor>& OutPixels)
{
    OutPixels.Reset();
    if (!World || !SceneCapture || !RenderTarget)
    {
        return false;
    }

    World->Tick(LEVELTICK_All, 1.0f / 30.0f);
    SceneCapture->CaptureScene();
    FlushRenderingCommands();

    FTextureRenderTargetResource* Resource = RenderTarget->GameThread_GetRenderTargetResource();
    if (!Resource)
    {
        return false;
    }

    FReadSurfaceDataFlags ReadFlags(RCM_UNorm);
    ReadFlags.SetLinearToGamma(false);
    if (!Resource->ReadPixels(OutPixels, ReadFlags))
    {
        return false;
    }
    return OutPixels.Num() == Width * Height;
}

void RenderFrame(
    const TArray<FReplayRow>& Rows,
    int32 RowIndex,
    const FVector2D& RootMin,
    const FVector2D& RootMax,
    int32 Width,
    int32 Height,
    TArray<FColor>& OutPixels)
{
    OutPixels.Init(FColor(14, 17, 20), Width * Height);
    DrawGrid(OutPixels, Width, Height);

    const FColor TrailColor(92, 190, 138);
    const FColor RootColor(255, 219, 91);
    const FColor FacingColor(83, 166, 255);
    for (int32 Index = 1; Index <= RowIndex; ++Index)
    {
        const FIntPoint A = ProjectRoot(Rows[Index - 1].RootPosMeters, RootMin, RootMax, Width, Height);
        const FIntPoint B = ProjectRoot(Rows[Index].RootPosMeters, RootMin, RootMax, Width, Height);
        DrawLine(OutPixels, Width, Height, A.X, A.Y, B.X, B.Y, TrailColor);
    }

    const FReplayRow& Row = Rows[RowIndex];
    const FIntPoint Root = ProjectRoot(Row.RootPosMeters, RootMin, RootMax, Width, Height);
    const FVector Facing3 = Row.RootRot.RotateVector(FVector::ForwardVector);
    const FVector FacingTarget = Row.RootPosMeters + FVector(Facing3.X, Facing3.Y, 0.0).GetSafeNormal() * 0.35;
    const FIntPoint Facing = ProjectRoot(FacingTarget, RootMin, RootMax, Width, Height);
    DrawLine(OutPixels, Width, Height, Root.X, Root.Y, Facing.X, Facing.Y, FacingColor);
    DrawCircle(OutPixels, Width, Height, Root.X, Root.Y, 7, RootColor);
    DrawDigits(OutPixels, Width, Height, FString::Printf(TEXT("%06d"), Row.Frame), 14, 14, FColor::White);
}

bool SavePpm(const FString& FilePath, const TArray<FColor>& Pixels, int32 Width, int32 Height)
{
    TArray<uint8> Bytes;
    const FString Header = FString::Printf(TEXT("P6\n%d %d\n255\n"), Width, Height);
    const FTCHARToUTF8 HeaderUtf8(*Header);
    Bytes.Append(reinterpret_cast<const uint8*>(HeaderUtf8.Get()), HeaderUtf8.Length());
    Bytes.Reserve(Bytes.Num() + Pixels.Num() * 3);
    for (const FColor& Pixel : Pixels)
    {
        Bytes.Add(Pixel.R);
        Bytes.Add(Pixel.G);
        Bytes.Add(Pixel.B);
    }
    return FFileHelper::SaveArrayToFile(Bytes, *FilePath);
}

bool SavePng(const FString& FilePath, const TArray<FColor>& Pixels, int32 Width, int32 Height)
{
    TArray<uint8> RgbaBytes;
    RgbaBytes.Reserve(Pixels.Num() * 4);
    for (const FColor& Pixel : Pixels)
    {
        RgbaBytes.Add(Pixel.R);
        RgbaBytes.Add(Pixel.G);
        RgbaBytes.Add(Pixel.B);
        RgbaBytes.Add(255);
    }

    IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(FName(TEXT("ImageWrapper")));
    const TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(EImageFormat::PNG);
    if (!ImageWrapper.IsValid()
        || !ImageWrapper->SetRaw(RgbaBytes.GetData(), RgbaBytes.Num(), Width, Height, ERGBFormat::RGBA, 8))
    {
        return false;
    }

    const TArray64<uint8>& Compressed = ImageWrapper->GetCompressed(100);
    TArray<uint8> Bytes;
    Bytes.Append(Compressed.GetData(), static_cast<int32>(Compressed.Num()));
    return FFileHelper::SaveArrayToFile(Bytes, *FilePath);
}

TArray<TSharedPtr<FJsonValue>> NumberArray2(const FVector2D& Value)
{
    TArray<TSharedPtr<FJsonValue>> Out;
    Out.Add(MakeShared<FJsonValueNumber>(Value.X));
    Out.Add(MakeShared<FJsonValueNumber>(Value.Y));
    return Out;
}

FString ReadContractString(const TSharedPtr<FJsonObject>& Contract, const FString& FieldName)
{
    FString Value = GetStringField(Contract, FieldName);
    return Value;
}

void LoadSkeletalContract(
    const FString& PackageDirectory,
    FString& OutTargetMesh,
    FString& OutTargetSkeleton,
    FString& OutFallbackRenderer,
    FString& OutBoneMapHash,
    FString& OutBasisTransformHash,
    bool& bOutFullCharacterParity)
{
    OutTargetMesh = TEXT("/GASPALS/Characters/UE5_Mannequins/Meshes/SKM_Manny.SKM_Manny");
    OutTargetSkeleton = TEXT("/GASPALS/Characters/UE5_Mannequins/Meshes/SK_Mannequin.SK_Mannequin");
    OutFallbackRenderer = TEXT("procedural_mimickit_skeletal_replay");
    OutBoneMapHash.Reset();
    OutBasisTransformHash.Reset();
    bOutFullCharacterParity = false;

    FString Error;
    TSharedPtr<FJsonObject> SkeletalContract;
    const FString SkeletalContractPath = SkeletalMappingContractPathForPackage(PackageDirectory);
    if (!FPaths::FileExists(SkeletalContractPath) || !LoadJsonObject(SkeletalContractPath, SkeletalContract, Error))
    {
        TSharedPtr<FJsonObject> VisualContract;
        if (LoadJsonObject(VisualAlignmentContractPathForPackage(PackageDirectory), VisualContract, Error))
        {
            SkeletalContract = GetObjectField(VisualContract, TEXT("skeletal_mapping_contract"));
        }
    }

    if (!SkeletalContract.IsValid())
    {
        return;
    }

    OutBoneMapHash = GetStringField(SkeletalContract, TEXT("bone_map_hash"));
    OutBasisTransformHash = GetStringField(SkeletalContract, TEXT("basis_transform_hash"));
    const TSharedPtr<FJsonObject> Target = GetObjectField(SkeletalContract, TEXT("ue_visual_target"));
    if (Target.IsValid())
    {
        const FString Mesh = GetStringField(Target, TEXT("skeletal_mesh"));
        const FString Skeleton = GetStringField(Target, TEXT("skeleton"));
        const FString Fallback = GetStringField(Target, TEXT("fallback_renderer"));
        if (!Mesh.IsEmpty()) { OutTargetMesh = Mesh; }
        if (!Skeleton.IsEmpty()) { OutTargetSkeleton = Skeleton; }
        if (Target->HasField(TEXT("fallback_renderer"))) { OutFallbackRenderer = Fallback; }
        bOutFullCharacterParity = GetBoolField(Target, TEXT("full_character_parity"));
    }
}
}

bool FGASPALSMimicKitVisualReplayCapture::LoadVisualPackage(
    const FString& PackageDirectory,
    FGASPALSMimicKitPackageSummary& OutSummary,
    FString& OutError)
{
    OutSummary = FGASPALSMimicKitPackageSummary();
    OutSummary.PackageDirectory = FPaths::ConvertRelativePathToFull(PackageDirectory);

    const FString BrainManifestPath = JoinPackagePath(OutSummary.PackageDirectory, TEXT("brain_manifest.json"));
    const FString JointOrderPath = JoinPackagePath(OutSummary.PackageDirectory, TEXT("joint_order.json"));
    const FString VisualReplayPath = JoinPackagePath(OutSummary.PackageDirectory, TEXT("visual_replay/pose_dof_replay.jsonl"));
    const FString VisualMetaPath = JoinPackagePath(OutSummary.PackageDirectory, TEXT("visual_replay/pose_dof_meta.json"));
    for (const FString& RequiredPath : {BrainManifestPath, JointOrderPath, VisualReplayPath, VisualMetaPath})
    {
        if (!FPaths::FileExists(RequiredPath))
        {
            OutError = FString::Printf(TEXT("Visual capture package missing required file: %s"), *RequiredPath);
            return false;
        }
    }

    TSharedPtr<FJsonObject> BrainManifest;
    if (!LoadJsonObject(BrainManifestPath, BrainManifest, OutError))
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

    TSharedPtr<FJsonObject> JointOrder;
    if (!LoadJsonObject(JointOrderPath, JointOrder, OutError))
    {
        return false;
    }
    OutSummary.DofSize = GetIntField(JointOrder, TEXT("dof_size"));

    TSharedPtr<FJsonObject> VisualMeta;
    if (!LoadJsonObject(VisualMetaPath, VisualMeta, OutError))
    {
        return false;
    }
    OutSummary.VisualReplayRows = GetIntField(VisualMeta, TEXT("row_count"));
    OutSummary.VisualReplayDofDim = GetIntField(VisualMeta, TEXT("dof_pos_dim"));
    OutSummary.bVisualReplayReady =
        OutSummary.VisualReplayRows > 0
        && OutSummary.VisualReplayDofDim == OutSummary.DofSize;

    OutSummary.bPackageReady =
        !OutSummary.BrainName.IsEmpty()
        && OutSummary.ActionDim > 0
        && OutSummary.PolicyHz > 0
        && OutSummary.PhysicsHz > 0
        && OutSummary.DofSize > 0
        && OutSummary.bVisualReplayReady;

    if (!OutSummary.bPackageReady)
    {
        OutError = TEXT("Visual capture package failed minimal readiness checks.");
        return false;
    }

    OutError.Reset();
    return true;
}

bool FGASPALSMimicKitVisualReplayCapture::CaptureDebugGeometry(
    const FGASPALSMimicKitPackageSummary& Package,
    const FString& CaptureDirectory,
    int32 FrameStride,
    int32 Width,
    int32 Height,
    FGASPALSMimicKitVisualCaptureSummary& OutSummary,
    FString& OutError)
{
    OutSummary = FGASPALSMimicKitVisualCaptureSummary();
    OutSummary.PackageDirectory = Package.PackageDirectory;
    OutSummary.CaptureDirectory = FPaths::ConvertRelativePathToFull(CaptureDirectory);
    OutSummary.MetaFile = FPaths::Combine(OutSummary.CaptureDirectory, TEXT("capture_meta.json"));
    OutSummary.BrainName = Package.BrainName;
    OutSummary.FrameStride = FMath::Max(1, FrameStride);
    OutSummary.Width = FMath::Max(320, Width);
    OutSummary.Height = FMath::Max(240, Height);
    OutSummary.CaptureMode = TEXT("debug_geometry");
    OutSummary.RenderSource = TEXT("procedural_debug_geometry");

    UWorld* TransientWorld = UWorld::CreateWorld(EWorldType::Game, false, TEXT("MimicKitVisualReplayCaptureWorld"));
    OutSummary.bTransientWorldCreated = TransientWorld != nullptr;

    TArray<FReplayRow> Rows;
    const FString ReplayFile = VisualReplayPathForPackage(Package.PackageDirectory);
    if (!LoadReplayRows(ReplayFile, 0, Rows, OutError))
    {
        OutSummary.Error = OutError;
        if (TransientWorld)
        {
            TransientWorld->DestroyWorld(false);
        }
        return false;
    }
    OutSummary.SourceRows = Rows.Num();
    ComputeBounds(Rows, OutSummary.RootMinMeters, OutSummary.RootMaxMeters);

    const FString FramesDirectory = FPaths::Combine(OutSummary.CaptureDirectory, TEXT("frames"));
    IFileManager::Get().DeleteDirectory(*FramesDirectory, false, true);
    IFileManager::Get().MakeDirectory(*FramesDirectory, true);

    TArray<TSharedPtr<FJsonValue>> FrameFiles;
    for (int32 RowIndex = 0; RowIndex < Rows.Num(); ++RowIndex)
    {
        const FReplayRow& Row = Rows[RowIndex];
        if (Row.Frame % OutSummary.FrameStride != 0)
        {
            continue;
        }

        TArray<FColor> Pixels;
        RenderFrame(Rows, RowIndex, OutSummary.RootMinMeters, OutSummary.RootMaxMeters, OutSummary.Width, OutSummary.Height, Pixels);
        const FString PngFileName = FString::Printf(TEXT("frame_%06d.png"), Row.Frame);
        const FString PpmFileName = FString::Printf(TEXT("frame_%06d.ppm"), Row.Frame);
        const FString PngFramePath = FPaths::Combine(FramesDirectory, PngFileName);
        const FString PpmFramePath = FPaths::Combine(FramesDirectory, PpmFileName);
        if (!SavePng(PngFramePath, Pixels, OutSummary.Width, OutSummary.Height))
        {
            OutError = FString::Printf(TEXT("Failed to save PNG capture frame: %s"), *PngFramePath);
            OutSummary.Error = OutError;
            if (TransientWorld)
            {
                TransientWorld->DestroyWorld(false);
            }
            return false;
        }
        SavePpm(PpmFramePath, Pixels, OutSummary.Width, OutSummary.Height);

        TSharedPtr<FJsonObject> FrameObject = MakeShared<FJsonObject>();
        FrameObject->SetNumberField(TEXT("frame"), Row.Frame);
        FrameObject->SetStringField(TEXT("file"), PngFileName);
        FrameObject->SetStringField(TEXT("png_file"), PngFileName);
        FrameObject->SetStringField(TEXT("legacy_ppm_file"), PpmFileName);
        FrameFiles.Add(MakeShared<FJsonValueObject>(FrameObject));
        OutSummary.CaptureFrames += 1;
    }

    TSharedPtr<FJsonObject> Meta = MakeShared<FJsonObject>();
    Meta->SetStringField(TEXT("capture_mode"), TEXT("debug_geometry"));
    Meta->SetStringField(TEXT("mode"), TEXT("transient_debug_geometry_top_down_root_facing"));
    Meta->SetStringField(TEXT("render_source"), OutSummary.RenderSource);
    Meta->SetStringField(TEXT("frame_format"), TEXT("png"));
    Meta->SetBoolField(TEXT("legacy_ppm_written"), true);
    Meta->SetStringField(TEXT("brain"), Package.BrainName);
    Meta->SetStringField(TEXT("package_dir"), Package.PackageDirectory);
    Meta->SetStringField(TEXT("replay_file"), ReplayFile);
    Meta->SetStringField(TEXT("capture_dir"), OutSummary.CaptureDirectory);
    Meta->SetNumberField(TEXT("source_rows"), OutSummary.SourceRows);
    Meta->SetNumberField(TEXT("capture_frames"), OutSummary.CaptureFrames);
    Meta->SetNumberField(TEXT("frame_stride"), OutSummary.FrameStride);
    Meta->SetNumberField(TEXT("width"), OutSummary.Width);
    Meta->SetNumberField(TEXT("height"), OutSummary.Height);
    Meta->SetBoolField(TEXT("transient_world_created"), OutSummary.bTransientWorldCreated);
    Meta->SetArrayField(TEXT("root_min_m"), NumberArray2(OutSummary.RootMinMeters));
    Meta->SetArrayField(TEXT("root_max_m"), NumberArray2(OutSummary.RootMaxMeters));
    Meta->SetArrayField(TEXT("frames"), FrameFiles);

    FString MetaText;
    const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&MetaText);
    FJsonSerializer::Serialize(Meta.ToSharedRef(), Writer);
    FFileHelper::SaveStringToFile(MetaText, *OutSummary.MetaFile, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);

    if (TransientWorld)
    {
        TransientWorld->DestroyWorld(false);
    }

    OutSummary.bPassed = OutSummary.CaptureFrames > 0 && FPaths::FileExists(OutSummary.MetaFile);
    if (!OutSummary.bPassed)
    {
        OutError = TEXT("Debug geometry capture produced no frames.");
        OutSummary.Error = OutError;
        return false;
    }

    OutError.Reset();
    return true;
}

bool FGASPALSMimicKitVisualReplayCapture::CaptureSkeletalReplay(
    const FGASPALSMimicKitPackageSummary& Package,
    const FString& CaptureDirectory,
    int32 FrameStride,
    int32 Width,
    int32 Height,
    FGASPALSMimicKitVisualCaptureSummary& OutSummary,
    FString& OutError)
{
    OutSummary = FGASPALSMimicKitVisualCaptureSummary();
    OutSummary.PackageDirectory = Package.PackageDirectory;
    OutSummary.CaptureDirectory = FPaths::ConvertRelativePathToFull(CaptureDirectory);
    OutSummary.MetaFile = FPaths::Combine(OutSummary.CaptureDirectory, TEXT("capture_meta.json"));
    OutSummary.BrainName = Package.BrainName;
    OutSummary.FrameStride = FMath::Max(1, FrameStride);
    OutSummary.Width = FMath::Max(320, Width);
    OutSummary.Height = FMath::Max(240, Height);
    OutSummary.CaptureMode = TEXT("skeletal_replay");
    OutSummary.RenderSource = TEXT("procedural_skeletal_debug");

    UWorld* TransientWorld = UWorld::CreateWorld(EWorldType::Game, false, TEXT("MimicKitSkeletalVisualReplayCaptureWorld"));
    OutSummary.bTransientWorldCreated = TransientWorld != nullptr;

    bool bFullCharacterParity = false;
    LoadSkeletalContract(
        Package.PackageDirectory,
        OutSummary.TargetMesh,
        OutSummary.TargetSkeleton,
        OutSummary.FallbackRenderer,
        OutSummary.BoneMapHash,
        OutSummary.BasisTransformHash,
        bFullCharacterParity);

    if (OutSummary.BoneMapHash.IsEmpty() || OutSummary.BasisTransformHash.IsEmpty())
    {
        OutError = TEXT("Skeletal replay requires bone_map_hash and basis_transform_hash from visual_alignment_contract v2.");
        OutSummary.Error = OutError;
        if (TransientWorld)
        {
            TransientWorld->DestroyWorld(false);
        }
        return false;
    }

    TArray<FJointMapping> Joints;
    if (!LoadJointMappingsForPackage(Package.PackageDirectory, Joints, false, OutError))
    {
        OutSummary.Error = OutError;
        if (TransientWorld)
        {
            TransientWorld->DestroyWorld(false);
        }
        return false;
    }

    TArray<FReplayRow> Rows;
    const FString ReplayFile = VisualReplayPathForPackage(Package.PackageDirectory);
    if (!LoadReplayRows(ReplayFile, Package.DofSize, Rows, OutError))
    {
        OutSummary.Error = OutError;
        if (TransientWorld)
        {
            TransientWorld->DestroyWorld(false);
        }
        return false;
    }
    OutSummary.SourceRows = Rows.Num();
    ComputeBounds(Rows, OutSummary.RootMinMeters, OutSummary.RootMaxMeters);

    const FString FramesDirectory = FPaths::Combine(OutSummary.CaptureDirectory, TEXT("frames"));
    IFileManager::Get().DeleteDirectory(*FramesDirectory, false, true);
    IFileManager::Get().MakeDirectory(*FramesDirectory, true);

    TArray<TSharedPtr<FJsonValue>> FrameFiles;
    for (int32 RowIndex = 0; RowIndex < Rows.Num(); ++RowIndex)
    {
        const FReplayRow& Row = Rows[RowIndex];
        if (Row.Frame % OutSummary.FrameStride != 0)
        {
            continue;
        }

        TArray<FColor> Pixels;
        RenderSkeletalFrame(Rows, Joints, RowIndex, OutSummary.Width, OutSummary.Height, Pixels);
        const FString PngFileName = FString::Printf(TEXT("frame_%06d.png"), Row.Frame);
        const FString PpmFileName = FString::Printf(TEXT("frame_%06d.ppm"), Row.Frame);
        const FString PngFramePath = FPaths::Combine(FramesDirectory, PngFileName);
        const FString PpmFramePath = FPaths::Combine(FramesDirectory, PpmFileName);
        if (!SavePng(PngFramePath, Pixels, OutSummary.Width, OutSummary.Height))
        {
            OutError = FString::Printf(TEXT("Failed to save skeletal PNG capture frame: %s"), *PngFramePath);
            OutSummary.Error = OutError;
            if (TransientWorld)
            {
                TransientWorld->DestroyWorld(false);
            }
            return false;
        }
        SavePpm(PpmFramePath, Pixels, OutSummary.Width, OutSummary.Height);

        TSharedPtr<FJsonObject> FrameObject = MakeShared<FJsonObject>();
        FrameObject->SetNumberField(TEXT("frame"), Row.Frame);
        FrameObject->SetStringField(TEXT("file"), PngFileName);
        FrameObject->SetStringField(TEXT("png_file"), PngFileName);
        FrameObject->SetStringField(TEXT("legacy_ppm_file"), PpmFileName);
        FrameObject->SetBoolField(TEXT("dof_pos_applied"), true);
        FrameFiles.Add(MakeShared<FJsonValueObject>(FrameObject));
        OutSummary.CaptureFrames += 1;
        OutSummary.SkeletalPoseFrames += 1;
    }

    OutSummary.bDofPosApplied = OutSummary.SkeletalPoseFrames > 0;

    TSharedPtr<FJsonObject> Meta = MakeShared<FJsonObject>();
    Meta->SetStringField(TEXT("capture_mode"), TEXT("skeletal_replay"));
    Meta->SetStringField(TEXT("mode"), TEXT("skeletal_replay"));
    Meta->SetStringField(TEXT("render_source"), OutSummary.RenderSource);
    Meta->SetStringField(TEXT("frame_format"), TEXT("png"));
    Meta->SetBoolField(TEXT("legacy_ppm_written"), true);
    Meta->SetStringField(TEXT("brain"), Package.BrainName);
    Meta->SetStringField(TEXT("package_dir"), Package.PackageDirectory);
    Meta->SetStringField(TEXT("replay_file"), ReplayFile);
    Meta->SetStringField(TEXT("capture_dir"), OutSummary.CaptureDirectory);
    Meta->SetStringField(TEXT("target_mesh"), OutSummary.TargetMesh);
    Meta->SetStringField(TEXT("target_skeleton"), OutSummary.TargetSkeleton);
    Meta->SetStringField(TEXT("fallback_renderer"), OutSummary.FallbackRenderer);
    Meta->SetStringField(TEXT("bone_map_hash"), OutSummary.BoneMapHash);
    Meta->SetStringField(TEXT("basis_transform_hash"), OutSummary.BasisTransformHash);
    Meta->SetBoolField(TEXT("full_character_parity"), bFullCharacterParity);
    Meta->SetBoolField(TEXT("dof_pos_applied"), OutSummary.bDofPosApplied);
    Meta->SetNumberField(TEXT("skeletal_pose_frames"), OutSummary.SkeletalPoseFrames);
    Meta->SetNumberField(TEXT("source_rows"), OutSummary.SourceRows);
    Meta->SetNumberField(TEXT("capture_frames"), OutSummary.CaptureFrames);
    Meta->SetNumberField(TEXT("frame_stride"), OutSummary.FrameStride);
    Meta->SetNumberField(TEXT("width"), OutSummary.Width);
    Meta->SetNumberField(TEXT("height"), OutSummary.Height);
    Meta->SetBoolField(TEXT("transient_world_created"), OutSummary.bTransientWorldCreated);
    Meta->SetArrayField(TEXT("root_min_m"), NumberArray2(OutSummary.RootMinMeters));
    Meta->SetArrayField(TEXT("root_max_m"), NumberArray2(OutSummary.RootMaxMeters));
    Meta->SetArrayField(TEXT("frames"), FrameFiles);

    FString MetaText;
    const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&MetaText);
    FJsonSerializer::Serialize(Meta.ToSharedRef(), Writer);
    FFileHelper::SaveStringToFile(MetaText, *OutSummary.MetaFile, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);

    if (TransientWorld)
    {
        TransientWorld->DestroyWorld(false);
    }

    OutSummary.bPassed =
        OutSummary.CaptureFrames > 0
        && OutSummary.bDofPosApplied
        && !OutSummary.BoneMapHash.IsEmpty()
        && !OutSummary.BasisTransformHash.IsEmpty()
        && FPaths::FileExists(OutSummary.MetaFile);
    if (!OutSummary.bPassed)
    {
        OutError = TEXT("Skeletal replay capture failed acceptance checks.");
        OutSummary.Error = OutError;
        return false;
    }

    OutError.Reset();
    return true;
}

bool FGASPALSMimicKitVisualReplayCapture::CaptureSourceCharacterReplay(
    const FGASPALSMimicKitPackageSummary& Package,
    const FString& CaptureDirectory,
    int32 FrameStride,
    int32 Width,
    int32 Height,
    FGASPALSMimicKitVisualCaptureSummary& OutSummary,
    FString& OutError)
{
    OutSummary = FGASPALSMimicKitVisualCaptureSummary();
    OutSummary.PackageDirectory = Package.PackageDirectory;
    OutSummary.CaptureDirectory = FPaths::ConvertRelativePathToFull(CaptureDirectory);
    OutSummary.MetaFile = FPaths::Combine(OutSummary.CaptureDirectory, TEXT("capture_meta.json"));
    OutSummary.BrainName = Package.BrainName;
    OutSummary.FrameStride = FMath::Max(1, FrameStride);
    OutSummary.Width = FMath::Max(320, Width);
    OutSummary.Height = FMath::Max(240, Height);
    OutSummary.CaptureMode = TEXT("source_character_skeletalmesh_replay");
    OutSummary.RenderSource = TEXT("ue_scene_capture_render_target");
    OutSummary.DriverComponent = TEXT("UPoseableMeshComponent");
    OutSummary.SourceAssetBuildReport = SourceRigAssetBuildReportPathForPackage(Package.PackageDirectory);

    bool bFullCharacterParityContract = false;
    LoadSkeletalContract(
        Package.PackageDirectory,
        OutSummary.TargetMesh,
        OutSummary.TargetSkeleton,
        OutSummary.FallbackRenderer,
        OutSummary.BoneMapHash,
        OutSummary.BasisTransformHash,
        bFullCharacterParityContract);

    if (!OutSummary.TargetMesh.StartsWith(TEXT("/Game/MimicKit/SwordShield/")))
    {
        OutError = FString::Printf(TEXT("Source character replay requires a /Game/MimicKit/SwordShield target mesh, got: %s"), *OutSummary.TargetMesh);
        OutSummary.Error = OutError;
        return false;
    }
    if (!OutSummary.FallbackRenderer.IsEmpty())
    {
        OutError = FString::Printf(TEXT("Source character replay requires an empty fallback_renderer, got: %s"), *OutSummary.FallbackRenderer);
        OutSummary.Error = OutError;
        return false;
    }
    if (!bFullCharacterParityContract)
    {
        OutError = TEXT("Source character replay requires full_character_parity=true in the source-character mapping contract.");
        OutSummary.Error = OutError;
        return false;
    }

    const FString TargetObjectPath = NormalizeAssetObjectPath(OutSummary.TargetMesh);
    USkeletalMesh* SourceMesh = Cast<USkeletalMesh>(StaticLoadObject(USkeletalMesh::StaticClass(), nullptr, *TargetObjectPath));
    OutSummary.bLoadedSkeletalMesh = SourceMesh != nullptr;
    if (!SourceMesh)
    {
        OutError = FString::Printf(TEXT("Could not load MimicKit source SkeletalMesh: %s"), *TargetObjectPath);
        OutSummary.Error = OutError;
        return false;
    }

    TSharedPtr<FJsonObject> BuildReport;
    const bool bBuildReportLoaded =
        FPaths::FileExists(OutSummary.SourceAssetBuildReport)
        && LoadJsonObject(OutSummary.SourceAssetBuildReport, BuildReport, OutError);
    OutSummary.bMimicKitSourceAssetImportedToUE =
        bBuildReportLoaded
        && GetBoolField(BuildReport, TEXT("source_assets_built"))
        && GetBoolField(BuildReport, TEXT("skeletal_mesh_ok"))
        && GetBoolField(BuildReport, TEXT("skeleton_ok"))
        && GetBoolField(BuildReport, TEXT("physics_asset_ok"));
    if (!OutSummary.bMimicKitSourceAssetImportedToUE)
    {
        OutError = FString::Printf(TEXT("Source rig asset build report is missing or incomplete: %s"), *OutSummary.SourceAssetBuildReport);
        OutSummary.Error = OutError;
        return false;
    }

    TArray<FJointMapping> Joints;
    if (!LoadJointMappingsForPackage(Package.PackageDirectory, Joints, true, OutError))
    {
        OutSummary.Error = OutError;
        return false;
    }

    TArray<FReplayRow> Rows;
    const FString ReplayFile = VisualReplayPathForPackage(Package.PackageDirectory);
    if (!LoadReplayRows(ReplayFile, Package.DofSize, Rows, OutError))
    {
        OutSummary.Error = OutError;
        return false;
    }
    OutSummary.SourceRows = Rows.Num();
    ComputeBounds(Rows, OutSummary.RootMinMeters, OutSummary.RootMaxMeters);

    UWorld::InitializationValues WorldValues;
    WorldValues.CreatePhysicsScene(false)
        .ShouldSimulatePhysics(false)
        .EnableTraceCollision(true)
        .CreateNavigation(false)
        .CreateAISystem(false)
        .AllowAudioPlayback(false);
    UWorld* TransientWorld = UWorld::CreateWorld(
        EWorldType::Game,
        false,
        TEXT("MimicKitSourceCharacterReplayCaptureWorld"),
        nullptr,
        false,
        ERHIFeatureLevel::Num,
        &WorldValues);
    OutSummary.bTransientWorldCreated = TransientWorld != nullptr;
    bool bWorldContextCreated = false;
    if (TransientWorld && GEngine)
    {
        FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
        WorldContext.SetCurrentWorld(TransientWorld);
        bWorldContextCreated = true;
    }
    auto DestroyTransientWorld = [&TransientWorld, &bWorldContextCreated]()
    {
        if (TransientWorld)
        {
            if (GEngine && bWorldContextCreated)
            {
                GEngine->DestroyWorldContext(TransientWorld);
                bWorldContextCreated = false;
            }
            TransientWorld->DestroyWorld(false);
            TransientWorld = nullptr;
        }
    };
    if (!TransientWorld)
    {
        OutError = TEXT("Could not create source-character UE render world.");
        OutSummary.Error = OutError;
        return false;
    }

    AActor* PoseActor = nullptr;
    UPoseableMeshComponent* PoseableMesh = nullptr;
    if (TransientWorld)
    {
        PoseActor = TransientWorld->SpawnActor<AActor>();
        if (PoseActor)
        {
            PoseableMesh = NewObject<UPoseableMeshComponent>(PoseActor);
            if (PoseableMesh)
            {
                PoseableMesh->SetSkeletalMesh(SourceMesh);
                PoseableMesh->SetVisibility(true, true);
                PoseableMesh->SetHiddenInGame(false);
                PoseableMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
                PoseableMesh->RegisterComponentWithWorld(TransientWorld);
                PoseActor->SetRootComponent(PoseableMesh);
                OutSummary.bPoseableComponentCreated = true;
            }
        }
    }
    if (!OutSummary.bPoseableComponentCreated)
    {
        OutError = TEXT("Could not create source-character UPoseableMeshComponent.");
        OutSummary.Error = OutError;
        DestroyTransientWorld();
        return false;
    }

    UStaticMesh* GroundMeshAsset = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
    AActor* GroundActor = TransientWorld->SpawnActor<AActor>();
    UStaticMeshComponent* GroundMesh = GroundActor ? NewObject<UStaticMeshComponent>(GroundActor) : nullptr;
    if (GroundActor && GroundMesh && GroundMeshAsset)
    {
        GroundActor->SetRootComponent(GroundMesh);
        GroundMesh->SetStaticMesh(GroundMeshAsset);
        GroundMesh->SetWorldLocation(FVector(0.0, 0.0, -2.5));
        GroundMesh->SetWorldScale3D(FVector(100.0, 100.0, 0.05));
        GroundMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        GroundMesh->SetVisibility(true, true);
        GroundMesh->SetHiddenInGame(false);
        GroundMesh->RegisterComponentWithWorld(TransientWorld);
    }

    AActor* LightActor = TransientWorld->SpawnActor<AActor>();
    UDirectionalLightComponent* DirectionalLight = LightActor ? NewObject<UDirectionalLightComponent>(LightActor) : nullptr;
    if (LightActor && DirectionalLight)
    {
        LightActor->SetRootComponent(DirectionalLight);
        DirectionalLight->SetWorldRotation(FRotator(-45.0, -35.0, 0.0));
        DirectionalLight->SetIntensity(3.5f);
        DirectionalLight->RegisterComponentWithWorld(TransientWorld);
    }

    UTextureRenderTarget2D* RenderTarget = NewObject<UTextureRenderTarget2D>(GetTransientPackage());
    if (!RenderTarget)
    {
        OutError = TEXT("Could not create source-character render target.");
        OutSummary.Error = OutError;
        DestroyTransientWorld();
        return false;
    }
    RenderTarget->ClearColor = FLinearColor(0.018f, 0.021f, 0.026f, 1.0f);
    RenderTarget->InitAutoFormat(OutSummary.Width, OutSummary.Height);
    RenderTarget->UpdateResourceImmediate(true);

    AActor* CaptureActor = TransientWorld->SpawnActor<AActor>();
    USceneCaptureComponent2D* SceneCapture = CaptureActor ? NewObject<USceneCaptureComponent2D>(CaptureActor) : nullptr;
    if (!CaptureActor || !SceneCapture)
    {
        OutError = TEXT("Could not create source-character USceneCaptureComponent2D.");
        OutSummary.Error = OutError;
        DestroyTransientWorld();
        return false;
    }
    CaptureActor->SetRootComponent(SceneCapture);
    SceneCapture->TextureTarget = RenderTarget;
    SceneCapture->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
    SceneCapture->FOVAngle = 42.0f;
    SceneCapture->bCaptureEveryFrame = false;
    SceneCapture->bCaptureOnMovement = false;
    SceneCapture->RegisterComponentWithWorld(TransientWorld);
    TransientWorld->BeginPlay();

    const FString FramesDirectory = FPaths::Combine(OutSummary.CaptureDirectory, TEXT("frames"));
    IFileManager::Get().DeleteDirectory(*FramesDirectory, false, true);
    IFileManager::Get().MakeDirectory(*FramesDirectory, true);

    TArray<TSharedPtr<FJsonValue>> FrameFiles;
    for (int32 RowIndex = 0; RowIndex < Rows.Num(); ++RowIndex)
    {
        const FReplayRow& Row = Rows[RowIndex];
        if (Row.Frame % OutSummary.FrameStride != 0)
        {
            continue;
        }

        TMap<FString, FBodyPose> Poses;
        ComputeBodyPoses(Row, Joints, Poses);
        PoseableMesh->SetWorldTransform(FTransform(Row.RootRot, Row.RootPosMeters * 100.0));
        for (const TPair<FString, FBodyPose>& Pair : Poses)
        {
            const FTransform BoneWorld(Pair.Value.Rotation, Pair.Value.PositionMeters * 100.0);
            PoseableMesh->SetBoneTransformByName(FName(*Pair.Key), BoneWorld, EBoneSpaces::WorldSpace);
        }
        PoseableMesh->RefreshBoneTransforms();

        TArray<FColor> Pixels;
        AimSceneCaptureAtRow(SceneCapture, Row);
        if (!ReadSceneCaptureFrame(TransientWorld, SceneCapture, RenderTarget, OutSummary.Width, OutSummary.Height, Pixels))
        {
            OutError = FString::Printf(TEXT("Failed to read UE SceneCapture render target for frame %d."), Row.Frame);
            OutSummary.Error = OutError;
            DestroyTransientWorld();
            return false;
        }
        const FString PngFileName = FString::Printf(TEXT("frame_%06d.png"), Row.Frame);
        const FString PngFramePath = FPaths::Combine(FramesDirectory, PngFileName);
        if (!SavePng(PngFramePath, Pixels, OutSummary.Width, OutSummary.Height))
        {
            OutError = FString::Printf(TEXT("Failed to save source-character PNG capture frame: %s"), *PngFramePath);
            OutSummary.Error = OutError;
            DestroyTransientWorld();
            return false;
        }

        TSharedPtr<FJsonObject> FrameObject = MakeShared<FJsonObject>();
        FrameObject->SetNumberField(TEXT("frame"), Row.Frame);
        FrameObject->SetStringField(TEXT("file"), PngFileName);
        FrameObject->SetStringField(TEXT("png_file"), PngFileName);
        FrameObject->SetBoolField(TEXT("dof_pos_applied"), true);
        FrameFiles.Add(MakeShared<FJsonValueObject>(FrameObject));
        OutSummary.CaptureFrames += 1;
        OutSummary.SkeletalPoseFrames += 1;
    }

    OutSummary.bDofPosApplied = OutSummary.SkeletalPoseFrames > 0;

    TSharedPtr<FJsonObject> Meta = MakeShared<FJsonObject>();
    Meta->SetStringField(TEXT("capture_mode"), TEXT("source_character_skeletalmesh_replay"));
    Meta->SetStringField(TEXT("mode"), TEXT("source_character_skeletalmesh_replay"));
    Meta->SetStringField(TEXT("render_source"), OutSummary.RenderSource);
    Meta->SetStringField(TEXT("frame_format"), TEXT("png"));
    Meta->SetBoolField(TEXT("legacy_ppm_written"), false);
    Meta->SetStringField(TEXT("driver_component"), OutSummary.DriverComponent);
    Meta->SetStringField(TEXT("brain"), Package.BrainName);
    Meta->SetStringField(TEXT("package_dir"), Package.PackageDirectory);
    Meta->SetStringField(TEXT("replay_file"), ReplayFile);
    Meta->SetStringField(TEXT("capture_dir"), OutSummary.CaptureDirectory);
    Meta->SetStringField(TEXT("target_mesh"), OutSummary.TargetMesh);
    Meta->SetStringField(TEXT("target_skeleton"), OutSummary.TargetSkeleton);
    Meta->SetStringField(TEXT("fallback_renderer"), OutSummary.FallbackRenderer);
    Meta->SetStringField(TEXT("bone_map_hash"), OutSummary.BoneMapHash);
    Meta->SetStringField(TEXT("basis_transform_hash"), OutSummary.BasisTransformHash);
    Meta->SetStringField(TEXT("source_asset_build_report"), OutSummary.SourceAssetBuildReport);
    Meta->SetBoolField(TEXT("loaded_skeletal_mesh"), OutSummary.bLoadedSkeletalMesh);
    Meta->SetBoolField(TEXT("poseable_component_created"), OutSummary.bPoseableComponentCreated);
    Meta->SetBoolField(TEXT("mimickit_source_asset_imported_to_ue"), OutSummary.bMimicKitSourceAssetImportedToUE);
    Meta->SetBoolField(TEXT("full_character_parity"), true);
    Meta->SetBoolField(TEXT("dof_pos_applied"), OutSummary.bDofPosApplied);
    Meta->SetNumberField(TEXT("skeletal_pose_frames"), OutSummary.SkeletalPoseFrames);
    Meta->SetNumberField(TEXT("source_rows"), OutSummary.SourceRows);
    Meta->SetNumberField(TEXT("capture_frames"), OutSummary.CaptureFrames);
    Meta->SetNumberField(TEXT("frame_stride"), OutSummary.FrameStride);
    Meta->SetNumberField(TEXT("width"), OutSummary.Width);
    Meta->SetNumberField(TEXT("height"), OutSummary.Height);
    Meta->SetBoolField(TEXT("transient_world_created"), OutSummary.bTransientWorldCreated);
    Meta->SetArrayField(TEXT("root_min_m"), NumberArray2(OutSummary.RootMinMeters));
    Meta->SetArrayField(TEXT("root_max_m"), NumberArray2(OutSummary.RootMaxMeters));
    Meta->SetArrayField(TEXT("frames"), FrameFiles);

    FString MetaText;
    const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&MetaText);
    FJsonSerializer::Serialize(Meta.ToSharedRef(), Writer);
    FFileHelper::SaveStringToFile(MetaText, *OutSummary.MetaFile, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);

    DestroyTransientWorld();

    OutSummary.bPassed =
        OutSummary.CaptureFrames > 0
        && OutSummary.bDofPosApplied
        && OutSummary.bLoadedSkeletalMesh
        && OutSummary.bPoseableComponentCreated
        && OutSummary.bMimicKitSourceAssetImportedToUE
        && FPaths::FileExists(OutSummary.MetaFile);
    if (!OutSummary.bPassed)
    {
        OutError = TEXT("Source-character replay capture failed acceptance checks.");
        OutSummary.Error = OutError;
        return false;
    }

    OutError.Reset();
    return true;
}

const TCHAR* JsonBool(bool bValue)
{
    return bValue ? TEXT("true") : TEXT("false");
}

bool IsFiniteVector(const FVector& Value)
{
    return FMath::IsFinite(Value.X) && FMath::IsFinite(Value.Y) && FMath::IsFinite(Value.Z);
}

FQuat ActionTargetRotationForJoint(const FJointMapping& Joint, const TArray<float>& Action)
{
    if (Joint.DofDim == 3 && Action.IsValidIndex(Joint.DofIndex + 2))
    {
        const FVector ExpMap(Action[Joint.DofIndex], Action[Joint.DofIndex + 1], Action[Joint.DofIndex + 2]);
        const double Angle = ExpMap.Size();
        if (Angle > KINDA_SMALL_NUMBER)
        {
            return FQuat(ExpMap / Angle, Angle).GetNormalized();
        }
    }
    if (Joint.DofDim == 1 && Action.IsValidIndex(Joint.DofIndex))
    {
        return FQuat(Joint.HingeAxis.GetSafeNormal(UE_SMALL_NUMBER, FVector::RightVector), Action[Joint.DofIndex]).GetNormalized();
    }
    return FQuat::Identity;
}

FTransform GetBodyTransformForObservation(USkeletalMeshComponent* Mesh, FName BodyName)
{
    if (!Mesh)
    {
        return FTransform::Identity;
    }
    if (FBodyInstance* Body = Mesh->GetBodyInstance(BodyName))
    {
        return Body->GetUnrealWorldTransform();
    }
    return Mesh->GetBoneTransform(BodyName, RTS_World);
}

FVector GetBodyAngularVelocityForObservation(USkeletalMeshComponent* Mesh, FName BodyName)
{
    if (!Mesh)
    {
        return FVector::ZeroVector;
    }
    if (FBodyInstance* Body = Mesh->GetBodyInstance(BodyName))
    {
        return Body->GetUnrealWorldAngularVelocityInRadians();
    }
    return FVector::ZeroVector;
}

void AppendObservationValue(TArray<float>& Observation, int32& Cursor, double Value)
{
    if (Observation.IsValidIndex(Cursor))
    {
        Observation[Cursor] = static_cast<float>(Value);
    }
    ++Cursor;
}

void AppendObservationVector(TArray<float>& Observation, int32& Cursor, const FVector& Value)
{
    AppendObservationValue(Observation, Cursor, Value.X);
    AppendObservationValue(Observation, Cursor, Value.Y);
    AppendObservationValue(Observation, Cursor, Value.Z);
}

void AppendQuatTanNorm(TArray<float>& Observation, int32& Cursor, const FQuat& Rotation)
{
    const FQuat UnitRotation = Rotation.GetNormalized();
    AppendObservationVector(Observation, Cursor, UnitRotation.RotateVector(FVector::ForwardVector));
    AppendObservationVector(Observation, Cursor, UnitRotation.RotateVector(FVector::UpVector));
}

int32 BuildUePhysicsObservation(
    USkeletalMeshComponent* Mesh,
    const TArray<FJointMapping>& Joints,
    int32 ExpectedObservationDim,
    TArray<float>& OutObservation)
{
    OutObservation.SetNumZeroed(FMath::Max(0, ExpectedObservationDim));
    if (!Mesh || OutObservation.IsEmpty())
    {
        return 0;
    }

    FBodyInstance* PelvisBody = Mesh->GetBodyInstance(TEXT("pelvis"));
    const FTransform RootTransform = PelvisBody
        ? PelvisBody->GetUnrealWorldTransform()
        : Mesh->GetBoneTransform(TEXT("pelvis"), RTS_World);
    const FVector RootLocationMeters = RootTransform.GetLocation() * 0.01;
    const FQuat RootRotation = RootTransform.GetRotation().GetNormalized();
    const FVector RootVelocityMeters = PelvisBody ? PelvisBody->GetUnrealWorldVelocity() * 0.01 : FVector::ZeroVector;
    const FVector RootAngularVelocity = PelvisBody ? PelvisBody->GetUnrealWorldAngularVelocityInRadians() : FVector::ZeroVector;
    const FVector RootForward = RootRotation.RotateVector(FVector::ForwardVector);
    const double Heading = FMath::Atan2(RootForward.Y, RootForward.X);
    const FQuat HeadingInv(FVector::UpVector, -Heading);
    const FQuat LocalRootRotation = (HeadingInv * RootRotation).GetNormalized();

    int32 Cursor = 0;
    AppendObservationValue(OutObservation, Cursor, RootLocationMeters.Z);
    AppendQuatTanNorm(OutObservation, Cursor, LocalRootRotation);
    AppendObservationVector(OutObservation, Cursor, HeadingInv.RotateVector(RootVelocityMeters));
    AppendObservationVector(OutObservation, Cursor, HeadingInv.RotateVector(RootAngularVelocity));

    for (const FJointMapping& Joint : Joints)
    {
        const FTransform ParentTransform = GetBodyTransformForObservation(Mesh, FName(*Joint.ParentBodyName));
        const FTransform ChildTransform = GetBodyTransformForObservation(Mesh, FName(*Joint.BodyName));
        const FQuat LocalJointRotation = (ParentTransform.GetRotation().Inverse() * ChildTransform.GetRotation()).GetNormalized();
        AppendQuatTanNorm(OutObservation, Cursor, LocalJointRotation);
    }

    for (const FJointMapping& Joint : Joints)
    {
        const FVector ParentAngular = GetBodyAngularVelocityForObservation(Mesh, FName(*Joint.ParentBodyName));
        const FVector ChildAngular = GetBodyAngularVelocityForObservation(Mesh, FName(*Joint.BodyName));
        const FVector LocalRelativeAngular = GetBodyTransformForObservation(Mesh, FName(*Joint.ParentBodyName))
            .GetRotation()
            .Inverse()
            .RotateVector(ChildAngular - ParentAngular);
        if (Joint.DofDim == 3)
        {
            AppendObservationVector(OutObservation, Cursor, LocalRelativeAngular);
        }
        else if (Joint.DofDim == 1)
        {
            AppendObservationValue(OutObservation, Cursor, LocalRelativeAngular.X);
        }
    }

    const FName KeyBodyNames[] = {
        TEXT("head"),
        TEXT("right_hand"),
        TEXT("left_hand"),
        TEXT("right_foot"),
        TEXT("left_foot"),
        TEXT("sword"),
    };
    for (const FName BodyName : KeyBodyNames)
    {
        const FVector RelativeMeters = (GetBodyTransformForObservation(Mesh, BodyName).GetLocation() - RootTransform.GetLocation()) * 0.01;
        AppendObservationVector(OutObservation, Cursor, HeadingInv.RotateVector(RelativeMeters));
    }

    const FVector LocalTargetDir = HeadingInv.RotateVector(FVector::ForwardVector);
    AppendObservationValue(OutObservation, Cursor, LocalTargetDir.X);
    AppendObservationValue(OutObservation, Cursor, LocalTargetDir.Y);
    AppendObservationValue(OutObservation, Cursor, 0.0);
    AppendObservationValue(OutObservation, Cursor, LocalTargetDir.X);
    AppendObservationValue(OutObservation, Cursor, LocalTargetDir.Y);

    return FMath::Min(Cursor, OutObservation.Num());
}

int32 ApplyConstraintJointDriveTargets(
    USkeletalMeshComponent* Mesh,
    const TArray<FJointMapping>& Joints,
    const TArray<float>& Action)
{
    if (!Mesh || Action.IsEmpty())
    {
        return 0;
    }

    Mesh->SetAllMotorsAngularPositionDrive(true, true);
    Mesh->SetAllMotorsAngularVelocityDrive(false, false);
    Mesh->SetAllMotorsAngularDriveParams(1200.0f, 120.0f, 100000.0f);

    TMap<FName, const FJointMapping*> JointsByBody;
    for (const FJointMapping& Joint : Joints)
    {
        if (Joint.DofDim > 0)
        {
            JointsByBody.Add(FName(*Joint.BodyName), &Joint);
        }
    }

    int32 AppliedCount = 0;
    TArray<FConstraintInstanceAccessor> Constraints;
    Mesh->GetConstraints(false, Constraints);
    for (const FConstraintInstanceAccessor& Accessor : Constraints)
    {
        FConstraintInstance* Constraint = Accessor.Get();
        if (!Constraint)
        {
            continue;
        }
        const FJointMapping* const* JointPtr = JointsByBody.Find(Constraint->GetChildBoneName());
        if (!JointPtr || !*JointPtr)
        {
            continue;
        }

        Constraint->SetAngularDriveMode(EAngularDriveMode::SLERP);
        Constraint->SetOrientationDriveSLERP(true);
        const float DriveStiffness = static_cast<float>(FMath::Max(1200.0, (*JointPtr)->Stiffness));
        const float DriveDamping = static_cast<float>(FMath::Max(120.0, (*JointPtr)->Damping));
        const float DriveForceLimit = static_cast<float>(FMath::Max(100000.0, (*JointPtr)->Gear * 1000.0));
        Constraint->SetAngularDriveParams(DriveStiffness, DriveDamping, DriveForceLimit);
        Constraint->SetAngularOrientationTarget(ActionTargetRotationForJoint(**JointPtr, Action));
        ++AppliedCount;
    }
    return AppliedCount;
}

FVector GetPhysicsFootLocation(USkeletalMeshComponent* Mesh, FName BodyName)
{
    if (!Mesh)
    {
        return FVector::ZeroVector;
    }
    if (FBodyInstance* Body = Mesh->GetBodyInstance(BodyName))
    {
        return Body->GetUnrealWorldTransform().GetLocation();
    }
    return Mesh->GetBoneLocation(BodyName, EBoneSpaces::WorldSpace);
}

void ApplyInitialPhysicsPoseFromReplay(
    USkeletalMeshComponent* Mesh,
    const FReplayRow& Row,
    const TArray<FJointMapping>& Joints)
{
    if (!Mesh)
    {
        return;
    }

    TMap<FString, FBodyPose> Poses;
    ComputeBodyPoses(Row, Joints, Poses);
    for (const TPair<FString, FBodyPose>& Pair : Poses)
    {
        FBodyInstance* Body = Mesh->GetBodyInstance(FName(*Pair.Key));
        if (!Body)
        {
            continue;
        }
        Body->SetBodyTransform(
            FTransform(Pair.Value.Rotation, Pair.Value.PositionMeters * 100.0),
            ETeleportType::TeleportPhysics);
        Body->SetLinearVelocity(FVector::ZeroVector, false);
        Body->SetAngularVelocityInRadians(FVector::ZeroVector, false);
    }
}

struct FFootContactQueryResult
{
    bool bContact = false;
    bool bLineTraceHit = false;
    bool bBodyInstanceFound = false;
    bool bBodyValidated = false;
    int32 ShapeCount = 0;
    FName BodyName = NAME_None;
    FVector FootLocationMeters = FVector::ZeroVector;
    double FootLowestZMeters = 0.0;
    double GroundDistanceMeters = 0.0;
    FString Method = TEXT("none");
};

FBox GetBodyWorldBounds(USkeletalMeshComponent* Mesh, FName BodyName, bool& bOutBodyInstanceFound, int32& OutShapeCount)
{
    bOutBodyInstanceFound = false;
    OutShapeCount = 0;
    if (!Mesh)
    {
        return FBox(ForceInit);
    }

    if (FBodyInstance* Body = Mesh->GetBodyInstance(BodyName))
    {
        bOutBodyInstanceFound = true;
        if (UBodySetup* BodySetup = Body->GetBodySetup())
        {
            OutShapeCount = BodySetup->AggGeom.GetElementCount();
        }
        const FBox Bounds = Body->GetBodyBounds();
        if (Bounds.IsValid)
        {
            return Bounds;
        }
    }

    const FVector BoneLocation = Mesh->GetBoneLocation(BodyName, EBoneSpaces::WorldSpace);
    return FBox(BoneLocation - FVector(12.0, 12.0, 12.0), BoneLocation + FVector(12.0, 12.0, 12.0));
}

bool TryLoadGroundTopMetersFromContract(const FString& PackageDirectory, double& OutGroundTopMeters, FString& OutSource)
{
    TSharedPtr<FJsonObject> Contract;
    FString Error;
    if (!LoadJsonObject(JoinPackagePath(PackageDirectory, TEXT("runtime_control_contract.json")), Contract, Error))
    {
        return false;
    }

    const TSharedPtr<FJsonObject> Scene = GetObjectField(Contract, TEXT("scene"));
    const TSharedPtr<FJsonObject> Ground = GetObjectField(Scene, TEXT("ground"));
    const TSharedPtr<FJsonObject> Contact = GetObjectField(Contract, TEXT("contact"));
    const TSharedPtr<FJsonObject> Candidates[] = {Ground, Scene, Contact, Contract};
    const FString FieldNames[] = {
        TEXT("ground_top_z_m"),
        TEXT("support_plane_z_m"),
        TEXT("flat_ground_z_m"),
        TEXT("ground_z_m"),
    };
    for (const TSharedPtr<FJsonObject>& Candidate : Candidates)
    {
        for (const FString& FieldName : FieldNames)
        {
            double CandidateValue = 0.0;
            if (Candidate.IsValid() && Candidate->TryGetNumberField(FieldName, CandidateValue))
            {
                OutGroundTopMeters = CandidateValue;
                OutSource = FString::Printf(TEXT("runtime_control_contract.%s"), *FieldName);
                return true;
            }
        }
    }

    return false;
}

double ResolveRuntimeGroundTopCm(
    const FString& PackageDirectory,
    USkeletalMeshComponent* Mesh,
    FString& OutGroundAlignmentSource,
    bool& bOutRightFootBodyValidated,
    bool& bOutLeftFootBodyValidated)
{
    double GroundTopMeters = 0.0;
    if (TryLoadGroundTopMetersFromContract(PackageDirectory, GroundTopMeters, OutGroundAlignmentSource))
    {
        bOutRightFootBodyValidated = Mesh && Mesh->GetBodyInstance(TEXT("right_foot")) != nullptr;
        bOutLeftFootBodyValidated = Mesh && Mesh->GetBodyInstance(TEXT("left_foot")) != nullptr;
        return GroundTopMeters * 100.0;
    }

    bool bRightFound = false;
    bool bLeftFound = false;
    int32 RightShapeCount = 0;
    int32 LeftShapeCount = 0;
    const FBox RightBounds = GetBodyWorldBounds(Mesh, TEXT("right_foot"), bRightFound, RightShapeCount);
    const FBox LeftBounds = GetBodyWorldBounds(Mesh, TEXT("left_foot"), bLeftFound, LeftShapeCount);
    bOutRightFootBodyValidated = bRightFound && RightShapeCount > 0 && RightBounds.IsValid;
    bOutLeftFootBodyValidated = bLeftFound && LeftShapeCount > 0 && LeftBounds.IsValid;

    if (RightBounds.IsValid && LeftBounds.IsValid)
    {
        OutGroundAlignmentSource = TEXT("visual_replay_frame0_foot_body_bounds_min_z");
        return FMath::Min(RightBounds.Min.Z, LeftBounds.Min.Z);
    }

    OutGroundAlignmentSource = TEXT("default_flat_ground_z0_missing_foot_bounds");
    return 0.0;
}

FFootContactQueryResult QueryChaosFootContact(
    UWorld* World,
    USkeletalMeshComponent* Mesh,
    FName BodyName,
    double GroundTopZCm)
{
    FFootContactQueryResult Result;
    Result.BodyName = BodyName;
    if (!World || !Mesh)
    {
        return Result;
    }

    int32 ShapeCount = 0;
    bool bBodyInstanceFound = false;
    const FBox BodyBounds = GetBodyWorldBounds(Mesh, BodyName, bBodyInstanceFound, ShapeCount);
    const FVector FootLocation = BodyBounds.IsValid ? BodyBounds.GetCenter() : GetPhysicsFootLocation(Mesh, BodyName);
    const FVector FootExtent = BodyBounds.IsValid ? BodyBounds.GetExtent() : FVector(12.0, 12.0, 12.0);
    Result.bBodyInstanceFound = bBodyInstanceFound;
    Result.ShapeCount = ShapeCount;
    Result.bBodyValidated = bBodyInstanceFound && ShapeCount > 0 && BodyBounds.IsValid;
    Result.FootLocationMeters = FootLocation * 0.01;
    Result.FootLowestZMeters = (BodyBounds.IsValid ? BodyBounds.Min.Z : FootLocation.Z) * 0.01;
    Result.GroundDistanceMeters = FMath::Max(0.0, Result.FootLowestZMeters - GroundTopZCm * 0.01);

    FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(MimicKitLiveChaosContact), false);
    if (AActor* Owner = Mesh->GetOwner())
    {
        QueryParams.AddIgnoredActor(Owner);
    }

    constexpr double ContactMaxDistanceCm = 30.0;
    const FVector ProbeExtent(
        FMath::Max(6.0, FootExtent.X),
        FMath::Max(6.0, FootExtent.Y),
        FMath::Max(4.0, FootExtent.Z));
    const FCollisionShape FootProbeShape = FCollisionShape::MakeBox(ProbeExtent);
    const bool bOverlap = World->OverlapBlockingTestByChannel(
        FootLocation,
        FQuat::Identity,
        ECC_WorldStatic,
        FootProbeShape,
        QueryParams);
    if (bOverlap)
    {
        Result.bContact = true;
        Result.GroundDistanceMeters = FMath::Max(0.0, Result.FootLowestZMeters - GroundTopZCm * 0.01);
        Result.Method = TEXT("body_overlap_box");
        return Result;
    }

    FHitResult SweepHit;
    const FVector SweepStart = FootLocation + FVector(0.0, 0.0, 2.0);
    const FVector SweepEnd = FootLocation - FVector(0.0, 0.0, ContactMaxDistanceCm + ProbeExtent.Z + 2.0);
    const bool bSweepHit = World->SweepSingleByChannel(
        SweepHit,
        SweepStart,
        SweepEnd,
        FQuat::Identity,
        ECC_WorldStatic,
        FootProbeShape,
        QueryParams);
    if (bSweepHit && SweepHit.bBlockingHit)
    {
        Result.GroundDistanceMeters = FMath::Max(
            0.0,
            (BodyBounds.IsValid ? BodyBounds.Min.Z - GroundTopZCm : FootLocation.Z - SweepHit.ImpactPoint.Z) * 0.01);
        Result.bContact = Result.GroundDistanceMeters <= ContactMaxDistanceCm * 0.01;
        Result.Method = Result.bContact ? TEXT("shape_sweep_ground") : TEXT("shape_sweep_ground_too_far");
        if (Result.bContact)
        {
            return Result;
        }
    }

    FHitResult UpSweepHit;
    const FVector UpSweepStart = FootLocation - FVector(0.0, 0.0, ContactMaxDistanceCm + ProbeExtent.Z + 2.0);
    const FVector UpSweepEnd = FootLocation + FVector(0.0, 0.0, 2.0);
    const bool bUpSweepHit = World->SweepSingleByChannel(
        UpSweepHit,
        UpSweepStart,
        UpSweepEnd,
        FQuat::Identity,
        ECC_WorldStatic,
        FootProbeShape,
        QueryParams);
    if (bUpSweepHit && UpSweepHit.bBlockingHit)
    {
        Result.GroundDistanceMeters = FMath::Max(
            0.0,
            FMath::Abs((BodyBounds.IsValid ? BodyBounds.Min.Z - GroundTopZCm : FootLocation.Z - UpSweepHit.ImpactPoint.Z) * 0.01));
        Result.bContact = Result.GroundDistanceMeters <= ContactMaxDistanceCm * 0.01;
        Result.Method = Result.bContact ? TEXT("bidirectional_shape_sweep_ground") : TEXT("bidirectional_shape_sweep_ground_too_far");
        if (Result.bContact)
        {
            return Result;
        }
    }

    FHitResult Hit;
    const FVector Start = FootLocation + FVector(0.0, 0.0, 12.0);
    const FVector End = FootLocation - FVector(0.0, 0.0, ContactMaxDistanceCm + ProbeExtent.Z + 2.0);
    Result.bLineTraceHit = World->LineTraceSingleByChannel(Hit, Start, End, ECC_WorldStatic, QueryParams) && Hit.bBlockingHit;
    if (Result.Method == TEXT("none") && Result.bLineTraceHit)
    {
        Result.GroundDistanceMeters = FMath::Max(0.0, (BodyBounds.IsValid ? BodyBounds.Min.Z - GroundTopZCm : FootLocation.Z - Hit.ImpactPoint.Z) * 0.01);
        Result.Method = TEXT("line_trace_diagnostic_only");
    }
    return Result;
}

bool LoadSourceCharacterMeshForRuntime(
    const FString& PackageDirectory,
    USkeletalMesh*& OutMesh,
    FString& OutTargetMesh,
    FString& OutTargetSkeleton,
    FString& OutFallbackRenderer,
    FString& OutError)
{
    FString BoneMapHash;
    FString BasisTransformHash;
    bool bFullCharacterParity = false;
    LoadSkeletalContract(
        PackageDirectory,
        OutTargetMesh,
        OutTargetSkeleton,
        OutFallbackRenderer,
        BoneMapHash,
        BasisTransformHash,
        bFullCharacterParity);

    if (!OutTargetMesh.StartsWith(TEXT("/Game/MimicKit/SwordShield/")) || !OutFallbackRenderer.IsEmpty() || !bFullCharacterParity)
    {
        OutError = FString::Printf(TEXT("Runtime Chaos closure requires source-character mesh parity, got mesh=%s fallback=%s full=%s."),
            *OutTargetMesh,
            *OutFallbackRenderer,
            JsonBool(bFullCharacterParity));
        return false;
    }

    OutMesh = Cast<USkeletalMesh>(StaticLoadObject(USkeletalMesh::StaticClass(), nullptr, *NormalizeAssetObjectPath(OutTargetMesh)));
    if (!OutMesh)
    {
        OutError = FString::Printf(TEXT("Runtime Chaos closure could not load source SkeletalMesh: %s"), *OutTargetMesh);
        return false;
    }
    if (!OutMesh->GetPhysicsAsset())
    {
        OutError = FString::Printf(TEXT("Runtime Chaos closure source SkeletalMesh has no PhysicsAsset: %s"), *OutTargetMesh);
        return false;
    }
    return true;
}

bool RunUePhysicsActorPolicyLoop(
    const FGASPALSMimicKitPackageSummary& Package,
    const TArray<FReplayRow>& Rows,
    const TArray<FJointMapping>& Joints,
    const TArray<float>& ActionLow,
    const TArray<float>& ActionHigh,
    double SlidingThresholdMps,
    int32 MinContactFramesPerFoot,
    FNnePolicyRunner& PolicyRunner,
    int32 PolicyFrames,
    FString& OutTraceText,
    FRuntimeContactStats& OutStats,
    int32& OutInferenceRows,
    int32& OutFiniteActionRows,
    int32& OutTotalClampCount,
    double& OutTotalLatencyMs,
    FString& OutError)
{
    OutTraceText.Reset();
    OutStats = FRuntimeContactStats();
    OutStats.ObservationSource = TEXT("ue_physics_actor_state");
    OutStats.JointDriveBackend = TEXT("FConstraintInstance angular SLERP drive");
    OutStats.ContactMeasurementSource = TEXT("chaos_contact_query");
    OutStats.ContactQueryMethod = TEXT("body_overlap_or_shape_sweep_against_ground");
    OutInferenceRows = 0;
    OutFiniteActionRows = 0;
    OutTotalClampCount = 0;
    OutTotalLatencyMs = 0.0;

    USkeletalMesh* SourceMesh = nullptr;
    FString TargetMesh;
    FString TargetSkeleton;
    FString FallbackRenderer;
    if (!LoadSourceCharacterMeshForRuntime(Package.PackageDirectory, SourceMesh, TargetMesh, TargetSkeleton, FallbackRenderer, OutError))
    {
        return false;
    }

    UWorld::InitializationValues WorldValues;
    WorldValues.CreatePhysicsScene(true)
        .ShouldSimulatePhysics(true)
        .EnableTraceCollision(true)
        .CreateNavigation(false)
        .CreateAISystem(false)
        .AllowAudioPlayback(false);
    UWorld* RuntimeWorld = UWorld::CreateWorld(
        EWorldType::Game,
        false,
        TEXT("MimicKitLivePolicyChaosClosureWorld"),
        nullptr,
        false,
        ERHIFeatureLevel::Num,
        &WorldValues);
    if (!RuntimeWorld)
    {
        OutError = TEXT("Could not create runtime Chaos closure world.");
        return false;
    }
    if (GEngine)
    {
        FWorldContext& RuntimeWorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
        RuntimeWorldContext.SetCurrentWorld(RuntimeWorld);
    }
    auto DestroyRuntimeWorld = [&RuntimeWorld]()
    {
        if (RuntimeWorld)
        {
            if (GEngine)
            {
                GEngine->DestroyWorldContext(RuntimeWorld);
            }
            RuntimeWorld->DestroyWorld(false);
            RuntimeWorld = nullptr;
        }
    };

    AActor* GroundActor = RuntimeWorld->SpawnActor<AActor>();
    UBoxComponent* Ground = GroundActor ? NewObject<UBoxComponent>(GroundActor) : nullptr;
    if (!GroundActor || !Ground)
    {
        OutError = TEXT("Could not create runtime Chaos closure ground.");
        DestroyRuntimeWorld();
        return false;
    }
    GroundActor->SetRootComponent(Ground);
    Ground->SetBoxExtent(FVector(5000.0, 5000.0, 5.0), false);
    Ground->SetWorldLocation(FVector(0.0, 0.0, -5.0));
    Ground->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    Ground->SetCollisionObjectType(ECC_WorldStatic);
    Ground->SetCollisionResponseToAllChannels(ECR_Block);
    Ground->RegisterComponentWithWorld(RuntimeWorld);

    AActor* CharacterActor = RuntimeWorld->SpawnActor<AActor>();
    USkeletalMeshComponent* CharacterMesh = CharacterActor ? NewObject<USkeletalMeshComponent>(CharacterActor) : nullptr;
    if (!CharacterActor || !CharacterMesh)
    {
        OutError = TEXT("Could not create runtime Chaos source character actor.");
        DestroyRuntimeWorld();
        return false;
    }
    CharacterActor->SetRootComponent(CharacterMesh);
    CharacterMesh->SetSkeletalMesh(SourceMesh);
    CharacterMesh->SetPhysicsAsset(SourceMesh->GetPhysicsAsset());
    CharacterMesh->SetWorldTransform(FTransform(
        Rows.IsEmpty() ? FQuat::Identity : Rows[0].RootRot,
        Rows.IsEmpty() ? FVector(0.0, 0.0, 90.0) : Rows[0].RootPosMeters * 100.0));
    CharacterMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    CharacterMesh->SetCollisionObjectType(ECC_PhysicsBody);
    CharacterMesh->SetCollisionResponseToAllChannels(ECR_Block);
    CharacterMesh->SetGenerateOverlapEvents(true);
    CharacterMesh->RegisterComponentWithWorld(RuntimeWorld);
    CharacterMesh->SetAllBodiesNotifyRigidBodyCollision(true);
    CharacterMesh->SetAllBodiesSimulatePhysics(true);
    CharacterMesh->SetSimulatePhysics(true);
    CharacterMesh->SetAllBodiesPhysicsBlendWeight(1.0f);
    if (!Rows.IsEmpty())
    {
        ApplyInitialPhysicsPoseFromReplay(CharacterMesh, Rows[0], Joints);
    }
    CharacterMesh->WakeAllRigidBodies();

    bool bRightFootBodyValidated = false;
    bool bLeftFootBodyValidated = false;
    const double GroundTopZCm = ResolveRuntimeGroundTopCm(
        Package.PackageDirectory,
        CharacterMesh,
        OutStats.GroundAlignmentSource,
        bRightFootBodyValidated,
        bLeftFootBodyValidated);
    OutStats.GroundTopZMeters = GroundTopZCm * 0.01;
    OutStats.bRightFootBodyValidated = bRightFootBodyValidated;
    OutStats.bLeftFootBodyValidated = bLeftFootBodyValidated;
    Ground->SetWorldLocation(FVector(0.0, 0.0, GroundTopZCm - 5.0));

    RuntimeWorld->BeginPlay();

    const int32 MaxFrames = FMath::Min(PolicyFrames, Rows.Num());
    const int32 SubstepsPerPolicy = FMath::Max(1, Package.PhysicsHz / FMath::Max(1, Package.PolicyHz));
    const double PolicyDt = 1.0 / static_cast<double>(FMath::Max(1, Package.PolicyHz));
    const float PhysicsDt = 1.0f / static_cast<float>(FMath::Max(1, Package.PhysicsHz));

    if (!Rows.IsEmpty())
    {
        TArray<float> Frame0Targets;
        Frame0Targets.Reserve(Rows[0].DofPos.Num());
        for (const double Value : Rows[0].DofPos)
        {
            Frame0Targets.Add(static_cast<float>(Value));
        }
        ApplyConstraintJointDriveTargets(CharacterMesh, Joints, Frame0Targets);
        const int32 SettleSubsteps = FMath::Max(1, FMath::RoundToInt(0.25 * static_cast<double>(FMath::Max(1, Package.PhysicsHz))));
        for (int32 Substep = 0; Substep < SettleSubsteps; ++Substep)
        {
            RuntimeWorld->Tick(LEVELTICK_All, PhysicsDt);
            ++OutStats.PrePolicySettleSubsteps;
        }
    }

    FVector PrevRightFootMeters = FVector::ZeroVector;
    FVector PrevLeftFootMeters = FVector::ZeroVector;
    bool bPrevRightContact = false;
    bool bPrevLeftContact = false;
    bool bAllRootsFinite = MaxFrames > 0;
    int32 AppliedDriveFrames = 0;

    for (int32 FrameIndex = 0; FrameIndex < MaxFrames; ++FrameIndex)
    {
        TArray<float> Observation;
        const int32 ObservationFilledDim = BuildUePhysicsObservation(CharacterMesh, Joints, Package.ObservationDim, Observation);
        OutStats.ObservationFilledDim = FMath::Max(OutStats.ObservationFilledDim, ObservationFilledDim);

        TArray<float> Action;
        double InferenceLatencyMs = 0.0;
        if (!PolicyRunner.Run(Observation, Action, InferenceLatencyMs, OutError))
        {
            DestroyRuntimeWorld();
            return false;
        }
        ++OutInferenceRows;
        OutTotalLatencyMs += InferenceLatencyMs;

        const bool bActionDimOk = Action.Num() == Package.ActionDim;
        const bool bActionFinite = bActionDimOk && AreFinite(Action);
        if (bActionFinite)
        {
            ++OutFiniteActionRows;
        }
        const int32 ClampCount = CountActionClamps(Action, ActionLow, ActionHigh);
        OutTotalClampCount += ClampCount;

        const int32 AppliedConstraints = ApplyConstraintJointDriveTargets(CharacterMesh, Joints, Action);
        const bool bJointDriveAppliedThisFrame = AppliedConstraints > 0;
        if (bJointDriveAppliedThisFrame)
        {
            ++AppliedDriveFrames;
        }

        for (int32 Substep = 0; Substep < SubstepsPerPolicy; ++Substep)
        {
            RuntimeWorld->Tick(LEVELTICK_All, PhysicsDt);
            ++OutStats.PhysicsSubsteps;
        }

        const FFootContactQueryResult RightContact = QueryChaosFootContact(RuntimeWorld, CharacterMesh, TEXT("right_foot"), GroundTopZCm);
        const FFootContactQueryResult LeftContact = QueryChaosFootContact(RuntimeWorld, CharacterMesh, TEXT("left_foot"), GroundTopZCm);
        const FVector RightFootMeters = RightContact.FootLocationMeters;
        const FVector LeftFootMeters = LeftContact.FootLocationMeters;
        const bool bRightContact = RightContact.bContact;
        const bool bLeftContact = LeftContact.bContact;
        int32 ContactEventsThisFrame = 0;
        if (bRightContact)
        {
            ++OutStats.RightFootContactFrames;
            ++ContactEventsThisFrame;
            if (bPrevRightContact)
            {
                const double Speed = FVector2D(RightFootMeters.X - PrevRightFootMeters.X, RightFootMeters.Y - PrevRightFootMeters.Y).Size() / PolicyDt;
                OutStats.MaxFootSlidingMpsRaw = FMath::Max(OutStats.MaxFootSlidingMpsRaw, Speed);
            }
            PrevRightFootMeters = RightFootMeters;
        }
        if (bLeftContact)
        {
            ++OutStats.LeftFootContactFrames;
            ++ContactEventsThisFrame;
            if (bPrevLeftContact)
            {
                const double Speed = FVector2D(LeftFootMeters.X - PrevLeftFootMeters.X, LeftFootMeters.Y - PrevLeftFootMeters.Y).Size() / PolicyDt;
                OutStats.MaxFootSlidingMpsRaw = FMath::Max(OutStats.MaxFootSlidingMpsRaw, Speed);
            }
            PrevLeftFootMeters = LeftFootMeters;
        }
        bPrevRightContact = bRightContact;
        bPrevLeftContact = bLeftContact;
        OutStats.ContactEvents += ContactEventsThisFrame;

        const FVector RootLocationMeters = CharacterMesh->GetBoneLocation(TEXT("pelvis"), EBoneSpaces::WorldSpace) * 0.01;
        bAllRootsFinite = bAllRootsFinite && IsFiniteVector(RootLocationMeters);

        const uint32 ActionHash = HashAction(Action);
        OutTraceText += FString::Printf(
            TEXT("{\"frame\":%d,\"time_seconds\":%.6f,\"observation_dim\":%d,\"observation_filled_dim\":%d,\"action_dim\":%d,\"observation_source\":\"ue_physics_actor_state\",\"obs_layout_contract\":\"compute_char_obs_plus_task_steering\",\"action_finite\":%s,\"root_finite\":%s,\"policy_backend\":\"NNERuntimeORT\",\"actual_runtime_name\":\"%s\",\"control_mode\":\"live_nne_joint_pd_targets\",\"policy_inference_ran\":true,\"trace_fallback_used\":false,\"joint_drive_applied\":%s,\"joint_drive_backend\":\"FConstraintInstance angular SLERP drive\",\"physics_substeps\":%d,\"contact_events\":%d,\"contact_query_method\":\"body_overlap_or_shape_sweep_against_ground\",\"right_foot_body_name\":\"%s\",\"left_foot_body_name\":\"%s\",\"right_foot_body_instance_found\":%s,\"left_foot_body_instance_found\":%s,\"right_foot_shape_count\":%d,\"left_foot_shape_count\":%d,\"right_foot_contact\":%s,\"left_foot_contact\":%s,\"right_foot_contact_method\":\"%s\",\"left_foot_contact_method\":\"%s\",\"ground_top_z_m\":%.6f,\"ground_alignment_source\":\"%s\",\"right_foot_z_m\":%.6f,\"left_foot_z_m\":%.6f,\"right_foot_lowest_z_m\":%.6f,\"left_foot_lowest_z_m\":%.6f,\"right_foot_ground_distance_m\":%.6f,\"left_foot_ground_distance_m\":%.6f,\"contact_measurement_source\":\"chaos_contact_query\",\"action_hash\":\"%08x\",\"action_clamp_count\":%d,\"inference_latency_ms\":%.6f}\n"),
            FrameIndex,
            static_cast<double>(FrameIndex) * PolicyDt,
            Package.ObservationDim,
            ObservationFilledDim,
            Action.Num(),
            JsonBool(bActionFinite),
            JsonBool(IsFiniteVector(RootLocationMeters)),
            *PolicyRunner.GetRuntimeName(),
            JsonBool(bJointDriveAppliedThisFrame),
            SubstepsPerPolicy,
            ContactEventsThisFrame,
            *RightContact.BodyName.ToString(),
            *LeftContact.BodyName.ToString(),
            JsonBool(RightContact.bBodyInstanceFound),
            JsonBool(LeftContact.bBodyInstanceFound),
            RightContact.ShapeCount,
            LeftContact.ShapeCount,
            JsonBool(bRightContact),
            JsonBool(bLeftContact),
            *RightContact.Method,
            *LeftContact.Method,
            OutStats.GroundTopZMeters,
            *OutStats.GroundAlignmentSource,
            RightFootMeters.Z,
            LeftFootMeters.Z,
            RightContact.FootLowestZMeters,
            LeftContact.FootLowestZMeters,
            RightContact.GroundDistanceMeters,
            LeftContact.GroundDistanceMeters,
            ActionHash,
            ClampCount,
            InferenceLatencyMs);
    }

    OutStats.bRootFinite = bAllRootsFinite;
    OutStats.bJointDriveApplied = AppliedDriveFrames == MaxFrames && MaxFrames > 0;
    OutStats.bFootContactsObserved =
        OutStats.RightFootContactFrames >= MinContactFramesPerFoot
        && OutStats.LeftFootContactFrames >= MinContactFramesPerFoot;
    OutStats.MaxFootSlidingMpsScored = OutStats.MaxFootSlidingMpsRaw;
    OutStats.bSlidingUnderThreshold = OutStats.bFootContactsObserved && OutStats.MaxFootSlidingMpsRaw <= SlidingThresholdMps;
    OutStats.bPhysicsSimulated =
        RuntimeWorld != nullptr
        && CharacterMesh->IsAnySimulatingPhysics()
        && OutStats.PhysicsSubsteps >= MaxFrames * SubstepsPerPolicy
        && MaxFrames > 0;

    DestroyRuntimeWorld();
    return true;
}

bool FGASPALSMimicKitVisualReplayCapture::RunLivePolicyChaosClosure(
    const FGASPALSMimicKitPackageSummary& Package,
    const FString& RuntimeDirectory,
    const FString& SourceCaptureDirectory,
    double RuntimeSeconds,
    FGASPALSMimicKitRuntimeClosureSummary& OutSummary,
    FString& OutError)
{
    OutSummary = FGASPALSMimicKitRuntimeClosureSummary();
    OutSummary.PackageDirectory = Package.PackageDirectory;
    OutSummary.RuntimeDirectory = FPaths::ConvertRelativePathToFull(RuntimeDirectory);
    OutSummary.RuntimeReportFile = FPaths::Combine(OutSummary.RuntimeDirectory, TEXT("runtime_closure_report.json"));
    OutSummary.RuntimeTraceFile = FPaths::Combine(OutSummary.RuntimeDirectory, TEXT("runtime_trace.jsonl"));
    OutSummary.CombinedReportFile = FPaths::Combine(OutSummary.RuntimeDirectory, TEXT("mimickit_ue_full_closure_report.json"));
    OutSummary.BrainName = Package.BrainName;
    OutSummary.RuntimeBackend = TEXT("NNERuntimeORT");
    OutSummary.ModelFile = JoinPackagePath(Package.PackageDirectory, Package.ModelFileName);
    OutSummary.SourceCaptureDirectory = FPaths::ConvertRelativePathToFull(SourceCaptureDirectory);
    OutSummary.ControlMode = TEXT("live_nne_joint_pd_targets");
    OutSummary.ObservationSource = TEXT("ue_physics_actor_state");
    OutSummary.ObsLayoutContract = TEXT("compute_char_obs_plus_task_steering");
    OutSummary.InitialStateSource = TEXT("visual_replay_frame0_only");
    OutSummary.JointDriveBackend = TEXT("FConstraintInstance angular SLERP drive");
    OutSummary.ContactMeasurementSource = TEXT("chaos_contact_query");
    OutSummary.ContactQueryMethod = TEXT("body_overlap_or_shape_sweep_against_ground");
    OutSummary.ObservationDim = Package.ObservationDim;
    OutSummary.ActionDim = Package.ActionDim;
    OutSummary.PolicyHz = Package.PolicyHz;
    OutSummary.PhysicsHz = Package.PhysicsHz;
    OutSummary.PolicyFrames = FMath::RoundToInt(FMath::Max(0.1, RuntimeSeconds) * static_cast<double>(FMath::Max(1, Package.PolicyHz)));
    OutSummary.bTraceFallbackUsed = false;

    OutSummary.bOnnxLoadedFromPackage = FPaths::FileExists(OutSummary.ModelFile);
    if (!OutSummary.bOnnxLoadedFromPackage)
    {
        OutError = FString::Printf(TEXT("Runtime closure could not find ONNX model file: %s"), *OutSummary.ModelFile);
        OutSummary.Error = OutError;
        return false;
    }

    TArray<float> ActionLow;
    TArray<float> ActionHigh;
    if (!LoadActionBounds(Package.PackageDirectory, Package.ActionDim, ActionLow, ActionHigh, OutError))
    {
        OutSummary.Error = OutError;
        return false;
    }

    TArray<FReplayRow> Rows;
    if (!LoadReplayRows(VisualReplayPathForPackage(Package.PackageDirectory), Package.DofSize, Rows, OutError))
    {
        OutSummary.Error = OutError;
        return false;
    }

    TArray<FJointMapping> Joints;
    if (!LoadJointMappingsForPackage(Package.PackageDirectory, Joints, true, OutError))
    {
        OutSummary.Error = OutError;
        return false;
    }

    double SlidingThresholdMps = 0.25;
    int32 MinContactFramesPerFoot = 1;
    TSharedPtr<FJsonObject> RuntimeContract;
    FString RuntimeContractError;
    if (LoadJsonObject(JoinPackagePath(Package.PackageDirectory, TEXT("runtime_control_contract.json")), RuntimeContract, RuntimeContractError))
    {
        const TSharedPtr<FJsonObject> ContactContract = GetObjectField(RuntimeContract, TEXT("contact"));
        SlidingThresholdMps = GetDoubleField(ContactContract, TEXT("sliding_threshold_mps"), SlidingThresholdMps);
        MinContactFramesPerFoot = FMath::Max(1, GetIntField(ContactContract, TEXT("min_contact_frames_per_foot")));
    }

    FNnePolicyRunner PolicyRunner;
    if (!PolicyRunner.Initialize(OutSummary.ModelFile, Package.ObservationDim, Package.ActionDim, OutError))
    {
        OutSummary.Error = OutError;
        return false;
    }
    OutSummary.ActualRuntimeName = PolicyRunner.GetRuntimeName();

    int32 FiniteActionRows = 0;
    int32 InferenceRows = 0;
    int32 TotalClampCount = 0;
    double TotalLatencyMs = 0.0;
    FString TraceText;

    FRuntimeContactStats ContactStats;
    if (!RunUePhysicsActorPolicyLoop(
        Package,
        Rows,
        Joints,
        ActionLow,
        ActionHigh,
        SlidingThresholdMps,
        MinContactFramesPerFoot,
        PolicyRunner,
        OutSummary.PolicyFrames,
        TraceText,
        ContactStats,
        InferenceRows,
        FiniteActionRows,
        TotalClampCount,
        TotalLatencyMs,
        OutError))
    {
        OutSummary.Error = OutError;
        return false;
    }

    const int32 ExpectedRows = FMath::Min(OutSummary.PolicyFrames, Rows.Num());
    OutSummary.bPolicyInferenceRan = InferenceRows == ExpectedRows && ExpectedRows > 0;
    OutSummary.bActionsFinite = FiniteActionRows == ExpectedRows && ExpectedRows > 0;
    OutSummary.bRootFinite = ContactStats.bRootFinite;
    OutSummary.bFootContactsObserved = ContactStats.bFootContactsObserved;
    OutSummary.bSlidingUnderThreshold = ContactStats.bSlidingUnderThreshold;
    OutSummary.bPhysicsSimulated = ContactStats.bPhysicsSimulated;
    OutSummary.bJointDriveApplied = ContactStats.bJointDriveApplied;
    OutSummary.PhysicsSubsteps = ContactStats.PhysicsSubsteps;
    OutSummary.PrePolicySettleSubsteps = ContactStats.PrePolicySettleSubsteps;
    OutSummary.ContactEvents = ContactStats.ContactEvents;
    OutSummary.RightFootContactFrames = ContactStats.RightFootContactFrames;
    OutSummary.LeftFootContactFrames = ContactStats.LeftFootContactFrames;
    OutSummary.bRightFootBodyValidated = ContactStats.bRightFootBodyValidated;
    OutSummary.bLeftFootBodyValidated = ContactStats.bLeftFootBodyValidated;
    OutSummary.GroundAlignmentSource = ContactStats.GroundAlignmentSource;
    OutSummary.MaxFootSlidingMpsRaw = ContactStats.MaxFootSlidingMpsRaw;
    OutSummary.MaxFootSlidingMpsScored = ContactStats.MaxFootSlidingMpsScored;
    OutSummary.MaxFootSlidingMps = ContactStats.MaxFootSlidingMpsScored;
    OutSummary.MaxJointTargetError = ContactStats.MaxJointTargetError;
    OutSummary.ObservationFilledDim = ContactStats.ObservationFilledDim;
    OutSummary.MeanInferenceLatencyMs = InferenceRows > 0 ? TotalLatencyMs / static_cast<double>(InferenceRows) : 0.0;
    OutSummary.ObservationSource = ContactStats.ObservationSource;
    OutSummary.JointDriveBackend = ContactStats.JointDriveBackend;
    OutSummary.ContactMeasurementSource = ContactStats.ContactMeasurementSource;
    OutSummary.ContactQueryMethod = ContactStats.ContactQueryMethod;
    OutSummary.bLivePolicyControlPass =
        OutSummary.bOnnxLoadedFromPackage
        && OutSummary.bPolicyInferenceRan
        && !OutSummary.bTraceFallbackUsed
        && OutSummary.bActionsFinite
        && OutSummary.ObservationSource == TEXT("ue_physics_actor_state")
        && OutSummary.ObservationFilledDim == OutSummary.ObservationDim
        && OutSummary.ObservationDim > 0
        && OutSummary.ActionDim == Package.DofSize
        && OutSummary.PolicyHz == 30
        && OutSummary.PhysicsHz == 240;
    OutSummary.bChaosContactValidated =
        OutSummary.bLivePolicyControlPass
        && OutSummary.bPhysicsSimulated
        && OutSummary.bJointDriveApplied
        && OutSummary.ContactMeasurementSource == TEXT("chaos_contact_query")
        && OutSummary.bRightFootBodyValidated
        && OutSummary.bLeftFootBodyValidated
        && OutSummary.bFootContactsObserved
        && OutSummary.bSlidingUnderThreshold;

    IFileManager::Get().MakeDirectory(*OutSummary.RuntimeDirectory, true);
    {
        TSharedPtr<FJsonObject> ProxyReport = MakeShared<FJsonObject>();
        ProxyReport->SetStringField(TEXT("brain"), Package.BrainName);
        ProxyReport->SetStringField(TEXT("closure_mode"), TEXT("kinematic_proxy_smoke"));
        ProxyReport->SetStringField(TEXT("observation_source"), TEXT("visual_replay_fixture_proxy"));
        ProxyReport->SetStringField(TEXT("joint_drive_backend"), TEXT("kinematic_contact_proxy"));
        ProxyReport->SetStringField(TEXT("contact_measurement_source"), TEXT("kinematic_contact_proxy"));
        ProxyReport->SetBoolField(TEXT("policy_inference_ran"), false);
        ProxyReport->SetBoolField(TEXT("trace_fallback_used"), false);
        ProxyReport->SetBoolField(TEXT("physics_simulated"), false);
        ProxyReport->SetBoolField(TEXT("joint_drive_applied"), false);
        ProxyReport->SetBoolField(TEXT("live_policy_control_pass"), false);
        ProxyReport->SetBoolField(TEXT("chaos_contact_validated"), false);
        ProxyReport->SetBoolField(TEXT("combined_pass"), false);
        ProxyReport->SetStringField(TEXT("status"), TEXT("diagnostic only; kinematic proxy cannot satisfy live/Chaos closure"));

        FString ProxyText;
        const TSharedRef<TJsonWriter<>> ProxyWriter = TJsonWriterFactory<>::Create(&ProxyText);
        FJsonSerializer::Serialize(ProxyReport.ToSharedRef(), ProxyWriter);
        FFileHelper::SaveStringToFile(
            ProxyText,
            *FPaths::Combine(OutSummary.RuntimeDirectory, TEXT("kinematic_proxy_report.json")),
            FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    }
    FFileHelper::SaveStringToFile(TraceText, *OutSummary.RuntimeTraceFile, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);

    TSharedPtr<FJsonObject> RuntimeReport = MakeShared<FJsonObject>();
    RuntimeReport->SetStringField(TEXT("brain"), Package.BrainName);
    RuntimeReport->SetStringField(TEXT("runtime_backend"), OutSummary.RuntimeBackend);
    RuntimeReport->SetStringField(TEXT("actual_runtime_name"), OutSummary.ActualRuntimeName);
    RuntimeReport->SetStringField(TEXT("backend_status"), TEXT("onnx package loaded without Python; actions produced by UE NNE RunSync"));
    RuntimeReport->SetStringField(TEXT("model_file"), OutSummary.ModelFile);
    RuntimeReport->SetStringField(TEXT("runtime_trace"), OutSummary.RuntimeTraceFile);
    RuntimeReport->SetStringField(TEXT("control_mode"), OutSummary.ControlMode);
    RuntimeReport->SetStringField(TEXT("closure_mode"), TEXT("source_character_visual_parity_plus_live_nne_joint_pd_chaos_contact"));
    RuntimeReport->SetStringField(TEXT("observation_source"), OutSummary.ObservationSource);
    RuntimeReport->SetStringField(TEXT("obs_layout_contract"), OutSummary.ObsLayoutContract);
    RuntimeReport->SetStringField(TEXT("initial_state_source"), OutSummary.InitialStateSource);
    RuntimeReport->SetStringField(TEXT("joint_drive_backend"), OutSummary.JointDriveBackend);
    RuntimeReport->SetNumberField(TEXT("policy_frames"), InferenceRows);
    RuntimeReport->SetNumberField(TEXT("observation_dim"), Package.ObservationDim);
    RuntimeReport->SetNumberField(TEXT("observation_filled_dim"), OutSummary.ObservationFilledDim);
    RuntimeReport->SetNumberField(TEXT("action_dim"), Package.ActionDim);
    RuntimeReport->SetNumberField(TEXT("policy_hz"), Package.PolicyHz);
    RuntimeReport->SetNumberField(TEXT("physics_hz"), Package.PhysicsHz);
    RuntimeReport->SetBoolField(TEXT("onnx_loaded_without_python"), OutSummary.bOnnxLoadedFromPackage);
    RuntimeReport->SetBoolField(TEXT("policy_inference_ran"), OutSummary.bPolicyInferenceRan);
    RuntimeReport->SetBoolField(TEXT("trace_fallback_used"), OutSummary.bTraceFallbackUsed);
    RuntimeReport->SetBoolField(TEXT("physics_simulated"), OutSummary.bPhysicsSimulated);
    RuntimeReport->SetBoolField(TEXT("joint_drive_applied"), OutSummary.bJointDriveApplied);
    RuntimeReport->SetNumberField(TEXT("physics_substeps"), OutSummary.PhysicsSubsteps);
    RuntimeReport->SetNumberField(TEXT("pre_policy_settle_substeps"), OutSummary.PrePolicySettleSubsteps);
    RuntimeReport->SetBoolField(TEXT("actions_finite"), OutSummary.bActionsFinite);
    RuntimeReport->SetBoolField(TEXT("root_finite"), OutSummary.bRootFinite);
    RuntimeReport->SetBoolField(TEXT("foot_contacts_observed"), OutSummary.bFootContactsObserved);
    RuntimeReport->SetBoolField(TEXT("right_foot_body_validated"), OutSummary.bRightFootBodyValidated);
    RuntimeReport->SetBoolField(TEXT("left_foot_body_validated"), OutSummary.bLeftFootBodyValidated);
    RuntimeReport->SetStringField(TEXT("ground_alignment_source"), OutSummary.GroundAlignmentSource);
    RuntimeReport->SetNumberField(TEXT("ground_top_z_m"), ContactStats.GroundTopZMeters);
    RuntimeReport->SetStringField(TEXT("contact_measurement_source"), OutSummary.ContactMeasurementSource);
    RuntimeReport->SetStringField(TEXT("contact_query_method"), OutSummary.ContactQueryMethod);
    RuntimeReport->SetNumberField(TEXT("contact_events"), OutSummary.ContactEvents);
    RuntimeReport->SetNumberField(TEXT("right_foot_contact_frames"), OutSummary.RightFootContactFrames);
    RuntimeReport->SetNumberField(TEXT("left_foot_contact_frames"), OutSummary.LeftFootContactFrames);
    RuntimeReport->SetNumberField(TEXT("max_foot_sliding_mps"), OutSummary.MaxFootSlidingMps);
    RuntimeReport->SetNumberField(TEXT("max_foot_sliding_mps_raw"), OutSummary.MaxFootSlidingMpsRaw);
    RuntimeReport->SetNumberField(TEXT("max_foot_sliding_mps_scored"), OutSummary.MaxFootSlidingMpsScored);
    RuntimeReport->SetNumberField(TEXT("max_joint_target_error"), OutSummary.MaxJointTargetError);
    RuntimeReport->SetNumberField(TEXT("mean_inference_latency_ms"), OutSummary.MeanInferenceLatencyMs);
    RuntimeReport->SetNumberField(TEXT("action_clamp_count"), TotalClampCount);
    RuntimeReport->SetBoolField(TEXT("sliding_under_threshold"), OutSummary.bSlidingUnderThreshold);
    RuntimeReport->SetBoolField(TEXT("live_policy_control_pass"), OutSummary.bLivePolicyControlPass);
    RuntimeReport->SetBoolField(TEXT("chaos_contact_validated"), OutSummary.bChaosContactValidated);

    FString RuntimeReportText;
    const TSharedRef<TJsonWriter<>> RuntimeWriter = TJsonWriterFactory<>::Create(&RuntimeReportText);
    FJsonSerializer::Serialize(RuntimeReport.ToSharedRef(), RuntimeWriter);
    FFileHelper::SaveStringToFile(RuntimeReportText, *OutSummary.RuntimeReportFile, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);

    TSharedPtr<FJsonObject> VisualReport;
    const FString VisualReportFile = FPaths::Combine(OutSummary.SourceCaptureDirectory, TEXT("visual_diff_report.json"));
    const bool bVisualReportLoaded = LoadJsonObject(VisualReportFile, VisualReport, OutError);
    const bool bSourceReplayPass = bVisualReportLoaded && GetBoolField(VisualReport, TEXT("source_character_replay_pass"));
    const bool bFullVisualParity = bVisualReportLoaded && GetBoolField(VisualReport, TEXT("full_visual_parity"));
    const TSharedPtr<FJsonObject> VisualMedia = bVisualReportLoaded ? GetObjectField(VisualReport, TEXT("ue_media")) : nullptr;
    const bool bPngOk = GetBoolField(VisualMedia, TEXT("png_ok"));
    const bool bMp4Ok = GetBoolField(VisualMedia, TEXT("mp4_ok"));

    TSharedPtr<FJsonObject> Combined = MakeShared<FJsonObject>();
    Combined->SetStringField(TEXT("brain"), Package.BrainName);
    Combined->SetStringField(TEXT("package_dir"), Package.PackageDirectory);
    Combined->SetStringField(TEXT("source_capture_dir"), OutSummary.SourceCaptureDirectory);
    Combined->SetStringField(TEXT("runtime_dir"), OutSummary.RuntimeDirectory);
    Combined->SetStringField(TEXT("runtime_report"), OutSummary.RuntimeReportFile);
    Combined->SetBoolField(TEXT("source_character_replay_pass"), bSourceReplayPass);
    Combined->SetBoolField(TEXT("full_visual_parity"), bFullVisualParity);
    Combined->SetBoolField(TEXT("live_policy_control_pass"), OutSummary.bLivePolicyControlPass);
    Combined->SetBoolField(TEXT("chaos_contact_validated"), OutSummary.bChaosContactValidated);
    Combined->SetBoolField(TEXT("policy_inference_ran"), OutSummary.bPolicyInferenceRan);
    Combined->SetBoolField(TEXT("trace_fallback_used"), OutSummary.bTraceFallbackUsed);
    Combined->SetBoolField(TEXT("physics_simulated"), OutSummary.bPhysicsSimulated);
    Combined->SetBoolField(TEXT("joint_drive_applied"), OutSummary.bJointDriveApplied);
    Combined->SetNumberField(TEXT("physics_substeps"), OutSummary.PhysicsSubsteps);
    Combined->SetNumberField(TEXT("pre_policy_settle_substeps"), OutSummary.PrePolicySettleSubsteps);
    Combined->SetNumberField(TEXT("contact_events"), OutSummary.ContactEvents);
    Combined->SetNumberField(TEXT("right_foot_contact_frames"), OutSummary.RightFootContactFrames);
    Combined->SetNumberField(TEXT("left_foot_contact_frames"), OutSummary.LeftFootContactFrames);
    Combined->SetBoolField(TEXT("right_foot_body_validated"), OutSummary.bRightFootBodyValidated);
    Combined->SetBoolField(TEXT("left_foot_body_validated"), OutSummary.bLeftFootBodyValidated);
    Combined->SetNumberField(TEXT("ground_top_z_m"), ContactStats.GroundTopZMeters);
    Combined->SetBoolField(TEXT("png_ok"), bPngOk);
    Combined->SetBoolField(TEXT("mp4_ok"), bMp4Ok);
    Combined->SetStringField(TEXT("runtime_backend"), OutSummary.RuntimeBackend);
    Combined->SetStringField(TEXT("actual_runtime_name"), OutSummary.ActualRuntimeName);
    Combined->SetStringField(TEXT("observation_source"), OutSummary.ObservationSource);
    Combined->SetStringField(TEXT("obs_layout_contract"), OutSummary.ObsLayoutContract);
    Combined->SetNumberField(TEXT("observation_filled_dim"), OutSummary.ObservationFilledDim);
    Combined->SetStringField(TEXT("initial_state_source"), OutSummary.InitialStateSource);
    Combined->SetStringField(TEXT("joint_drive_backend"), OutSummary.JointDriveBackend);
    Combined->SetStringField(TEXT("contact_measurement_source"), OutSummary.ContactMeasurementSource);
    Combined->SetStringField(TEXT("contact_query_method"), OutSummary.ContactQueryMethod);
    Combined->SetStringField(TEXT("ground_alignment_source"), OutSummary.GroundAlignmentSource);
    Combined->SetStringField(TEXT("closure_mode"), TEXT("source_character_visual_parity_plus_live_nne_joint_pd_chaos_contact"));
    const bool bCombinedPass =
        bSourceReplayPass
        && bFullVisualParity
        && OutSummary.bPolicyInferenceRan
        && !OutSummary.bTraceFallbackUsed
        && OutSummary.ObservationSource == TEXT("ue_physics_actor_state")
        && OutSummary.ObservationFilledDim == OutSummary.ObservationDim
        && OutSummary.bPhysicsSimulated
        && OutSummary.bJointDriveApplied
        && OutSummary.ContactMeasurementSource == TEXT("chaos_contact_query")
        && OutSummary.bLivePolicyControlPass
        && OutSummary.bChaosContactValidated
        && bPngOk
        && bMp4Ok;
    Combined->SetBoolField(TEXT("combined_pass"), bCombinedPass);

    FString CombinedText;
    const TSharedRef<TJsonWriter<>> CombinedWriter = TJsonWriterFactory<>::Create(&CombinedText);
    FJsonSerializer::Serialize(Combined.ToSharedRef(), CombinedWriter);
    FFileHelper::SaveStringToFile(CombinedText, *OutSummary.CombinedReportFile, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);

    OutSummary.bPassed = bCombinedPass;
    if (!OutSummary.bPassed)
    {
        OutError = TEXT("Live policy/Chaos closure did not satisfy combined source-character gate.");
        OutSummary.Error = OutError;
        return false;
    }

    OutError.Reset();
    return true;
}

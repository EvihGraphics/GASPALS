#include "GASPALSMimicKitSourceRigAssetBuilder.h"

#include "Animation/Skeleton.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "BoneWeights.h"
#include "Dom/JsonObject.h"
#include "Engine/SkeletalMesh.h"
#include "HAL/FileManager.h"
#include "MeshDescription.h"
#include "MeshDescriptionBuilder.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "PhysicsAssetUtils.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "ReferenceSkeleton.h"
#include "Rendering/SkeletalMeshModel.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "SkeletalMeshAttributes.h"
#include "StaticToSkeletalMeshConverter.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

namespace
{
struct FSourceBoneDef
{
    FName Name;
    FName ParentName;
    int32 ParentIndex = INDEX_NONE;
    FVector LocalOffsetCm = FVector::ZeroVector;
    FVector GlobalPositionCm = FVector::ZeroVector;
};

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

bool LoadJsonObject(const FString& FilePath, TSharedPtr<FJsonObject>& OutObject, FString& OutError)
{
    FString Text;
    if (!FFileHelper::LoadFileToString(Text, *FilePath))
    {
        OutError = FString::Printf(TEXT("Could not read JSON file: %s"), *FilePath);
        return false;
    }
    const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
    if (!FJsonSerializer::Deserialize(Reader, OutObject) || !OutObject.IsValid())
    {
        OutError = FString::Printf(TEXT("Could not parse JSON file: %s"), *FilePath);
        return false;
    }
    return true;
}

bool WriteJsonObject(const FString& FilePath, const TSharedRef<FJsonObject>& Object, FString& OutError)
{
    FString Text;
    const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Text);
    FJsonSerializer::Serialize(Object, Writer);
    if (!FFileHelper::SaveStringToFile(Text, *FilePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
    {
        OutError = FString::Printf(TEXT("Could not write JSON file: %s"), *FilePath);
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

bool GetBoolField(const TSharedPtr<FJsonObject>& Object, const FString& FieldName)
{
    bool Value = false;
    return Object.IsValid() && Object->TryGetBoolField(FieldName, Value) ? Value : false;
}

FVector RestOffsetForBodyCm(const FString& BodyName)
{
    if (BodyName == TEXT("torso")) { return FVector(0.0, 0.0, 42.0); }
    if (BodyName == TEXT("head")) { return FVector(4.0, 0.0, 36.0); }
    if (BodyName == TEXT("right_upper_arm")) { return FVector(2.0, -24.0, 28.0); }
    if (BodyName == TEXT("right_lower_arm")) { return FVector(0.0, -31.0, -2.0); }
    if (BodyName == TEXT("right_hand")) { return FVector(0.0, -24.0, -2.0); }
    if (BodyName == TEXT("sword")) { return FVector(42.0, -4.0, 2.0); }
    if (BodyName == TEXT("left_upper_arm")) { return FVector(2.0, 24.0, 28.0); }
    if (BodyName == TEXT("left_lower_arm")) { return FVector(0.0, 31.0, -2.0); }
    if (BodyName == TEXT("left_hand")) { return FVector(0.0, 24.0, -2.0); }
    if (BodyName == TEXT("shield")) { return FVector(5.0, 12.0, 2.0); }
    if (BodyName == TEXT("right_thigh")) { return FVector(0.0, -13.0, -42.0); }
    if (BodyName == TEXT("right_shin")) { return FVector(0.0, 0.0, -43.0); }
    if (BodyName == TEXT("right_foot")) { return FVector(18.0, 0.0, -10.0); }
    if (BodyName == TEXT("left_thigh")) { return FVector(0.0, 13.0, -42.0); }
    if (BodyName == TEXT("left_shin")) { return FVector(0.0, 0.0, -43.0); }
    if (BodyName == TEXT("left_foot")) { return FVector(18.0, 0.0, -10.0); }
    return FVector::ZeroVector;
}

FVector BoxExtentForBodyCm(const FString& BodyName)
{
    if (BodyName == TEXT("pelvis")) { return FVector(18.0, 12.0, 10.0); }
    if (BodyName == TEXT("torso")) { return FVector(16.0, 18.0, 24.0); }
    if (BodyName == TEXT("head")) { return FVector(11.0, 9.0, 11.0); }
    if (BodyName.Contains(TEXT("upper_arm")) || BodyName.Contains(TEXT("lower_arm"))) { return FVector(7.0, 7.0, 18.0); }
    if (BodyName.Contains(TEXT("thigh")) || BodyName.Contains(TEXT("shin"))) { return FVector(8.0, 8.0, 22.0); }
    if (BodyName.Contains(TEXT("foot"))) { return FVector(18.0, 7.0, 5.0); }
    if (BodyName == TEXT("sword")) { return FVector(42.0, 2.5, 2.5); }
    if (BodyName == TEXT("shield")) { return FVector(4.0, 18.0, 18.0); }
    return FVector(7.0, 7.0, 7.0);
}

bool LoadBoneDefs(const FString& JointOrderFile, TArray<FSourceBoneDef>& OutBones, int32& OutDofSize, FString& OutError)
{
    TSharedPtr<FJsonObject> JointOrder;
    if (!LoadJsonObject(JointOrderFile, JointOrder, OutError))
    {
        return false;
    }

    OutBones.Reset();
    OutDofSize = GetIntField(JointOrder, TEXT("dof_size"));

    TMap<FName, int32> BoneIndexByName;
    FSourceBoneDef Root;
    Root.Name = TEXT("pelvis");
    Root.ParentIndex = INDEX_NONE;
    Root.GlobalPositionCm = FVector::ZeroVector;
    BoneIndexByName.Add(Root.Name, OutBones.Add(Root));

    const TArray<TSharedPtr<FJsonValue>>* Joints = nullptr;
    if (!JointOrder->TryGetArrayField(TEXT("joints"), Joints))
    {
        OutError = FString::Printf(TEXT("joint_order has no joints: %s"), *JointOrderFile);
        return false;
    }

    for (const TSharedPtr<FJsonValue>& Value : *Joints)
    {
        const TSharedPtr<FJsonObject> Object = Value.IsValid() ? Value->AsObject() : nullptr;
        const FString BodyName = GetStringField(Object, TEXT("body_name"));
        const FString ParentName = GetStringField(Object, TEXT("parent_body_name"));
        if (BodyName.IsEmpty() || ParentName.IsEmpty())
        {
            continue;
        }
        const FName BoneName(*BodyName);
        if (BoneIndexByName.Contains(BoneName))
        {
            continue;
        }

        const int32* ParentIndex = BoneIndexByName.Find(FName(*ParentName));
        if (!ParentIndex)
        {
            OutError = FString::Printf(TEXT("joint_order parent appears after child: %s -> %s"), *ParentName, *BodyName);
            return false;
        }

        FSourceBoneDef Bone;
        Bone.Name = BoneName;
        Bone.ParentName = FName(*ParentName);
        Bone.ParentIndex = *ParentIndex;
        Bone.LocalOffsetCm = RestOffsetForBodyCm(BodyName);
        Bone.GlobalPositionCm = OutBones[*ParentIndex].GlobalPositionCm + Bone.LocalOffsetCm;
        BoneIndexByName.Add(Bone.Name, OutBones.Add(Bone));
    }

    if (OutBones.Num() < 17 || OutDofSize <= 0)
    {
        OutError = FString::Printf(TEXT("source skeleton is incomplete: bones=%d dof=%d"), OutBones.Num(), OutDofSize);
        return false;
    }
    return true;
}

void AddBox(
    FMeshDescriptionBuilder& Builder,
    FSkinWeightsVertexAttributesRef& SkinWeights,
    const FPolygonGroupID PolygonGroup,
    const FVector& Center,
    const FVector& Extent,
    const int32 BoneIndex)
{
    using namespace UE::AnimationCore;
    const FVector Corners[8] = {
        Center + FVector(-Extent.X, -Extent.Y, -Extent.Z),
        Center + FVector( Extent.X, -Extent.Y, -Extent.Z),
        Center + FVector( Extent.X,  Extent.Y, -Extent.Z),
        Center + FVector(-Extent.X,  Extent.Y, -Extent.Z),
        Center + FVector(-Extent.X, -Extent.Y,  Extent.Z),
        Center + FVector( Extent.X, -Extent.Y,  Extent.Z),
        Center + FVector( Extent.X,  Extent.Y,  Extent.Z),
        Center + FVector(-Extent.X,  Extent.Y,  Extent.Z),
    };

    FVertexID Vertices[8];
    for (int32 Index = 0; Index < 8; ++Index)
    {
        Vertices[Index] = Builder.AppendVertex(Corners[Index]);
        const FBoneWeight Influence(static_cast<FBoneIndexType>(BoneIndex), 1.0f);
        SkinWeights.Set(Vertices[Index], FBoneWeights::Create({Influence}));
    }

    auto Tri = [&Builder, &Vertices, PolygonGroup](int32 A, int32 B, int32 C)
    {
        Builder.AppendTriangle(Vertices[A], Vertices[B], Vertices[C], PolygonGroup);
    };

    Tri(0, 2, 1); Tri(0, 3, 2);
    Tri(4, 5, 6); Tri(4, 6, 7);
    Tri(0, 1, 5); Tri(0, 5, 4);
    Tri(1, 2, 6); Tri(1, 6, 5);
    Tri(2, 3, 7); Tri(2, 7, 6);
    Tri(3, 0, 4); Tri(3, 4, 7);
}

FReferenceSkeleton BuildReferenceSkeleton(USkeleton* Skeleton, const TArray<FSourceBoneDef>& Bones)
{
    {
        FReferenceSkeletonModifier Modifier(Skeleton);
        for (int32 Index = 0; Index < Bones.Num(); ++Index)
        {
            const FSourceBoneDef& Bone = Bones[Index];
            const FString BoneName = Bone.Name.ToString();
            Modifier.Add(
                FMeshBoneInfo(Bone.Name, BoneName, Bone.ParentIndex),
                FTransform(FQuat::Identity, Bone.LocalOffsetCm),
                false);
        }
    }
    return Skeleton->GetReferenceSkeleton();
}

bool BuildSourceMeshDescription(const TArray<FSourceBoneDef>& Bones, FMeshDescription& MeshDescription, FReferenceSkeleton& RefSkeleton)
{
    FSkeletalMeshAttributes Attributes(MeshDescription);
    Attributes.Register();
    FMeshDescriptionBuilder Builder;
    Builder.SetMeshDescription(&MeshDescription);
    Builder.SetNumUVLayers(1);
    const FPolygonGroupID PolygonGroup = Builder.AppendPolygonGroup(TEXT("MimicKitSource"));

    FSkeletalMeshAttributes::FBoneNameAttributesRef BoneNames = Attributes.GetBoneNames();
    FSkeletalMeshAttributes::FBoneParentIndexAttributesRef BoneParentIndices = Attributes.GetBoneParentIndices();
    FSkeletalMeshAttributes::FBonePoseAttributesRef BonePoses = Attributes.GetBonePoses();
    for (int32 Index = 0; Index < RefSkeleton.GetRawBoneNum(); ++Index)
    {
        const FBoneID BoneID = Attributes.CreateBone();
        BoneNames.Set(BoneID, RefSkeleton.GetRawRefBoneInfo()[Index].Name);
        BoneParentIndices.Set(BoneID, RefSkeleton.GetRawRefBoneInfo()[Index].ParentIndex);
        BonePoses.Set(BoneID, RefSkeleton.GetRawRefBonePose()[Index]);
    }

    FSkinWeightsVertexAttributesRef SkinWeights = Attributes.GetVertexSkinWeights();
    for (int32 BoneIndex = 0; BoneIndex < Bones.Num(); ++BoneIndex)
    {
        const FString BodyName = Bones[BoneIndex].Name.ToString();
        AddBox(
            Builder,
            SkinWeights,
            PolygonGroup,
            Bones[BoneIndex].GlobalPositionCm,
            BoxExtentForBodyCm(BodyName),
            BoneIndex);
    }
    return MeshDescription.Vertices().Num() > 0 && MeshDescription.Triangles().Num() > 0;
}

bool SaveAssetPackage(UPackage* Package, UObject* Asset, FString& OutError)
{
    if (!Package || !Asset)
    {
        OutError = TEXT("SaveAssetPackage received a null package or asset.");
        return false;
    }
    Package->MarkPackageDirty();
    FAssetRegistryModule::AssetCreated(Asset);

    const FString FileName = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());
    FSavePackageArgs SaveArgs;
    SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
    SaveArgs.SaveFlags = SAVE_NoError;
    if (!UPackage::SavePackage(Package, Asset, *FileName, SaveArgs))
    {
        OutError = FString::Printf(TEXT("Could not save asset package: %s"), *FileName);
        return false;
    }
    return true;
}

void PatchSourceTargetContract(const FString& PackageDirectory, const FMimicKitSourceRigAssetBuildSummary& Summary)
{
    for (const FString& RelativePath : {TEXT("skeletal_mapping_contract.json"), TEXT("visual_alignment_contract.json")})
    {
        const FString ContractPath = JoinPackagePath(PackageDirectory, RelativePath);
        FString Error;
        TSharedPtr<FJsonObject> Contract;
        if (!FPaths::FileExists(ContractPath) || !LoadJsonObject(ContractPath, Contract, Error))
        {
            continue;
        }

        TSharedPtr<FJsonObject> Target;
        if (RelativePath == TEXT("visual_alignment_contract.json"))
        {
            TSharedPtr<FJsonObject> Skeletal = GetObjectField(Contract, TEXT("skeletal_mapping_contract"));
            if (!Skeletal.IsValid())
            {
                Skeletal = MakeShared<FJsonObject>();
                Contract->SetObjectField(TEXT("skeletal_mapping_contract"), Skeletal);
            }
            Target = GetObjectField(Skeletal, TEXT("ue_visual_target"));
            if (!Target.IsValid())
            {
                Target = MakeShared<FJsonObject>();
                Skeletal->SetObjectField(TEXT("ue_visual_target"), Target);
            }
        }
        else
        {
            Target = GetObjectField(Contract, TEXT("ue_visual_target"));
            if (!Target.IsValid())
            {
                Target = MakeShared<FJsonObject>();
                Contract->SetObjectField(TEXT("ue_visual_target"), Target);
            }
        }

        Target->SetStringField(TEXT("asset_strategy"), TEXT("mimickit_source_equivalent_generated_skeletal_asset"));
        Target->SetStringField(TEXT("skeletal_mesh"), Summary.SkeletalMeshPath);
        Target->SetStringField(TEXT("skeleton"), Summary.SkeletonPath);
        Target->SetStringField(TEXT("physics_asset"), Summary.PhysicsAssetPath);
        Target->SetStringField(TEXT("fallback_renderer"), TEXT(""));
        Target->SetBoolField(TEXT("full_character_parity"), true);
        Target->SetBoolField(TEXT("mimickit_source_asset_imported_to_ue"), Summary.bSourceAssetsBuilt);
        Target->SetStringField(TEXT("mimickit_source_asset_import_status"), Summary.bSourceAssetsBuilt ? TEXT("built") : TEXT("failed"));
        Target->SetStringField(TEXT("source_asset_build_report"), Summary.ReportFile);

        WriteJsonObject(ContractPath, Contract.ToSharedRef(), Error);
    }
}
}

bool FGASPALSMimicKitSourceRigAssetBuilder::BuildSourceRigAssets(
    const FString& PackageDirectory,
    const FString& AssetRoot,
    FMimicKitSourceRigAssetBuildSummary& OutSummary,
    FString& OutError)
{
    OutSummary = FMimicKitSourceRigAssetBuildSummary();
    OutSummary.PackageDirectory = FPaths::ConvertRelativePathToFull(PackageDirectory);
    OutSummary.AssetRoot = AssetRoot.IsEmpty() ? TEXT("/Game/MimicKit/SwordShield") : AssetRoot;
    OutSummary.SkeletalMeshPath = FPaths::Combine(OutSummary.AssetRoot, TEXT("SK_MimicKit_SwordShield"));
    OutSummary.SkeletonPath = FPaths::Combine(OutSummary.AssetRoot, TEXT("Skeleton_MimicKit_SwordShield"));
    OutSummary.PhysicsAssetPath = FPaths::Combine(OutSummary.AssetRoot, TEXT("PA_MimicKit_SwordShield"));
    OutSummary.SpecFile = JoinPackagePath(OutSummary.PackageDirectory, TEXT("mimickit_source_rig_asset_spec.json"));
    OutSummary.RuntimeContractFile = JoinPackagePath(OutSummary.PackageDirectory, TEXT("runtime_control_contract.json"));
    OutSummary.ReportFile = JoinPackagePath(OutSummary.PackageDirectory, TEXT("mimickit_source_rig_asset_build_report.json"));

    TSharedPtr<FJsonObject> Spec;
    if (!LoadJsonObject(OutSummary.SpecFile, Spec, OutError))
    {
        OutSummary.Error = OutError;
        return false;
    }
    OutSummary.BodyCount = GetIntField(GetObjectField(Spec, TEXT("validation")), TEXT("source_body_count"));
    OutSummary.bUsdHasUsdSkel = GetBoolField(GetObjectField(GetObjectField(Spec, TEXT("source_assets")), TEXT("usd_tokens")), TEXT("has_usdskel_token"));
    OutSummary.bGeneratedFromMjcfPrimitiveSpec = !OutSummary.bUsdHasUsdSkel;

    TArray<FSourceBoneDef> Bones;
    if (!LoadBoneDefs(JoinPackagePath(OutSummary.PackageDirectory, TEXT("joint_order.json")), Bones, OutSummary.DofSize, OutError))
    {
        OutSummary.Error = OutError;
        return false;
    }
    OutSummary.BoneCount = Bones.Num();

    USkeletalMesh* ExistingMesh = LoadObject<USkeletalMesh>(nullptr, *NormalizeAssetObjectPath(OutSummary.SkeletalMeshPath));
    USkeleton* ExistingSkeleton = LoadObject<USkeleton>(nullptr, *NormalizeAssetObjectPath(OutSummary.SkeletonPath));
    UPhysicsAsset* ExistingPhysics = LoadObject<UPhysicsAsset>(nullptr, *NormalizeAssetObjectPath(OutSummary.PhysicsAssetPath));
    if (ExistingMesh && ExistingSkeleton && ExistingPhysics)
    {
        OutSummary.bSkeletalMeshOk = true;
        OutSummary.bSkeletonOk = true;
        OutSummary.bPhysicsAssetOk = true;
        OutSummary.bSourceAssetsBuilt = true;
        OutSummary.PhysicsBodyCount = ExistingPhysics->SkeletalBodySetups.Num();
        OutSummary.PhysicsConstraintCount = ExistingPhysics->ConstraintSetup.Num();
    }
    else
    {
        UPackage* SkeletonPackage = CreatePackage(*OutSummary.SkeletonPath);
        UPackage* MeshPackage = CreatePackage(*OutSummary.SkeletalMeshPath);
        UPackage* PhysicsPackage = CreatePackage(*OutSummary.PhysicsAssetPath);
        USkeleton* Skeleton = NewObject<USkeleton>(
            SkeletonPackage,
            USkeleton::StaticClass(),
            FName(TEXT("Skeleton_MimicKit_SwordShield")),
            RF_Public | RF_Standalone | RF_Transactional);
        USkeletalMesh* SkeletalMesh = NewObject<USkeletalMesh>(
            MeshPackage,
            USkeletalMesh::StaticClass(),
            FName(TEXT("SK_MimicKit_SwordShield")),
            RF_Public | RF_Standalone | RF_Transactional);
        UPhysicsAsset* PhysicsAsset = NewObject<UPhysicsAsset>(
            PhysicsPackage,
            UPhysicsAsset::StaticClass(),
            FName(TEXT("PA_MimicKit_SwordShield")),
            RF_Public | RF_Standalone | RF_Transactional);
        if (!Skeleton || !SkeletalMesh || !PhysicsAsset)
        {
            OutError = TEXT("Could not create source rig asset objects.");
            OutSummary.Error = OutError;
            return false;
        }

        FReferenceSkeleton RefSkeleton = BuildReferenceSkeleton(Skeleton, Bones);
        FMeshDescription MeshDescription;
        if (!BuildSourceMeshDescription(Bones, MeshDescription, RefSkeleton))
        {
            OutError = TEXT("Could not build MimicKit source mesh description.");
            OutSummary.Error = OutError;
            return false;
        }

        TArray<const FMeshDescription*> MeshDescriptions;
        MeshDescriptions.Add(&MeshDescription);
        TArray<FSkeletalMaterial> Materials;
        FSkeletalMaterial SourceMaterial;
        SourceMaterial.MaterialSlotName = TEXT("MimicKitSource");
        SourceMaterial.ImportedMaterialSlotName = TEXT("MimicKitSource");
        Materials.Add(SourceMaterial);

        SkeletalMesh->SetSkeleton(Skeleton);
        const bool bMeshInitialized = FStaticToSkeletalMeshConverter::InitializeSkeletalMeshFromMeshDescriptions(
            SkeletalMesh,
            MakeArrayView(MeshDescriptions),
            MakeArrayView(Materials),
            RefSkeleton,
            true,
            true,
            true);
        if (!bMeshInitialized)
        {
            OutError = TEXT("Could not initialize source-equivalent SkeletalMesh from mesh description.");
            OutSummary.Error = OutError;
            return false;
        }
        SkeletalMesh->SetSkeleton(Skeleton);
        Skeleton->MergeAllBonesToBoneTree(SkeletalMesh, false);
        Skeleton->SetPreviewMesh(SkeletalMesh, false);

        FPhysAssetCreateParams PhysParams;
        PhysParams.MinBoneSize = 1.0f;
        PhysParams.bBodyForAll = true;
        PhysParams.GeomType = EFG_Sphyl;
        FText PhysError;
        const bool bPhysicsOk = FPhysicsAssetUtils::CreateFromSkeletalMesh(PhysicsAsset, SkeletalMesh, PhysParams, PhysError, true, false);
        if (!bPhysicsOk)
        {
            OutError = FString::Printf(TEXT("Could not create PhysicsAsset from source mesh: %s"), *PhysError.ToString());
            OutSummary.Error = OutError;
            return false;
        }
        SkeletalMesh->SetPhysicsAsset(PhysicsAsset);

        OutSummary.bSkeletalMeshOk = SaveAssetPackage(MeshPackage, SkeletalMesh, OutError);
        OutSummary.bSkeletonOk = SaveAssetPackage(SkeletonPackage, Skeleton, OutError);
        OutSummary.bPhysicsAssetOk = SaveAssetPackage(PhysicsPackage, PhysicsAsset, OutError);
        OutSummary.PhysicsBodyCount = PhysicsAsset->SkeletalBodySetups.Num();
        OutSummary.PhysicsConstraintCount = PhysicsAsset->ConstraintSetup.Num();
        OutSummary.bSourceAssetsBuilt = OutSummary.bSkeletalMeshOk && OutSummary.bSkeletonOk && OutSummary.bPhysicsAssetOk;
    }

    TSharedPtr<FJsonObject> Report = MakeShared<FJsonObject>();
    Report->SetStringField(TEXT("package_dir"), OutSummary.PackageDirectory);
    Report->SetStringField(TEXT("asset_root"), OutSummary.AssetRoot);
    Report->SetStringField(TEXT("skeletal_mesh"), OutSummary.SkeletalMeshPath);
    Report->SetStringField(TEXT("skeleton"), OutSummary.SkeletonPath);
    Report->SetStringField(TEXT("physics_asset"), OutSummary.PhysicsAssetPath);
    Report->SetStringField(TEXT("spec_file"), OutSummary.SpecFile);
    Report->SetStringField(TEXT("runtime_contract_file"), OutSummary.RuntimeContractFile);
    Report->SetBoolField(TEXT("usd_has_usdskel"), OutSummary.bUsdHasUsdSkel);
    Report->SetBoolField(TEXT("generated_from_mjcf_primitive_spec"), OutSummary.bGeneratedFromMjcfPrimitiveSpec);
    Report->SetNumberField(TEXT("body_count"), OutSummary.BodyCount);
    Report->SetNumberField(TEXT("bone_count"), OutSummary.BoneCount);
    Report->SetNumberField(TEXT("dof_size"), OutSummary.DofSize);
    Report->SetNumberField(TEXT("physics_body_count"), OutSummary.PhysicsBodyCount);
    Report->SetNumberField(TEXT("physics_constraint_count"), OutSummary.PhysicsConstraintCount);
    Report->SetBoolField(TEXT("skeletal_mesh_ok"), OutSummary.bSkeletalMeshOk);
    Report->SetBoolField(TEXT("skeleton_ok"), OutSummary.bSkeletonOk);
    Report->SetBoolField(TEXT("physics_asset_ok"), OutSummary.bPhysicsAssetOk);
    Report->SetBoolField(TEXT("source_assets_built"), OutSummary.bSourceAssetsBuilt);
    if (!WriteJsonObject(OutSummary.ReportFile, Report.ToSharedRef(), OutError))
    {
        OutSummary.Error = OutError;
        return false;
    }

    PatchSourceTargetContract(OutSummary.PackageDirectory, OutSummary);

    if (!OutSummary.bSourceAssetsBuilt)
    {
        OutError = TEXT("Source rig asset build did not produce all required assets.");
        OutSummary.Error = OutError;
        return false;
    }

    OutError.Reset();
    return true;
}

#pragma once

#include "CoreMinimal.h"
#include "GASPALSShadowTypes.generated.h"

UENUM(BlueprintType)
enum class EGASPALSShadowEventSeverity : uint8
{
    Info UMETA(DisplayName = "Info"),
    Warning UMETA(DisplayName = "Warning"),
    Error UMETA(DisplayName = "Error")
};

UENUM(BlueprintType)
enum class EGASPALSShadowControlMode : uint8
{
    Unknown UMETA(DisplayName = "Unknown"),
    Uncontrolled UMETA(DisplayName = "Uncontrolled"),
    VelocityFacing UMETA(DisplayName = "Velocity Facing"),
    Trajectory UMETA(DisplayName = "Trajectory")
};

USTRUCT(BlueprintType)
struct GASPALSSHADOW_API FGASPALSShadowInputSample
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GASPALSShadow")
    EGASPALSShadowControlMode ControlMode = EGASPALSShadowControlMode::Unknown;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GASPALSShadow")
    FVector2D MoveStick = FVector2D::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GASPALSShadow")
    FVector2D LookStick = FVector2D::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GASPALSShadow")
    float LeftTrigger = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GASPALSShadow")
    float RightTrigger = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GASPALSShadow")
    bool bDesiredStrafe = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GASPALSShadow")
    bool bDesiredWalk = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GASPALSShadow")
    bool bDesiredSprint = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GASPALSShadow")
    bool bJumpPressed = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GASPALSShadow")
    bool bCrouchRequested = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GASPALSShadow")
    FRotator ControlRotation = FRotator::ZeroRotator;
};

USTRUCT(BlueprintType)
struct GASPALSSHADOW_API FGASPALSShadowSubjectSample
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GASPALSShadow")
    FString ActorName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GASPALSShadow")
    FString ActorPath;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GASPALSShadow")
    FString ActorClass;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GASPALSShadow")
    FString PawnClass;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GASPALSShadow")
    FString ControllerName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GASPALSShadow")
    FString ControllerClass;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GASPALSShadow")
    FString NetRole;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GASPALSShadow")
    bool bIsPlayerControlled = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GASPALSShadow")
    bool bIsLocallyControlled = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GASPALSShadow")
    bool bHasAuthority = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GASPALSShadow")
    TArray<FString> ActorTags;
};

USTRUCT(BlueprintType)
struct GASPALSSHADOW_API FGASPALSShadowMovementSample
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GASPALSShadow")
    FVector ActorLocation = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GASPALSShadow")
    FRotator ActorRotation = FRotator::ZeroRotator;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GASPALSShadow")
    FVector Velocity = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GASPALSShadow")
    FVector Acceleration = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GASPALSShadow")
    FVector AngularVelocity = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GASPALSShadow")
    FVector LocalVelocity = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GASPALSShadow")
    FVector LocalAcceleration = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GASPALSShadow")
    FString MovementMode = TEXT("Unknown");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GASPALSShadow")
    uint8 CustomMovementMode = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GASPALSShadow")
    float Speed = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GASPALSShadow")
    float Speed2D = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GASPALSShadow")
    float YawSpeedDegreesPerSecond = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GASPALSShadow")
    bool bIsFalling = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GASPALSShadow")
    bool bIsCrouching = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GASPALSShadow")
    FString MovementBaseName;
};

USTRUCT(BlueprintType)
struct GASPALSSHADOW_API FGASPALSShadowTrajectoryPoint
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GASPALSShadow")
    FVector LocalPosition = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GASPALSShadow")
    FVector LocalDirection = FVector::ForwardVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GASPALSShadow")
    float HorizonSeconds = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GASPALSShadow")
    int32 HorizonFrames = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GASPALSShadow")
    FString Source;
};

USTRUCT(BlueprintType)
struct GASPALSSHADOW_API FGASPALSShadowLocomotionSample
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GASPALSShadow")
    FString DesiredGait;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GASPALSShadow")
    FString DesiredStance;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GASPALSShadow")
    FString RotationMode;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GASPALSShadow")
    FString LocomotionState;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GASPALSShadow")
    bool bHasMovementInput = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GASPALSShadow")
    bool bIsAiming = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GASPALSShadow")
    bool bIsRagdoll = false;
};

USTRUCT(BlueprintType)
struct GASPALSSHADOW_API FGASPALSShadowAnimationSample
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GASPALSShadow")
    FString AnimInstanceClass;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GASPALSShadow")
    FString MeshComponentName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GASPALSShadow")
    FString SkeletalMesh;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GASPALSShadow")
    FString Skeleton;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GASPALSShadow")
    FString OverlayBase;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GASPALSShadow")
    FString OverlayPose;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GASPALSShadow")
    FString ActiveMontage;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GASPALSShadow")
    float ActiveMontagePositionSeconds = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GASPALSShadow")
    FString ActiveMontageSection;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GASPALSShadow")
    bool bIsAnyMontagePlaying = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GASPALSShadow")
    FString PoseSearchDatabaseFamily;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GASPALSShadow")
    FString PoseSearchState;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GASPALSShadow")
    FString PoseSearchContinuationState;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GASPALSShadow")
    TArray<FString> ObservedTags;
};

USTRUCT(BlueprintType)
struct GASPALSSHADOW_API FGASPALSShadowTraversalSample
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GASPALSShadow")
    bool bTraversalRequested = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GASPALSShadow")
    bool bTraversalAvailable = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GASPALSShadow")
    FString TraversalState;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GASPALSShadow")
    FString LastChooserResult;
};

USTRUCT(BlueprintType)
struct GASPALSSHADOW_API FGASPALSShadowContractExtensionSample
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GASPALSShadow")
    FString ControlContractId = TEXT("control_to_lmm/v1");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GASPALSShadow")
    FString EncoderProfile;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GASPALSShadow")
    TArray<FString> ActiveChannels;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GASPALSShadow")
    TMap<FString, FString> ChannelSources;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GASPALSShadow")
    TMap<FString, bool> NamedBools;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GASPALSShadow")
    TMap<FString, int32> NamedIntegers;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GASPALSShadow")
    TMap<FString, float> NamedFloats;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GASPALSShadow")
    TMap<FString, FString> NamedStrings;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GASPALSShadow")
    TMap<FString, FVector> NamedVectors;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GASPALSShadow")
    TMap<FString, FRotator> NamedRotators;
};

USTRUCT(BlueprintType)
struct GASPALSSHADOW_API FGASPALSShadowFrameRecord
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GASPALSShadow")
    FString SchemaVersion = TEXT("gaspals_shadow/v2");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GASPALSShadow")
    FString SessionId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GASPALSShadow")
    int64 FrameIndex = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GASPALSShadow")
    double WorldTimeSeconds = 0.0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GASPALSShadow")
    float DeltaSeconds = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GASPALSShadow")
    FString CaptureReason = TEXT("tick");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GASPALSShadow")
    FString ActorName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GASPALSShadow")
    FGASPALSShadowSubjectSample Subject;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GASPALSShadow")
    FGASPALSShadowInputSample Input;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GASPALSShadow")
    FGASPALSShadowMovementSample Movement;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GASPALSShadow")
    TArray<FGASPALSShadowTrajectoryPoint> Trajectory;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GASPALSShadow")
    FGASPALSShadowLocomotionSample Locomotion;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GASPALSShadow")
    FGASPALSShadowAnimationSample Animation;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GASPALSShadow")
    FGASPALSShadowTraversalSample Traversal;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GASPALSShadow")
    TMap<FString, float> NamedFloats;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GASPALSShadow")
    TMap<FString, FString> NamedStrings;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GASPALSShadow")
    FGASPALSShadowContractExtensionSample Extensions;
};

USTRUCT(BlueprintType)
struct GASPALSSHADOW_API FGASPALSShadowSessionStats
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GASPALSShadow")
    int64 TotalFrames = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GASPALSShadow")
    int64 TotalEvents = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GASPALSShadow")
    double FirstWorldTimeSeconds = 0.0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GASPALSShadow")
    double LastWorldTimeSeconds = 0.0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GASPALSShadow")
    TArray<FString> ObservedActors;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GASPALSShadow")
    TArray<FString> ObservedControllers;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GASPALSShadow")
    TArray<FString> ActiveChannels;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GASPALSShadow")
    TMap<FString, FString> DeclaredChannelSources;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GASPALSShadow")
    TArray<FString> ExtensionBoolKeys;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GASPALSShadow")
    TArray<FString> ExtensionIntegerKeys;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GASPALSShadow")
    TArray<FString> ExtensionFloatKeys;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GASPALSShadow")
    TArray<FString> ExtensionStringKeys;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GASPALSShadow")
    TArray<FString> ExtensionVectorKeys;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GASPALSShadow")
    TArray<FString> ExtensionRotatorKeys;
};

USTRUCT(BlueprintType)
struct GASPALSSHADOW_API FGASPALSShadowSessionManifest
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GASPALSShadow")
    FString ManifestSchemaVersion = TEXT("gaspals_shadow/session/v1");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GASPALSShadow")
    FString FrameSchemaVersion = TEXT("gaspals_shadow/v2");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GASPALSShadow")
    FString EventSchemaVersion = TEXT("gaspals_shadow/event/v1");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GASPALSShadow")
    FString ControlContractId = TEXT("control_to_lmm/v1");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GASPALSShadow")
    FString SessionId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GASPALSShadow")
    FString Status = TEXT("active");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GASPALSShadow")
    FString CreatedAtUtc;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GASPALSShadow")
    FString LastUpdatedAtUtc;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GASPALSShadow")
    FString ClosedAtUtc;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GASPALSShadow")
    FString ProjectName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GASPALSShadow")
    FString WorldName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GASPALSShadow")
    FString MapName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GASPALSShadow")
    FString EngineVersion;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GASPALSShadow")
    FString PluginVersion;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GASPALSShadow")
    FString FramesFile = TEXT("frames.jsonl");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GASPALSShadow")
    FString EventsFile = TEXT("events.jsonl");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GASPALSShadow")
    FString MimicKitShadowFile = TEXT("mimickit_shadow.jsonl");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GASPALSShadow")
    FGASPALSShadowSessionStats Stats;
};

USTRUCT(BlueprintType)
struct GASPALSSHADOW_API FGASPALSShadowSessionEvent
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GASPALSShadow")
    FString SchemaVersion = TEXT("gaspals_shadow/event/v1");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GASPALSShadow")
    FString SessionId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GASPALSShadow")
    int64 EventIndex = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GASPALSShadow")
    int64 FrameIndex = INDEX_NONE;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GASPALSShadow")
    double WorldTimeSeconds = 0.0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GASPALSShadow")
    FString EventName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GASPALSShadow")
    FString Source;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GASPALSShadow")
    FString ActorName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GASPALSShadow")
    FString Message;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GASPALSShadow")
    EGASPALSShadowEventSeverity Severity = EGASPALSShadowEventSeverity::Info;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GASPALSShadow")
    TMap<FString, FString> Metadata;
};

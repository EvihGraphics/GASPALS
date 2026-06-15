#include "GASPALSShadowObserverComponent.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/Skeleton.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "GASPALSShadowWorldSubsystem.h"

namespace
{
bool FindBoolValue(const TMap<FString, bool>& Map, const TCHAR* Key, bool DefaultValue = false)
{
    if (const bool* Value = Map.Find(Key))
    {
        return *Value;
    }
    return DefaultValue;
}

int32 FindIntegerValue(const TMap<FString, int32>& Map, const TCHAR* Key, int32 DefaultValue = 0)
{
    if (const int32* Value = Map.Find(Key))
    {
        return *Value;
    }
    return DefaultValue;
}

float FindFloatValue(const TMap<FString, float>& Map, const TCHAR* Key, float DefaultValue = 0.0f)
{
    if (const float* Value = Map.Find(Key))
    {
        return *Value;
    }
    return DefaultValue;
}

FString FindStringValue(const TMap<FString, FString>& Map, const TCHAR* Key)
{
    if (const FString* Value = Map.Find(Key))
    {
        return *Value;
    }
    return FString();
}
}

UGASPALSShadowObserverComponent::UGASPALSShadowObserverComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = true;
}

void UGASPALSShadowObserverComponent::BeginPlay()
{
    Super::BeginPlay();
    TimeSinceLastCapture = CaptureIntervalSeconds;
    RecordLifecycleEvent(TEXT("observer_begin_play"), TEXT("Observer component attached to owner."), EGASPALSShadowEventSeverity::Info);
}

void UGASPALSShadowObserverComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    RecordLifecycleEvent(
        TEXT("observer_end_play"),
        FString::Printf(TEXT("Observer component ending play (%s)."), *UEnum::GetValueAsString(EndPlayReason)),
        EGASPALSShadowEventSeverity::Info);
    Super::EndPlay(EndPlayReason);
}

void UGASPALSShadowObserverComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (!bAutoCaptureOwnerState)
    {
        return;
    }

    TimeSinceLastCapture += DeltaTime;
    if (TimeSinceLastCapture < CaptureIntervalSeconds)
    {
        return;
    }

    TimeSinceLastCapture = 0.0f;
    RecordFrame(BuildOwnerSnapshot());
}

FGASPALSShadowFrameRecord UGASPALSShadowObserverComponent::BuildOwnerSnapshot() const
{
    FGASPALSShadowFrameRecord Record;

    const AActor* Owner = GetOwner();
    const UWorld* World = GetWorld();

    Record.FrameIndex = CaptureCounter;
    Record.WorldTimeSeconds = World ? World->GetTimeSeconds() : 0.0;
    Record.DeltaSeconds = World ? World->GetDeltaSeconds() : 0.0f;
    Record.ActorName = Owner ? Owner->GetName() : TEXT("None");

    if (World)
    {
        if (const UGASPALSShadowWorldSubsystem* Subsystem = World->GetSubsystem<UGASPALSShadowWorldSubsystem>())
        {
            Record.SessionId = Subsystem->GetActiveSessionId();
        }
    }

    if (!Owner)
    {
        return Record;
    }

    Record.Subject.ActorName = Owner->GetName();
    Record.Subject.ActorPath = Owner->GetPathName();
    Record.Subject.ActorClass = Owner->GetClass() ? Owner->GetClass()->GetName() : TEXT("");
    Record.Subject.bHasAuthority = Owner->HasAuthority();
    Record.Subject.NetRole = UEnum::GetValueAsString(Owner->GetLocalRole());
    for (const FName& Tag : Owner->Tags)
    {
        const FString TagString = Tag.ToString();
        Record.Subject.ActorTags.Add(TagString);
        Record.Animation.ObservedTags.Add(TagString);
    }

    Record.Movement.ActorLocation = Owner->GetActorLocation();
    Record.Movement.ActorRotation = Owner->GetActorRotation();
    Record.Movement.Velocity = Owner->GetVelocity();
    Record.Movement.Speed = Record.Movement.Velocity.Size();
    Record.Movement.Speed2D = FVector(Record.Movement.Velocity.X, Record.Movement.Velocity.Y, 0.0f).Size();
    Record.Movement.LocalVelocity = Owner->GetActorTransform().InverseTransformVectorNoScale(Record.Movement.Velocity);

    if (const UPrimitiveComponent* RootPrimitive = Cast<UPrimitiveComponent>(Owner->GetRootComponent()))
    {
        Record.Movement.AngularVelocity = RootPrimitive->GetPhysicsAngularVelocityInDegrees();
        Record.Movement.YawSpeedDegreesPerSecond = Record.Movement.AngularVelocity.Z;
    }

    if (const APawn* Pawn = Cast<APawn>(Owner))
    {
        Record.Subject.PawnClass = Pawn->GetClass() ? Pawn->GetClass()->GetName() : TEXT("");
        Record.Subject.bIsPlayerControlled = Pawn->IsPlayerControlled();
        Record.Subject.bIsLocallyControlled = Pawn->IsLocallyControlled();

        if (const AController* Controller = Pawn->GetController())
        {
            Record.Input.ControlRotation = Controller->GetControlRotation();
            Record.Subject.ControllerName = Controller->GetName();
            Record.Subject.ControllerClass = Controller->GetClass() ? Controller->GetClass()->GetName() : TEXT("");
        }
    }

    if (const ACharacter* Character = Cast<ACharacter>(Owner))
    {
        if (const UCharacterMovementComponent* MovementComponent = Character->GetCharacterMovement())
        {
            Record.Movement.Acceleration = MovementComponent->GetCurrentAcceleration();
            Record.Movement.LocalAcceleration = Owner->GetActorTransform().InverseTransformVectorNoScale(Record.Movement.Acceleration);
            Record.Movement.MovementMode = UEnum::GetValueAsString(MovementComponent->MovementMode);
            Record.Movement.CustomMovementMode = MovementComponent->CustomMovementMode;
            Record.Movement.bIsFalling = MovementComponent->IsFalling();
            Record.Movement.bIsCrouching = MovementComponent->IsCrouching();
            if (const UPrimitiveComponent* MovementBase = MovementComponent->GetMovementBase())
            {
                Record.Movement.MovementBaseName = MovementBase->GetName();
            }
        }

        if (const USkeletalMeshComponent* Mesh = Character->GetMesh())
        {
            Record.Animation.MeshComponentName = Mesh->GetName();

            if (const UAnimInstance* AnimInstance = Mesh->GetAnimInstance())
            {
                Record.Animation.AnimInstanceClass = AnimInstance->GetClass() ? AnimInstance->GetClass()->GetName() : TEXT("");
                Record.Animation.bIsAnyMontagePlaying = AnimInstance->IsAnyMontagePlaying();

                if (const UAnimMontage* ActiveMontage = AnimInstance->GetCurrentActiveMontage())
                {
                    Record.Animation.ActiveMontage = ActiveMontage->GetName();
                    Record.Animation.ActiveMontagePositionSeconds = AnimInstance->Montage_GetPosition(ActiveMontage);
                    const int32 SectionIndex = ActiveMontage->GetSectionIndexFromPosition(Record.Animation.ActiveMontagePositionSeconds);
                    if (SectionIndex != INDEX_NONE)
                    {
                        Record.Animation.ActiveMontageSection = ActiveMontage->GetSectionName(SectionIndex).ToString();
                    }
                }
            }

            if (const USkeletalMesh* SkeletalMesh = Mesh->GetSkeletalMeshAsset())
            {
                Record.Animation.SkeletalMesh = SkeletalMesh->GetName();
                if (const USkeleton* Skeleton = SkeletalMesh->GetSkeleton())
                {
                    Record.Animation.Skeleton = Skeleton->GetName();
                }
            }
        }
    }

    const int32 ControlModeValue = FindIntegerValue(PendingNamedIntegers, TEXT("ControlMode"), -1);
    if (ControlModeValue >= static_cast<int32>(EGASPALSShadowControlMode::Unknown)
        && ControlModeValue <= static_cast<int32>(EGASPALSShadowControlMode::Trajectory))
    {
        Record.Input.ControlMode = static_cast<EGASPALSShadowControlMode>(ControlModeValue);
    }
    else
    {
        Record.Input.ControlMode = EGASPALSShadowControlMode::Unknown;
    }
    Record.Input.LeftTrigger = FindFloatValue(PendingNamedFloats, TEXT("LeftTrigger"));
    Record.Input.RightTrigger = FindFloatValue(PendingNamedFloats, TEXT("RightTrigger"));
    Record.Input.bDesiredStrafe = FindBoolValue(PendingNamedBools, TEXT("DesiredStrafe"));
    Record.Input.bDesiredWalk = FindBoolValue(PendingNamedBools, TEXT("DesiredWalk"));
    Record.Input.bDesiredSprint = FindBoolValue(PendingNamedBools, TEXT("DesiredSprint"));
    Record.Input.bJumpPressed = FindBoolValue(PendingNamedBools, TEXT("JumpPressed"));
    Record.Input.bCrouchRequested = FindBoolValue(PendingNamedBools, TEXT("CrouchRequested"));

    Record.Locomotion.DesiredGait = FindStringValue(PendingNamedStrings, TEXT("DesiredGait"));
    Record.Locomotion.DesiredStance = FindStringValue(PendingNamedStrings, TEXT("DesiredStance"));
    Record.Locomotion.RotationMode = FindStringValue(PendingNamedStrings, TEXT("RotationMode"));
    Record.Locomotion.LocomotionState = FindStringValue(PendingNamedStrings, TEXT("LocomotionState"));
    Record.Locomotion.bHasMovementInput = FindBoolValue(PendingNamedBools, TEXT("HasMovementInput"));
    Record.Locomotion.bIsAiming = FindBoolValue(PendingNamedBools, TEXT("IsAiming"));
    Record.Locomotion.bIsRagdoll = FindBoolValue(PendingNamedBools, TEXT("IsRagdoll"));

    Record.Animation.OverlayBase = FindStringValue(PendingNamedStrings, TEXT("OverlayBase"));
    Record.Animation.OverlayPose = FindStringValue(PendingNamedStrings, TEXT("OverlayPose"));
    Record.Animation.PoseSearchDatabaseFamily = FindStringValue(PendingNamedStrings, TEXT("PoseSearchDatabaseFamily"));
    Record.Animation.PoseSearchState = FindStringValue(PendingNamedStrings, TEXT("PoseSearchState"));
    Record.Animation.PoseSearchContinuationState = FindStringValue(PendingNamedStrings, TEXT("PoseSearchContinuationState"));

    Record.Traversal.bTraversalRequested = FindBoolValue(PendingNamedBools, TEXT("TraversalRequested"));
    Record.Traversal.bTraversalAvailable = FindBoolValue(PendingNamedBools, TEXT("TraversalAvailable"));
    Record.Traversal.TraversalState = FindStringValue(PendingNamedStrings, TEXT("TraversalState"));
    Record.Traversal.LastChooserResult = FindStringValue(PendingNamedStrings, TEXT("LastChooserResult"));

    Record.NamedFloats = PendingNamedFloats;
    Record.NamedStrings = PendingNamedStrings;

    Record.Extensions.ControlContractId = PendingControlContractId;
    Record.Extensions.EncoderProfile = PendingEncoderProfile;
    Record.Extensions.ActiveChannels = PendingActiveChannels.Array();
    Record.Extensions.ChannelSources = PendingChannelSources;
    Record.Extensions.NamedBools = PendingNamedBools;
    Record.Extensions.NamedIntegers = PendingNamedIntegers;
    Record.Extensions.NamedFloats = PendingNamedFloats;
    Record.Extensions.NamedStrings = PendingNamedStrings;
    Record.Extensions.NamedVectors = PendingNamedVectors;
    Record.Extensions.NamedRotators = PendingNamedRotators;

    return Record;
}

bool UGASPALSShadowObserverComponent::RecordFrame(const FGASPALSShadowFrameRecord& Record)
{
    if (UWorld* World = GetWorld())
    {
        if (UGASPALSShadowWorldSubsystem* Subsystem = World->GetSubsystem<UGASPALSShadowWorldSubsystem>())
        {
            const bool bRecorded = Subsystem->AppendFrameRecord(Record);
            if (bRecorded)
            {
                ++CaptureCounter;
            }
            return bRecorded;
        }
    }

    return false;
}

void UGASPALSShadowObserverComponent::SetNamedFloat(FName Name, float Value)
{
    PendingNamedFloats.Add(Name.ToString(), Value);
}

void UGASPALSShadowObserverComponent::SetNamedString(FName Name, const FString& Value)
{
    PendingNamedStrings.Add(Name.ToString(), Value);
}

void UGASPALSShadowObserverComponent::SetNamedBool(FName Name, bool Value)
{
    PendingNamedBools.Add(Name.ToString(), Value);
}

void UGASPALSShadowObserverComponent::SetNamedInteger(FName Name, int32 Value)
{
    PendingNamedIntegers.Add(Name.ToString(), Value);
}

void UGASPALSShadowObserverComponent::SetNamedVector(FName Name, FVector Value)
{
    PendingNamedVectors.Add(Name.ToString(), Value);
}

void UGASPALSShadowObserverComponent::SetNamedRotator(FName Name, FRotator Value)
{
    PendingNamedRotators.Add(Name.ToString(), Value);
}

void UGASPALSShadowObserverComponent::DeclareChannelSource(FName ChannelName, const FString& Source)
{
    const FString Channel = ChannelName.ToString();
    PendingChannelSources.Add(Channel, Source);
    PendingActiveChannels.Add(Channel);

    if (UWorld* World = GetWorld())
    {
        if (UGASPALSShadowWorldSubsystem* Subsystem = World->GetSubsystem<UGASPALSShadowWorldSubsystem>())
        {
            FGASPALSShadowSessionEvent Event;
            Event.EventName = TEXT("channel_declared");
            Event.Source = TEXT("observer_component");
            Event.ActorName = GetOwner() ? GetOwner()->GetName() : TEXT("");
            Event.Message = FString::Printf(TEXT("Declared channel '%s' from '%s'."), *Channel, *Source);
            Event.Metadata.Add(TEXT("channel"), Channel);
            Event.Metadata.Add(TEXT("source"), Source);
            Subsystem->AppendEvent(Event);
        }
    }
}

void UGASPALSShadowObserverComponent::SetChannelActive(FName ChannelName, bool bActive)
{
    const FString Channel = ChannelName.ToString();
    if (bActive)
    {
        PendingActiveChannels.Add(Channel);
    }
    else
    {
        PendingActiveChannels.Remove(Channel);
    }
}

void UGASPALSShadowObserverComponent::SetEncoderProfile(const FString& EncoderProfile)
{
    PendingEncoderProfile = EncoderProfile;
}

void UGASPALSShadowObserverComponent::SetControlContractId(const FString& ControlContractId)
{
    PendingControlContractId = ControlContractId;
}

bool UGASPALSShadowObserverComponent::EmitEvent(FName EventName, const FString& Message, EGASPALSShadowEventSeverity Severity)
{
    if (UWorld* World = GetWorld())
    {
        if (UGASPALSShadowWorldSubsystem* Subsystem = World->GetSubsystem<UGASPALSShadowWorldSubsystem>())
        {
            FGASPALSShadowSessionEvent Event;
            Event.EventName = EventName.ToString();
            Event.Source = TEXT("observer_component");
            Event.ActorName = GetOwner() ? GetOwner()->GetName() : TEXT("");
            Event.Message = Message;
            Event.Severity = Severity;
            return Subsystem->AppendEvent(Event);
        }
    }

    return false;
}

void UGASPALSShadowObserverComponent::ClearNamedMetadata()
{
    PendingNamedFloats.Reset();
    PendingNamedStrings.Reset();
    PendingNamedBools.Reset();
    PendingNamedIntegers.Reset();
    PendingNamedVectors.Reset();
    PendingNamedRotators.Reset();
}

void UGASPALSShadowObserverComponent::RecordLifecycleEvent(
    FName EventName,
    const FString& Message,
    EGASPALSShadowEventSeverity Severity) const
{
    if (UWorld* World = GetWorld())
    {
        if (UGASPALSShadowWorldSubsystem* Subsystem = World->GetSubsystem<UGASPALSShadowWorldSubsystem>())
        {
            FGASPALSShadowSessionEvent Event;
            Event.EventName = EventName.ToString();
            Event.Source = TEXT("observer_component");
            Event.ActorName = GetOwner() ? GetOwner()->GetName() : TEXT("");
            Event.Message = Message;
            Event.Severity = Severity;
            Subsystem->AppendEvent(Event);
        }
    }
}

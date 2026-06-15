#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GASPALSShadowTypes.h"
#include "GASPALSShadowObserverComponent.generated.h"

UCLASS(ClassGroup = (GASPALSShadow), BlueprintType, Blueprintable, meta = (BlueprintSpawnableComponent))
class GASPALSSHADOW_API UGASPALSShadowObserverComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UGASPALSShadowObserverComponent();

    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GASPALSShadow")
    bool bAutoCaptureOwnerState = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GASPALSShadow", meta = (ClampMin = "0.0"))
    float CaptureIntervalSeconds = 0.0f;

    UFUNCTION(BlueprintCallable, Category = "GASPALSShadow")
    FGASPALSShadowFrameRecord BuildOwnerSnapshot() const;

    UFUNCTION(BlueprintCallable, Category = "GASPALSShadow")
    bool RecordFrame(const FGASPALSShadowFrameRecord& Record);

    UFUNCTION(BlueprintCallable, Category = "GASPALSShadow")
    void SetNamedFloat(FName Name, float Value);

    UFUNCTION(BlueprintCallable, Category = "GASPALSShadow")
    void SetNamedString(FName Name, const FString& Value);

    UFUNCTION(BlueprintCallable, Category = "GASPALSShadow")
    void SetNamedBool(FName Name, bool Value);

    UFUNCTION(BlueprintCallable, Category = "GASPALSShadow")
    void SetNamedInteger(FName Name, int32 Value);

    UFUNCTION(BlueprintCallable, Category = "GASPALSShadow")
    void SetNamedVector(FName Name, FVector Value);

    UFUNCTION(BlueprintCallable, Category = "GASPALSShadow")
    void SetNamedRotator(FName Name, FRotator Value);

    UFUNCTION(BlueprintCallable, Category = "GASPALSShadow")
    void DeclareChannelSource(FName ChannelName, const FString& Source);

    UFUNCTION(BlueprintCallable, Category = "GASPALSShadow")
    void SetChannelActive(FName ChannelName, bool bActive);

    UFUNCTION(BlueprintCallable, Category = "GASPALSShadow")
    void SetEncoderProfile(const FString& EncoderProfile);

    UFUNCTION(BlueprintCallable, Category = "GASPALSShadow")
    void SetControlContractId(const FString& ControlContractId);

    UFUNCTION(BlueprintCallable, Category = "GASPALSShadow")
    bool EmitEvent(FName EventName, const FString& Message, EGASPALSShadowEventSeverity Severity = EGASPALSShadowEventSeverity::Info);

    UFUNCTION(BlueprintCallable, Category = "GASPALSShadow")
    void ClearNamedMetadata();

private:
    void RecordLifecycleEvent(FName EventName, const FString& Message, EGASPALSShadowEventSeverity Severity) const;

    int64 CaptureCounter = 0;
    float TimeSinceLastCapture = 0.0f;

    UPROPERTY(Transient)
    TMap<FString, float> PendingNamedFloats;

    UPROPERTY(Transient)
    TMap<FString, FString> PendingNamedStrings;

    UPROPERTY(Transient)
    TMap<FString, bool> PendingNamedBools;

    UPROPERTY(Transient)
    TMap<FString, int32> PendingNamedIntegers;

    UPROPERTY(Transient)
    TMap<FString, FVector> PendingNamedVectors;

    UPROPERTY(Transient)
    TMap<FString, FRotator> PendingNamedRotators;

    UPROPERTY(Transient)
    TMap<FString, FString> PendingChannelSources;

    UPROPERTY(Transient)
    TSet<FString> PendingActiveChannels;

    UPROPERTY(Transient)
    FString PendingEncoderProfile;

    UPROPERTY(Transient)
    FString PendingControlContractId = TEXT("control_to_lmm/v1");
};

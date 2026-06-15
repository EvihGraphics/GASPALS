#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "GASPALSShadowTypes.h"
#include "GASPALSShadowWorldSubsystem.generated.h"

UCLASS()
class GASPALSSHADOW_API UGASPALSShadowWorldSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    UFUNCTION(BlueprintCallable, Category = "GASPALSShadow")
    FString GetActiveSessionDirectory() const;

    UFUNCTION(BlueprintCallable, Category = "GASPALSShadow")
    FString GetActiveSessionId() const;

    UFUNCTION(BlueprintCallable, Category = "GASPALSShadow")
    bool AppendFrameRecord(const FGASPALSShadowFrameRecord& Record);

    UFUNCTION(BlueprintCallable, Category = "GASPALSShadow")
    bool AppendEvent(const FGASPALSShadowSessionEvent& Event);

    UFUNCTION(BlueprintCallable, Category = "GASPALSShadow")
    bool AppendMimicKitShadowEvent(const FString& EventName, const TMap<FString, FString>& Metadata);

private:
    void EnsureSessionDirectory();
    void WriteSessionManifest();
    void MarkSessionClosed();
    void UpdateStatsFromFrame(const FGASPALSShadowFrameRecord& Record);
    void UpdateStatsFromEvent(const FGASPALSShadowSessionEvent& Event);
    bool AppendJsonLineToFile(const FString& FilePath, const FString& JsonLine);

    FString SessionDirectory;
    FString ManifestPath;
    FString FramesFilePath;
    FString EventsFilePath;
    FString MimicKitShadowFilePath;
    FGASPALSShadowSessionManifest SessionManifest;
    FCriticalSection WriteLock;
    int64 NextEventIndex = 0;
    int64 NextMimicKitEventIndex = 0;
    bool bSessionInitialized = false;
};

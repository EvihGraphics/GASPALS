#include "GASPALSShadowWorldSubsystem.h"

#include "HAL/FileManager.h"
#include "JsonObjectConverter.h"
#include "Misc/App.h"
#include "Misc/EngineVersion.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeLock.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace
{
FString NowUtcIso8601()
{
    return FDateTime::UtcNow().ToIso8601();
}

void AddUniqueValue(TArray<FString>& Values, const FString& Value)
{
    if (!Value.IsEmpty())
    {
        Values.AddUnique(Value);
    }
}

template <typename TValue>
void AddMapKeys(TArray<FString>& Keys, const TMap<FString, TValue>& Map)
{
    for (const TPair<FString, TValue>& Pair : Map)
    {
        AddUniqueValue(Keys, Pair.Key);
    }
}

void MergeStringMap(TMap<FString, FString>& Target, const TMap<FString, FString>& Source)
{
    for (const TPair<FString, FString>& Pair : Source)
    {
        if (!Pair.Key.IsEmpty() && !Pair.Value.IsEmpty())
        {
            Target.Add(Pair.Key, Pair.Value);
        }
    }
}
}

void UGASPALSShadowWorldSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    EnsureSessionDirectory();
    WriteSessionManifest();

    FGASPALSShadowSessionEvent StartedEvent;
    StartedEvent.EventName = TEXT("session_started");
    StartedEvent.Source = TEXT("world_subsystem");
    StartedEvent.Message = TEXT("Shadow session initialized.");
    AppendEvent(StartedEvent);
}

void UGASPALSShadowWorldSubsystem::Deinitialize()
{
    if (bSessionInitialized)
    {
        FGASPALSShadowSessionEvent ClosedEvent;
        ClosedEvent.EventName = TEXT("session_closing");
        ClosedEvent.Source = TEXT("world_subsystem");
        ClosedEvent.Message = TEXT("Shadow session is closing.");
        AppendEvent(ClosedEvent);
        MarkSessionClosed();
    }

    Super::Deinitialize();
}

FString UGASPALSShadowWorldSubsystem::GetActiveSessionDirectory() const
{
    return SessionDirectory;
}

FString UGASPALSShadowWorldSubsystem::GetActiveSessionId() const
{
    return SessionManifest.SessionId;
}

bool UGASPALSShadowWorldSubsystem::AppendFrameRecord(const FGASPALSShadowFrameRecord& InRecord)
{
    EnsureSessionDirectory();

    FGASPALSShadowFrameRecord Record = InRecord;
    if (Record.SchemaVersion.IsEmpty())
    {
        Record.SchemaVersion = TEXT("gaspals_shadow/v2");
    }
    if (Record.SessionId.IsEmpty())
    {
        Record.SessionId = SessionManifest.SessionId;
    }
    if (Record.Subject.ActorName.IsEmpty())
    {
        Record.Subject.ActorName = Record.ActorName;
    }

    FString JsonLine;
    if (!FJsonObjectConverter::UStructToJsonObjectString(Record, JsonLine))
    {
        return false;
    }
    JsonLine.AppendChar(TEXT('\n'));

    FScopeLock Guard(&WriteLock);
    if (!AppendJsonLineToFile(FramesFilePath, JsonLine))
    {
        return false;
    }

    UpdateStatsFromFrame(Record);
    WriteSessionManifest();
    return true;
}

bool UGASPALSShadowWorldSubsystem::AppendEvent(const FGASPALSShadowSessionEvent& InEvent)
{
    EnsureSessionDirectory();

    FGASPALSShadowSessionEvent Event = InEvent;
    if (Event.SchemaVersion.IsEmpty())
    {
        Event.SchemaVersion = TEXT("gaspals_shadow/event/v1");
    }
    if (Event.SessionId.IsEmpty())
    {
        Event.SessionId = SessionManifest.SessionId;
    }
    if (Event.EventIndex <= 0)
    {
        Event.EventIndex = NextEventIndex;
    }
    NextEventIndex = FMath::Max<int64>(NextEventIndex, Event.EventIndex + 1);
    if (Event.WorldTimeSeconds <= 0.0 && GetWorld())
    {
        Event.WorldTimeSeconds = GetWorld()->GetTimeSeconds();
    }

    FString JsonLine;
    if (!FJsonObjectConverter::UStructToJsonObjectString(Event, JsonLine))
    {
        return false;
    }
    JsonLine.AppendChar(TEXT('\n'));

    FScopeLock Guard(&WriteLock);
    if (!AppendJsonLineToFile(EventsFilePath, JsonLine))
    {
        return false;
    }

    UpdateStatsFromEvent(Event);
    WriteSessionManifest();
    return true;
}

bool UGASPALSShadowWorldSubsystem::AppendMimicKitShadowEvent(const FString& EventName, const TMap<FString, FString>& Metadata)
{
    EnsureSessionDirectory();

    TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
    Root->SetStringField(TEXT("schema_version"), TEXT("gaspals_shadow/mimickit/v1"));
    Root->SetStringField(TEXT("session_id"), SessionManifest.SessionId);
    Root->SetNumberField(TEXT("event_index"), NextMimicKitEventIndex++);
    Root->SetStringField(TEXT("event_name"), EventName);
    Root->SetStringField(TEXT("created_at_utc"), NowUtcIso8601());
    if (GetWorld())
    {
        Root->SetNumberField(TEXT("world_time_seconds"), GetWorld()->GetTimeSeconds());
    }

    TSharedRef<FJsonObject> MetadataObject = MakeShared<FJsonObject>();
    for (const TPair<FString, FString>& Pair : Metadata)
    {
        if (!Pair.Key.IsEmpty())
        {
            MetadataObject->SetStringField(Pair.Key, Pair.Value);
        }
    }
    Root->SetObjectField(TEXT("metadata"), MetadataObject);

    FString JsonLine;
    const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonLine);
    if (!FJsonSerializer::Serialize(Root, Writer))
    {
        return false;
    }
    JsonLine.AppendChar(TEXT('\n'));

    FScopeLock Guard(&WriteLock);
    return AppendJsonLineToFile(MimicKitShadowFilePath, JsonLine);
}

void UGASPALSShadowWorldSubsystem::EnsureSessionDirectory()
{
    if (bSessionInitialized)
    {
        return;
    }

    const FString Timestamp = FDateTime::UtcNow().ToString(TEXT("%Y%m%d_%H%M%S"));
    SessionDirectory = FPaths::Combine(FPaths::ProjectLogDir(), TEXT("GASPALSShadow"), Timestamp);
    ManifestPath = FPaths::Combine(SessionDirectory, TEXT("session.json"));
    FramesFilePath = FPaths::Combine(SessionDirectory, TEXT("frames.jsonl"));
    EventsFilePath = FPaths::Combine(SessionDirectory, TEXT("events.jsonl"));
    MimicKitShadowFilePath = FPaths::Combine(SessionDirectory, TEXT("mimickit_shadow.jsonl"));

    IFileManager::Get().MakeDirectory(*SessionDirectory, true);
    FFileHelper::SaveStringToFile(TEXT(""), *FramesFilePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    FFileHelper::SaveStringToFile(TEXT(""), *EventsFilePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    FFileHelper::SaveStringToFile(TEXT(""), *MimicKitShadowFilePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);

    SessionManifest = FGASPALSShadowSessionManifest();
    SessionManifest.SessionId = Timestamp;
    SessionManifest.CreatedAtUtc = NowUtcIso8601();
    SessionManifest.LastUpdatedAtUtc = SessionManifest.CreatedAtUtc;
    SessionManifest.ProjectName = FApp::GetProjectName();
    SessionManifest.WorldName = GetWorld() ? GetWorld()->GetName() : TEXT("UnknownWorld");
    SessionManifest.MapName = GetWorld() ? GetWorld()->GetMapName() : TEXT("UnknownMap");
    SessionManifest.EngineVersion = FEngineVersion::Current().ToString();
    SessionManifest.PluginVersion = TEXT("0.1.0");

    NextEventIndex = 0;
    NextMimicKitEventIndex = 0;
    bSessionInitialized = true;
}

void UGASPALSShadowWorldSubsystem::WriteSessionManifest()
{
    EnsureSessionDirectory();

    if (SessionManifest.Status != TEXT("closed"))
    {
        SessionManifest.LastUpdatedAtUtc = NowUtcIso8601();
    }

    FString ManifestJson;
    if (!FJsonObjectConverter::UStructToJsonObjectString(SessionManifest, ManifestJson))
    {
        return;
    }

    FFileHelper::SaveStringToFile(
        ManifestJson,
        *ManifestPath,
        FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}

void UGASPALSShadowWorldSubsystem::MarkSessionClosed()
{
    SessionManifest.Status = TEXT("closed");
    SessionManifest.ClosedAtUtc = NowUtcIso8601();
    SessionManifest.LastUpdatedAtUtc = SessionManifest.ClosedAtUtc;
    WriteSessionManifest();
}

void UGASPALSShadowWorldSubsystem::UpdateStatsFromFrame(const FGASPALSShadowFrameRecord& Record)
{
    FGASPALSShadowSessionStats& Stats = SessionManifest.Stats;
    if (Stats.TotalFrames == 0)
    {
        Stats.FirstWorldTimeSeconds = Record.WorldTimeSeconds;
    }

    Stats.TotalFrames += 1;
    Stats.LastWorldTimeSeconds = Record.WorldTimeSeconds;

    AddUniqueValue(Stats.ObservedActors, Record.ActorName);
    AddUniqueValue(Stats.ObservedActors, Record.Subject.ActorName);
    AddUniqueValue(Stats.ObservedControllers, Record.Subject.ControllerName);

    for (const FString& ChannelName : Record.Extensions.ActiveChannels)
    {
        AddUniqueValue(Stats.ActiveChannels, ChannelName);
    }

    MergeStringMap(Stats.DeclaredChannelSources, Record.Extensions.ChannelSources);
    AddMapKeys(Stats.ExtensionBoolKeys, Record.Extensions.NamedBools);
    AddMapKeys(Stats.ExtensionIntegerKeys, Record.Extensions.NamedIntegers);
    AddMapKeys(Stats.ExtensionFloatKeys, Record.Extensions.NamedFloats);
    AddMapKeys(Stats.ExtensionStringKeys, Record.Extensions.NamedStrings);
    AddMapKeys(Stats.ExtensionVectorKeys, Record.Extensions.NamedVectors);
    AddMapKeys(Stats.ExtensionRotatorKeys, Record.Extensions.NamedRotators);
}

void UGASPALSShadowWorldSubsystem::UpdateStatsFromEvent(const FGASPALSShadowSessionEvent& Event)
{
    FGASPALSShadowSessionStats& Stats = SessionManifest.Stats;
    Stats.TotalEvents += 1;
    AddUniqueValue(Stats.ObservedActors, Event.ActorName);
}

bool UGASPALSShadowWorldSubsystem::AppendJsonLineToFile(const FString& FilePath, const FString& JsonLine)
{
    return FFileHelper::SaveStringToFile(
        JsonLine,
        *FilePath,
        FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM,
        &IFileManager::Get(),
        FILEWRITE_Append);
}

// Skylanders Conquest - Telemetry / event logging implementation

#include "SkylandersTelemetry.h"
#include "SkylandersEnemyGod.h"
#include "SkylandersCharacter.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/DateTime.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformFileManager.h"

static FString StateToStr(EGodAIState S)
{
	switch (S)
	{
	case EGodAIState::Laning:    return TEXT("laning");
	case EGodAIState::Fighting:  return TEXT("fighting");
	case EGodAIState::Retreating:return TEXT("retreating");
	case EGodAIState::Dead:      return TEXT("dead");
	default:                     return TEXT("?");
	}
}

static FString TeamToStr(ETowerTeam T)
{
	return T == ETowerTeam::Enemy ? TEXT("red") : TEXT("blue");
}

void USkylandersTelemetrySubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// Periodic god snapshot every 2.5s (movement / stuck / economy time-series).
	SnapshotHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateUObject(this, &USkylandersTelemetrySubsystem::Snapshot), 2.5f);
}

void USkylandersTelemetrySubsystem::Deinitialize()
{
	if (SnapshotHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(SnapshotHandle);
	}
	Super::Deinitialize();
}

USkylandersTelemetrySubsystem* USkylandersTelemetrySubsystem::Get(const UObject* WorldContext)
{
	if (!WorldContext) return nullptr;
	if (UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr)
	{
		if (UGameInstance* GI = World->GetGameInstance())
		{
			return GI->GetSubsystem<USkylandersTelemetrySubsystem>();
		}
	}
	return nullptr;
}

void USkylandersTelemetrySubsystem::OpenLog()
{
	if (bReady) return;

	const FString Dir = FPaths::ProjectSavedDir() / TEXT("Telemetry");
	IFileManager::Get().MakeDirectory(*Dir, true);

	// FDateTime::Now() is fine in engine code (unlike workflow scripts).
	const FString Stamp = FDateTime::Now().ToString(TEXT("%Y.%m.%d-%H.%M.%S"));
	LogFilePath = Dir / FString::Printf(TEXT("session_%s.jsonl"), *Stamp);
	bReady = true;

	UE_LOG(LogTemp, Log, TEXT("[Telemetry] logging to %s"), *LogFilePath);
}

void USkylandersTelemetrySubsystem::WriteLine(const FString& Line)
{
	if (!bReady) OpenLog();
	FFileHelper::SaveStringToFile(Line + LINE_TERMINATOR, *LogFilePath,
		FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM,
		&IFileManager::Get(), EFileWrite::FILEWRITE_Append);
}

float USkylandersTelemetrySubsystem::MatchTime() const
{
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UWorld* World = GI->GetWorld())
		{
			return World->GetTimeSeconds();
		}
	}
	return 0.0f;
}

void USkylandersTelemetrySubsystem::LogEvent(const FString& EventType, const FString& Fields)
{
	FString Line = FString::Printf(TEXT("{\"t\":%.2f,\"type\":\"%s\""), MatchTime(), *EventType);
	if (!Fields.IsEmpty())
	{
		Line += TEXT(",") + Fields;
	}
	Line += TEXT("}");
	WriteLine(Line);
}

void USkylandersTelemetrySubsystem::LogMatchStart(const FString& MapName)
{
	LogEvent(TEXT("match_start"), FString::Printf(TEXT("\"map\":\"%s\""), *MapName));
	LastGodPos.Reset();
}

void USkylandersTelemetrySubsystem::LogMatchEnd(bool bVictory, float MatchSeconds)
{
	LogEvent(TEXT("match_end"), FString::Printf(TEXT("\"victory\":%s,\"seconds\":%.1f"),
		bVictory ? TEXT("true") : TEXT("false"), MatchSeconds));
}

void USkylandersTelemetrySubsystem::LogGodDeath(ASkylandersEnemyGod* God, const FString& Cause)
{
	if (!God) return;
	const FVector L = God->GetActorLocation();
	LogEvent(TEXT("god_death"), FString::Printf(
		TEXT("\"name\":\"%s\",\"team\":\"%s\",\"level\":%d,\"cause\":\"%s\",\"x\":%.0f,\"y\":%.0f"),
		*God->GodName, *TeamToStr(God->Team), God->Level, *Cause, L.X, L.Y));
}

void USkylandersTelemetrySubsystem::LogPlayerDeath(ASkylandersCharacter* Player, const FString& Cause)
{
	FVector L = Player ? Player->GetActorLocation() : FVector::ZeroVector;
	int32 Lvl = Player ? Player->PlayerLevel : 0;
	LogEvent(TEXT("player_death"), FString::Printf(
		TEXT("\"level\":%d,\"cause\":\"%s\",\"x\":%.0f,\"y\":%.0f"), Lvl, *Cause, L.X, L.Y));
}

void USkylandersTelemetrySubsystem::LogStructureDestroyed(const FString& Name, const FString& Team)
{
	LogEvent(TEXT("structure_destroyed"), FString::Printf(
		TEXT("\"name\":\"%s\",\"team\":\"%s\""), *Name, *Team));
}

void USkylandersTelemetrySubsystem::LogAbility(const FString& InstigatorName, const FString& Team, const FString& AbilityName, const FVector& Location)
{
	LogEvent(TEXT("ability"), FString::Printf(
		TEXT("\"by\":\"%s\",\"team\":\"%s\",\"ability\":\"%s\",\"x\":%.0f,\"y\":%.0f"),
		*InstigatorName, *Team, *AbilityName, Location.X, Location.Y));
}

bool USkylandersTelemetrySubsystem::Snapshot(float /*DeltaTime*/)
{
	UGameInstance* GI = GetGameInstance();
	UWorld* World = GI ? GI->GetWorld() : nullptr;
	// Only snapshot during a live game world (skip menu / no PIE).
	if (!World || World->GetTimeSeconds() < 0.5f) return true;

	TArray<AActor*> Gods;
	UGameplayStatics::GetAllActorsOfClass(World, ASkylandersEnemyGod::StaticClass(), Gods);
	if (Gods.Num() == 0) return true; // not in a match

	for (AActor* A : Gods)
	{
		ASkylandersEnemyGod* God = Cast<ASkylandersEnemyGod>(A);
		if (!God) continue;

		const FVector Pos = God->GetActorLocation();
		const FString Key = God->GodName;
		float Moved = 0.0f;
		if (FVector* Prev = LastGodPos.Find(Key))
		{
			Moved = FVector::Dist2D(Pos, *Prev);
		}
		LastGodPos.Add(Key, Pos);

		// Stuck = actively laning/fighting but barely moved this interval.
		const bool bActive = God->CurrentState == EGodAIState::Laning || God->CurrentState == EGodAIState::Fighting;
		const bool bStuck = bActive && Moved < 40.0f;

		LogEvent(TEXT("snapshot"), FString::Printf(
			TEXT("\"name\":\"%s\",\"team\":\"%s\",\"state\":\"%s\",\"x\":%.0f,\"y\":%.0f,\"hp\":%.0f,\"gold\":%d,\"level\":%d,\"moved\":%.0f,\"stuck\":%s"),
			*God->GodName, *TeamToStr(God->Team), *StateToStr(God->CurrentState),
			Pos.X, Pos.Y, God->CurrentHealth, God->Gold, God->Level, Moved,
			bStuck ? TEXT("true") : TEXT("false")));
	}

	return true; // keep ticking
}

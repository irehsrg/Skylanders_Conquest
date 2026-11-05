// Skylanders Conquest - Headless smoke test implementation

#include "SkylandersSmokeTest.h"
#include "SkylandersActorRegistry.h"
#include "SkylandersBuffCamp.h"
#include "SkylandersCharacter.h"
#include "SkylandersEnemyGod.h"
#include "SkylandersMinion.h"
#include "SkylandersTitan.h"
#include "SkylandersTower.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/OutputDeviceRedirector.h"
#include "Misc/Paths.h"
#include "HAL/PlatformMisc.h"

// Minimal JSON string escaping (FString::ReplaceCharWithEscapedChar escapes
// single quotes too, which is not valid JSON).
static FString JsonEscape(const FString& In)
{
	// Written with character codes so the escapes survive any tooling that
	// mangles backslashes: 0x5C is the backslash, 0x22 the double quote.
	const TCHAR BS = (TCHAR)0x5C;
	FString Out;
	Out.Reserve(In.Len() + 8);
	for (TCHAR Ch : In)
	{
		switch (Ch)
		{
		case (TCHAR)0x22: Out.AppendChar(BS); Out.AppendChar(Ch); break;
		case (TCHAR)0x5C: Out.AppendChar(BS); Out.AppendChar(Ch); break;
		case (TCHAR)0x0A: Out.AppendChar(BS); Out.AppendChar(TEXT('n')); break;
		case (TCHAR)0x0D: Out.AppendChar(BS); Out.AppendChar(TEXT('r')); break;
		case (TCHAR)0x09: Out.AppendChar(BS); Out.AppendChar(TEXT('t')); break;
		default:
			if (Ch < 0x20) { Out.AppendChar(BS); Out += FString::Printf(TEXT("u%04x"), (uint32)Ch); }
			else Out.AppendChar(Ch);
		}
	}
	return Out;
}

bool USkylandersSmokeTestSubsystem::bActive = false;
TMap<FName, int32> USkylandersSmokeTestSubsystem::Counters;

void USkylandersSmokeTestSubsystem::Count(const TCHAR* Name, int32 Delta)
{
	if (!bActive) return;
	Counters.FindOrAdd(FName(Name)) += Delta;
}

void USkylandersSmokeTestSubsystem::FErrorSink::Serialize(const TCHAR* V, ELogVerbosity::Type Verbosity, const FName& Category)
{
	if (Verbosity != ELogVerbosity::Error && Verbosity != ELogVerbosity::Fatal) return;
	// Engine-side noise that is not ours: the HTTP server probe and the unified
	// error self-test both log Errors on every headless start. LogPython: the
	// engine's experimental NiagaraToolsets plugin runs an init_unreal.py at
	// startup that references editor-only toolset classes, which do not exist
	// under -game, so it logs a 9-line traceback every run. The game itself has
	// no runtime Python (Scripts/*.py are editor/offline tools), so nothing of
	// ours can log under this category during a match.
	static const FName Ignored[] = { FName("LogHttpConnection"), FName("LogHttpListener"), FName("LogPython") };
	for (const FName& I : Ignored) if (Category == I) return;
	FString Msg(V);
	if (Msg.Contains(TEXT("UnifiedErrorTest"))) return;
	ErrorCount++;
	if (FirstErrors.Num() < 10) FirstErrors.Add(FString::Printf(TEXT("[%s] %s"), *Category.ToString(), *Msg));
}

void USkylandersSmokeTestSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	float Seconds = 0.0f;
	if (!FParse::Value(FCommandLine::Get(), TEXT("SkySmoke="), Seconds) || Seconds <= 0.0f)
	{
		return;
	}
	DurationSeconds = Seconds;
	bActive = true;
	Counters.Reset();
	GLog->AddOutputDevice(&ErrorSink);
	TickHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &USkylandersSmokeTestSubsystem::Tick), 0.0f);
	UE_LOG(LogTemp, Warning, TEXT("[Smoke] Smoke test armed for %.0f s"), DurationSeconds);
}

void USkylandersSmokeTestSubsystem::Deinitialize()
{
	if (bActive)
	{
		FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
		GLog->RemoveOutputDevice(&ErrorSink);
		bActive = false;
	}
	Super::Deinitialize();
}

bool USkylandersSmokeTestSubsystem::Tick(float DeltaTime)
{
	if (bFinished) return false;
	UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
	if (!World || !World->IsGameWorld() || !World->HasBegunPlay()) return true;

	Elapsed += DeltaTime;
	SinceSample += DeltaTime;

	// Nobody is at the keyboard: a scripted bot plays the player so combat,
	// abilities and leveling actually get exercised (see SkylandersCharacter_Bot.cpp).
	if (ASkylandersCharacter* P = Cast<ASkylandersCharacter>(UGameplayStatics::GetPlayerPawn(World, 0)))
	{
		P->DriveAsBot(DeltaTime);
	}
	if (SinceSample >= 1.0f)
	{
		SinceSample = 0.0f;
		Sample(World);
	}
	if (Elapsed >= DurationSeconds)
	{
		Finish();
		return false;
	}
	return true;
}

void USkylandersSmokeTestSubsystem::Sample(UWorld* World)
{
	FSample S;
	S.T = Elapsed;
	TArray<AActor*> Found;

	USkylandersActorRegistry::GetAllActorsOfClass(World, ASkylandersMinion::StaticClass(), Found);
	for (AActor* A : Found)
	{
		if (ASkylandersMinion* M = Cast<ASkylandersMinion>(A))
		{
			if (!M->bDead) S.Minions++;
		}
	}

	USkylandersActorRegistry::GetAllActorsOfClass(World, ASkylandersEnemyGod::StaticClass(), Found);
	S.Gods = Found.Num();
	for (AActor* A : Found)
	{
		if (ASkylandersEnemyGod* G = Cast<ASkylandersEnemyGod>(A))
		{
			if (G->CurrentState != EGodAIState::Dead) S.GodsAlive++;
			S.GodHealthSum += G->CurrentHealth;
			S.GodHealthMaxSum += G->MaxHealth + G->ItemBonusStats.MaxHealth;
		}
	}

	USkylandersActorRegistry::GetAllActorsOfClass(World, ASkylandersTower::StaticClass(), Found);
	S.Towers = Found.Num();
	for (AActor* A : Found)
	{
		if (ASkylandersTower* T = Cast<ASkylandersTower>(A))
		{
			if (!T->bDestroyed) S.TowersAlive++;
		}
	}

	USkylandersActorRegistry::GetAllActorsOfClass(World, ASkylandersTitan::StaticClass(), Found);
	S.Titans = Found.Num();
	USkylandersActorRegistry::GetAllActorsOfClass(World, ASkylandersBuffCamp::StaticClass(), Found);
	S.Camps = Found.Num();

	if (ASkylandersCharacter* P = Cast<ASkylandersCharacter>(UGameplayStatics::GetPlayerPawn(World, 0)))
	{
		S.PlayerHealth = P->CurrentHealth;
		S.PlayerLevel = P->PlayerLevel;
		S.PlayerCoins = P->Coins;
		S.PlayerKills = P->Kills;
		S.PlayerDeaths = P->Deaths;
	}

	MaxMinionsSeen = FMath::Max(MaxMinionsSeen, S.Minions);
	Samples.Add(S);
}

FString USkylandersSmokeTestSubsystem::FSample::ToJson() const
{
	return FString::Printf(TEXT("{\"t\":%.0f,\"minions\":%d,\"gods\":%d,\"gods_alive\":%d,\"god_hp\":%.0f,\"god_hp_max\":%.0f,\"towers\":%d,\"towers_alive\":%d,\"titans\":%d,\"camps\":%d,\"player_hp\":%.0f,\"player_level\":%d,\"player_coins\":%d,\"player_kills\":%d,\"player_deaths\":%d}"),
		T, Minions, Gods, GodsAlive, GodHealthSum, GodHealthMaxSum, Towers, TowersAlive, Titans, Camps, PlayerHealth, PlayerLevel, PlayerCoins, PlayerKills, PlayerDeaths);
}

TArray<FString> USkylandersSmokeTestSubsystem::EvaluateInvariants() const
{
	TArray<FString> Failures;
	auto C = [](const TCHAR* N) { const int32* V = Counters.Find(FName(N)); return V ? *V : 0; };
	auto Require = [&](bool bOk, const FString& What) { if (!bOk) Failures.Add(What); };

	Require(Samples.Num() > 0, TEXT("no samples taken (world never began play?)"));
	if (Samples.Num() == 0) return Failures;
	const FSample& Last = Samples.Last();

	Require(Last.Gods >= 5, FString::Printf(TEXT("expected 5 gods, saw %d"), Last.Gods));
	Require(Last.Towers == 4, FString::Printf(TEXT("expected 4 towers, saw %d"), Last.Towers));
	Require(Last.Titans == 2, FString::Printf(TEXT("expected 2 titans, saw %d"), Last.Titans));
	Require(Last.Camps >= 4, FString::Printf(TEXT("expected >=4 buff camps, saw %d"), Last.Camps));
	Require(MaxMinionsSeen >= 12, FString::Printf(TEXT("expected a full wave (>=12 minions) at some point, max seen %d"), MaxMinionsSeen));
	Require(C(TEXT("minion_spawn")) >= 12, FString::Printf(TEXT("minion_spawn=%d"), C(TEXT("minion_spawn"))));
	if (DurationSeconds >= 90.0f)
	{
		// Waves meet around 60 s; by 90 s there must have been combat.
		Require(C(TEXT("minion_death")) >= 1, TEXT("no minion died: waves never fought"));
		Require(C(TEXT("god_damage")) >= 1, TEXT("no god took damage: heroes never fought"));
		Require(C(TEXT("hit_flash")) >= 1, TEXT("no hit flash fired"));
		// The player bot must have actually played: shot, cast, been hit, and
		// the feedback layers (shake, sound) must have fired.
		Require(C(TEXT("player_shot")) >= 1, TEXT("player bot never fired a shot"));
		Require(C(TEXT("player_ability")) >= 1, TEXT("player bot never cast an ability"));
		Require(C(TEXT("player_damage")) >= 1, TEXT("player never took damage"));
		Require(C(TEXT("camera_shake")) >= 1, TEXT("no camera shake fired"));
		Require(C(TEXT("sound_play")) >= 1, TEXT("no sound played"));
		// Everyone spawns with starting gold and shops at the fountain, so by
		// 90 s the player bot and at least one god have each bought a starter.
		Require(C(TEXT("god_item_bought")) >= 1, TEXT("no AI god bought an item"));
		Require(C(TEXT("item_bought")) >= 2, FString::Printf(TEXT("item_bought=%d, expected player + god starters"), C(TEXT("item_bought"))));
	}
	// AI state machine sanity: gods must change state at all (they lane, fight,
	// die), but a 120 s match with hundreds of flips means the tower back-off
	// hysteresis has regressed into per-tick thrash.
	Require(C(TEXT("god_state_change")) > 0, TEXT("no god ever changed AI state"));
	if (DurationSeconds <= 120.0f)
	{
		Require(C(TEXT("god_state_change")) <= 200,
			FString::Printf(TEXT("god_state_change=%d > 200: AI state thrash"), C(TEXT("god_state_change"))));
	}
	Require(Last.PlayerLevel >= 3, FString::Printf(TEXT("player level %d < 3"), Last.PlayerLevel));
	Require(Last.PlayerCoins > 0, TEXT("player never earned gold"));
	Require(ErrorSink.ErrorCount == 0, FString::Printf(TEXT("%d error(s) logged"), ErrorSink.ErrorCount));
	return Failures;
}

void USkylandersSmokeTestSubsystem::Finish()
{
	bFinished = true;
	TArray<FString> Failures = EvaluateInvariants();

	FString Json = TEXT("{\n");
	Json += FString::Printf(TEXT("  \"duration\": %.0f,\n  \"passed\": %s,\n"), DurationSeconds, Failures.Num() == 0 ? TEXT("true") : TEXT("false"));
	Json += TEXT("  \"failures\": [");
	for (int32 i = 0; i < Failures.Num(); i++)
	{
		Json += FString::Printf(TEXT("%s\"%s\""), i ? TEXT(", ") : TEXT(""), *JsonEscape(Failures[i]));
	}
	Json += TEXT("],\n  \"counters\": {");
	TArray<FName> Keys;
	Counters.GetKeys(Keys);
	Keys.Sort(FNameLexicalLess());
	for (int32 i = 0; i < Keys.Num(); i++)
	{
		Json += FString::Printf(TEXT("%s\"%s\": %d"), i ? TEXT(", ") : TEXT(""), *Keys[i].ToString(), Counters[Keys[i]]);
	}
	// Per-god state-change counts (thrash measurement), also echoed to the log
	Json += TEXT("},\n  \"god_state_changes\": {");
	if (UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr)
	{
		TArray<AActor*> Gods;
		USkylandersActorRegistry::GetAllActorsOfClass(World, ASkylandersEnemyGod::StaticClass(), Gods);
		int32 Written = 0;
		for (AActor* A : Gods)
		{
			if (ASkylandersEnemyGod* G = Cast<ASkylandersEnemyGod>(A))
			{
				Json += FString::Printf(TEXT("%s\"%s\": %d"), Written++ ? TEXT(", ") : TEXT(""), *JsonEscape(G->GodName), G->StateChangeCount);
				UE_LOG(LogTemp, Warning, TEXT("[Smoke] god_state_change %s (%s) = %d"), *G->GodName,
					G->Team == ETowerTeam::Enemy ? TEXT("red") : TEXT("blue"), G->StateChangeCount);
			}
		}
	}
	Json += TEXT("},\n  \"errors\": [");
	for (int32 i = 0; i < ErrorSink.FirstErrors.Num(); i++)
	{
		Json += FString::Printf(TEXT("%s\"%s\""), i ? TEXT(", ") : TEXT(""), *JsonEscape(ErrorSink.FirstErrors[i]));
	}
	Json += FString::Printf(TEXT("],\n  \"error_count\": %d,\n  \"samples\": [\n"), ErrorSink.ErrorCount);
	for (int32 i = 0; i < Samples.Num(); i++)
	{
		Json += FString::Printf(TEXT("    %s%s\n"), *Samples[i].ToJson(), i + 1 < Samples.Num() ? TEXT(",") : TEXT(""));
	}
	Json += TEXT("  ]\n}\n");

	const FString Path = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("SmokeTest"), TEXT("report.json"));
	FFileHelper::SaveStringToFile(Json, *Path);

	if (Failures.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Smoke] PASS after %.0f s. Report: %s"), Elapsed, *Path);
	}
	else
	{
		for (const FString& F : Failures)
		{
			UE_LOG(LogTemp, Warning, TEXT("[Smoke] FAIL: %s"), *F);
		}
		UE_LOG(LogTemp, Warning, TEXT("[Smoke] FAIL (%d). Report: %s"), Failures.Num(), *Path);
	}
	GLog->Flush();
	// A clean (non-forced) exit always ends the process with code 0: the launcher
	// returns the init error level and ignores the quit status. So a pass exits
	// cleanly (which also proves shutdown works), and a fail terminates with 1
	// after the report and log are flushed.
	if (Failures.Num() == 0)
	{
		FPlatformMisc::RequestExitWithStatus(false, 0);
	}
	else
	{
		FPlatformMisc::RequestExitWithStatus(true, 1);
	}
}

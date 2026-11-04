// Skylanders Conquest - Player Character: Smoke-test bot
//
// Part of ASkylandersCharacter. The class is split across several
// translation units by concern; see SkylandersCharacter.cpp for the index.
//
// DriveAsBot exists ONLY for the headless smoke test (-SkySmoke=<s>): with no
// human at the keyboard the player otherwise idles in the fountain and the run
// never exercises shooting, abilities, damage taken or leveling. It is
// deliberately dumb and deterministic: walk the lane toward the nearest red
// target, stop at auto-attack range and shoot, cast what is ready, retreat to
// the fountain when low. It only calls the same methods the input bindings do
// (never the input system), so it drives the real gameplay code paths.

#include "SkylandersCharacter.h"
#include "SkylandersSmokeTest.h"
#include "SkylandersActorRegistry.h"
#include "SkylandersEnemyGod.h"
#include "SkylandersMinion.h"
#include "SkylandersSpawnArea.h"
#include "SkylandersItemCatalog.h"
#include "GameFramework/Controller.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"
#include "Engine/World.h"

namespace
{
	// Never push past this far into the enemy half (the red tower at 3800 shoots
	// out to ~2900); beyond it the bot would just feed the tower.
	constexpr float BotEnemySideLimit = 2500.0f;
	// With no blue wave on the lane, wait here (just in front of our own tower at
	// 3800 on our side) instead of walking into three red gods alone at mid.
	constexpr float BotWaitLine = -3000.0f;
	constexpr float BotAbilityRange = 900.0f;
	constexpr float BotUltRange = 600.0f;
	constexpr float BotRetreatBelow = 0.30f;
	constexpr float BotResumeAbove = 0.70f;
	constexpr float BotRepathInterval = 0.5f;
	constexpr int32 BotStarterItemID = 1; // Short Sword

	float AbilityManaCost(const ASkylandersCharacter* C, int32 i)
	{
		switch (i)
		{
		case 0: return C->Ability1_ManaCost;
		case 1: return C->Ability2_ManaCost;
		case 2: return C->Ability3_ManaCost;
		case 3: return C->Ability4_ManaCost;
		default: return 0.0f;
		}
	}

	float AbilityCooldown(const ASkylandersCharacter* C, int32 i)
	{
		switch (i)
		{
		case 0: return C->Ability1_RemainingCooldown;
		case 1: return C->Ability2_RemainingCooldown;
		case 2: return C->Ability3_RemainingCooldown;
		case 3: return C->Ability4_RemainingCooldown;
		default: return 0.0f;
		}
	}
}

void ASkylandersCharacter::DriveAsBot(float DeltaTime)
{
	if (!USkylandersSmokeTestSubsystem::IsRunning()) return;
	UWorld* World = GetWorld();
	if (!World || !Controller) return;

	BotElapsed += DeltaTime;

	// Finish a ground-aimed cast pressed last tick (they fire on release)
	if (BotPendingRelease >= 0)
	{
		OnAbilityReleased(BotPendingRelease);
		BotPendingRelease = -1;
	}

	if (CurrentHealth <= 0.0f) return; // dead: wait for the respawn timer

	// ---- Spend skill points: 1, 2, 3 first (starting points), then the ult
	// whenever it unlocks, else the lowest-rank basic ability.
	if (AbilityPoints > 0)
	{
		int32 Pick = -1;
		if (BotElapsed < 2.0f)
		{
			for (int32 i = 0; i < 3 && Pick < 0; i++)
			{
				if (AbilityLevels[i] == 0 && CanLevelAbility(i)) Pick = i;
			}
		}
		if (Pick < 0 && CanLevelAbility(3)) Pick = 3;
		if (Pick < 0)
		{
			for (int32 i = 0; i < 3; i++)
			{
				if (CanLevelAbility(i) && (Pick < 0 || AbilityLevels[i] < AbilityLevels[Pick])) Pick = i;
			}
		}
		if (Pick >= 0) LevelUpAbility(Pick);
	}

	// ---- Shop: BuyItem requires the spawn area, so only try while standing in
	// it (start of match, after a respawn or retreat), and only if not owned yet.
	if (bIsInSpawnArea && !Inventory.Contains(BotStarterItemID))
	{
		// Check the price first: a refused BuyItem logs a warning every frame
		const FSkylandersItemData* Item = USkylandersItemCatalog::FindItem(BotStarterItemID);
		if (Item && Coins >= Item->Cost)
		{
			BuyItem(BotStarterItemID);
		}
	}

	// ---- Where is home? The friendly fountain sits on one end of the lane; the
	// enemy side is the opposite sign of X.
	FVector HomeLoc = FVector::ZeroVector;
	if (AActor* Home = GetPlayerStart())
	{
		HomeLoc = Home->GetActorLocation();
	}
	const float EnemySign = (HomeLoc.X <= 0.0f) ? 1.0f : -1.0f;
	const FVector MyLoc = GetActorLocation();

	// ---- Health gate: retreat below 30%, resume above 70%
	const float EffMaxHP = FMath::Max(MaxHealth + ItemBonusStats.MaxHealth, 1.0f);
	const float HPFrac = CurrentHealth / EffMaxHP;
	if (!bBotRetreating && HPFrac < BotRetreatBelow && !bIsInSpawnArea)
	{
		bBotRetreating = true;
	}
	else if (bBotRetreating && HPFrac > BotResumeAbove)
	{
		bBotRetreating = false;
	}

	// ---- Stay with our wave: never stand in front of the front-most blue minion
	// (lone heroes at mid get deleted by the red gods and spend the match walking
	// back from the fountain). Signed distance along the lane toward red.
	float FrontLine = BotWaitLine;
	{
		TArray<AActor*> Found;
		USkylandersActorRegistry::GetAllActorsOfClass(World, ASkylandersMinion::StaticClass(), Found);
		for (AActor* A : Found)
		{
			const ASkylandersMinion* M = Cast<ASkylandersMinion>(A);
			if (!M || M->Team != ETowerTeam::Friendly || M->bDead) continue;
			FrontLine = FMath::Max(FrontLine, A->GetActorLocation().X * EnemySign);
		}
		FrontLine = FMath::Min(FrontLine, BotEnemySideLimit);
	}

	// ---- Target: nearest living red god within a lane-fight distance, else the
	// nearest red minion, else the nearest red god anywhere.
	AActor* Target = nullptr;
	bool bTargetIsGod = false;
	{
		TArray<AActor*> Found;
		float BestGod = TNumericLimits<float>::Max();
		AActor* NearestGod = nullptr;
		USkylandersActorRegistry::GetAllActorsOfClass(World, ASkylandersEnemyGod::StaticClass(), Found);
		for (AActor* A : Found)
		{
			const ASkylandersEnemyGod* G = Cast<ASkylandersEnemyGod>(A);
			if (!G || G->Team != ETowerTeam::Enemy || G->CurrentState == EGodAIState::Dead) continue;
			const float D = FVector::Dist2D(MyLoc, A->GetActorLocation());
			if (D < BestGod) { BestGod = D; NearestGod = A; }
		}

		AActor* NearestMinion = nullptr;
		float BestMinion = TNumericLimits<float>::Max();
		Found.Reset();
		USkylandersActorRegistry::GetAllActorsOfClass(World, ASkylandersMinion::StaticClass(), Found);
		for (AActor* A : Found)
		{
			const ASkylandersMinion* M = Cast<ASkylandersMinion>(A);
			if (!M || M->Team != ETowerTeam::Enemy || M->bDead) continue;
			const float D = FVector::Dist2D(MyLoc, A->GetActorLocation());
			if (D < BestMinion) { BestMinion = D; NearestMinion = A; }
		}

		if (NearestGod && BestGod <= 2500.0f) { Target = NearestGod; bTargetIsGod = true; }
		else if (NearestMinion) { Target = NearestMinion; }
		else if (NearestGod) { Target = NearestGod; bTargetIsGod = true; }
	}

	// ---- Choose a goal and whether to stand and shoot
	const float StopRange = 0.8f * AutoAttackRange;
	float TargetDist = TNumericLimits<float>::Max();
	if (Target) TargetDist = FVector::Dist2D(MyLoc, Target->GetActorLocation());

	bool bWantMove = false;
	FVector Goal = MyLoc;
	if (bBotRetreating)
	{
		// Walk home; once in the fountain, stand still and let it heal us
		if (!bIsInSpawnArea)
		{
			bWantMove = true;
			Goal = HomeLoc;
		}
	}
	else if (Target)
	{
		if (TargetDist > StopRange)
		{
			bWantMove = true;
			Goal = Target->GetActorLocation();
		}
	}
	else
	{
		// Nothing alive on red's side yet: walk up to the front line and wait
		bWantMove = true;
		Goal = FVector(EnemySign * FrontLine, 0.0f, MyLoc.Z);
	}

	// Never chase past our wave, and never into the red tower's zone (the front
	// line is already capped at the limit line): clamp the goal there.
	if (Goal.X * EnemySign > FrontLine)
	{
		Goal.X = EnemySign * FrontLine;
	}
	if (bWantMove && FVector::Dist2D(MyLoc, Goal) < 60.0f)
	{
		bWantMove = false; // parked at the limit line
	}

	// ---- Steering: follow the nav path (the lane has a titan and a phoenix
	// standing on the straight line between the fountain and mid).
	FVector MoveDir = FVector::ZeroVector;
	if (bWantMove)
	{
		BotRepathTimer -= DeltaTime;
		if (BotRepathTimer <= 0.0f || FVector::Dist2D(Goal, BotPathGoal) > 300.0f)
		{
			BotRepathTimer = BotRepathInterval;
			BotPathGoal = Goal;
			BotPath.Reset();
			if (UNavigationPath* Path = UNavigationSystemV1::FindPathToLocationSynchronously(World, MyLoc, Goal, this))
			{
				if (Path->IsValid())
				{
					BotPath = Path->PathPoints;
				}
			}
		}
		// Drop path points we have already reached
		while (BotPath.Num() > 0 && FVector::Dist2D(MyLoc, BotPath[0]) < 80.0f)
		{
			BotPath.RemoveAt(0);
		}
		const FVector Next = BotPath.Num() > 0 ? BotPath[0] : Goal;
		MoveDir = (Next - MyLoc).GetSafeNormal2D();
	}

	// ---- Facing: at the target when fighting (autos and ground aims follow
	// the control yaw), otherwise along the path so we never backpedal.
	const bool bFighting = !bBotRetreating && Target && TargetDist <= BotAbilityRange;
	FVector FaceDir = MoveDir;
	if (!bBotRetreating && Target && TargetDist <= FMath::Max(BotAbilityRange, AutoAttackRange))
	{
		FaceDir = (Target->GetActorLocation() - MyLoc).GetSafeNormal2D();
	}
	if (!FaceDir.IsNearlyZero())
	{
		Controller->SetControlRotation(FRotator(0.0f, FaceDir.Rotation().Yaw, 0.0f));
	}

	if (!MoveDir.IsNearlyZero())
	{
		AddMovementInput(MoveDir, 1.0f);
	}

	// Breadcrumb every few seconds so a failing run's log shows what the bot did
	if (FMath::FloorToInt((BotElapsed - DeltaTime) / 5.0f) != FMath::FloorToInt(BotElapsed / 5.0f))
	{
		UE_LOG(LogTemp, Log, TEXT("[SmokeBot] t=%.0f pos=(%.0f,%.0f) hp=%.0f%% lvl=%d %s target=%s dist=%.0f path=%d"),
			BotElapsed, MyLoc.X, MyLoc.Y, HPFrac * 100.0f, PlayerLevel,
			bBotRetreating ? TEXT("RETREAT") : (bWantMove ? TEXT("MOVE") : TEXT("HOLD")),
			Target ? *Target->GetName() : TEXT("none"), Target ? TargetDist : 0.0f, BotPath.Num());
	}

	if (bBotRetreating || !Target) return;

	// ---- Shoot whenever a shot can reach (FireProjectile rate-limits itself).
	// The bot still closes to StopRange before standing still; firing during the
	// last stretch of the approach just stops a short skirmish going unanswered.
	if (TargetDist <= AutoAttackRange)
	{
		FireProjectile();
	}

	// ---- Abilities: basics when ready and close, ult on a nearby god
	// A kit can refuse a cast for its own reasons (e.g. a channel in progress);
	// the throttle keeps a refused press from repeating (and logging) every frame.
	if (bFighting && BotPendingRelease < 0 && !IsChanneling() && BotElapsed >= BotNextCastTime)
	{
		auto TryCast = [this](int32 i) -> bool
		{
			if (AbilityLevels[i] <= 0 || AbilityCooldown(this, i) > 0.0f) return false;
			if (CurrentMana < AbilityManaCost(this, i)) return false;
			OnAbilityPressed(i);
			if (bAbilityUsesGroundAim[i]) BotPendingRelease = i;
			BotNextCastTime = BotElapsed + 0.5f;
			return true;
		};
		for (int32 i = 0; i < 3; i++)
		{
			if (TryCast(i)) return; // one cast per tick
		}
		if (bTargetIsGod && TargetDist <= BotUltRange)
		{
			TryCast(3);
		}
	}
}

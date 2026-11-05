// Skylanders Conquest - Headless smoke test implementation

#include "SkylandersSmokeTest.h"
#include "SkylandersActorRegistry.h"
#include "SkylandersBuffCamp.h"
#include "SkylandersCharacter.h"
#include "SkylandersCharacterSelectWidget.h"
#include "SkylandersEnemyGod.h"
#include "SkylandersGameInstance.h"
#include "SkylandersItemCatalog.h"
#include "SkylandersMainMenuWidget.h"
#include "SkylandersMinion.h"
#include "SkylandersSettingsWidget.h"
#include "SkylandersShopWidget.h"
#include "SkylandersTitan.h"
#include "SkylandersTower.h"
#include "SkylandersVictoryWidget.h"
#include "Blueprint/UserWidget.h"
#include "GameFramework/PlayerController.h"
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
	bItemTestRequested = FParse::Param(FCommandLine::Get(), TEXT("SkyItemTest"));
	bUISmokeRequested = FParse::Param(FCommandLine::Get(), TEXT("SkyUISmoke"));
	Counters.Reset();
	GLog->AddOutputDevice(&ErrorSink);
	TickHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &USkylandersSmokeTestSubsystem::Tick), 0.0f);
	UE_LOG(LogTemp, Warning, TEXT("[Smoke] Smoke test armed for %.0f s%s%s"), DurationSeconds,
		bItemTestRequested ? TEXT(" (+ item stat test)") : TEXT(""),
		bUISmokeRequested ? TEXT(" (UI construction test)") : TEXT(""));
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

	if (bUISmokeRequested)
	{
		return TickUISmoke(World);
	}

	// Nobody is at the keyboard: a scripted bot plays the player so combat,
	// abilities and leveling actually get exercised (see SkylandersCharacter_Bot.cpp).
	if (ASkylandersCharacter* P = Cast<ASkylandersCharacter>(UGameplayStatics::GetPlayerPawn(World, 0)))
	{
		if (CharacterName.IsEmpty())
		{
			CharacterName = P->CharacterName.IsEmpty() ? P->GetClass()->GetName() : P->CharacterName;
			const USkylandersGameInstance* GI = Cast<USkylandersGameInstance>(GetGameInstance());
			CharacterID = GI ? GI->SelectedCharacterID.ToString() : FString();
			UE_LOG(LogTemp, Warning, TEXT("[Smoke] character=%s (id=%s, class=%s)"),
				*CharacterName, *CharacterID, *P->GetClass()->GetName());
		}
		P->DriveAsBot(DeltaTime);

		// Shop probe: open the real shop widget mid-match for a moment, which
		// constructs it (ui_shop_opened) under live gold/inventory updates
		if (DurationSeconds >= ShopProbeAt + ShopProbeLength)
		{
			if (!bShopProbeOpened && Elapsed >= ShopProbeAt)
			{
				bShopProbeOpened = true;
				P->OpenShop();
				UE_LOG(LogTemp, Warning, TEXT("[Smoke] ui shop probe: opened at %.0f s"), Elapsed);
			}
			else if (bShopProbeOpened && !bShopProbeClosed && Elapsed >= ShopProbeAt + ShopProbeLength)
			{
				bShopProbeClosed = true;
				P->CloseShop();
				UE_LOG(LogTemp, Warning, TEXT("[Smoke] ui shop probe: closed at %.0f s"), Elapsed);
			}
		}
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

bool USkylandersSmokeTestSubsystem::TickUISmoke(UWorld* World)
{
	if (UISmokeFrames < 0)
	{
		const FString Map = UGameplayStatics::GetCurrentLevelName(World, true);
		UE_LOG(LogTemp, Warning, TEXT("[Smoke] ui: map=%s"), *Map);
		if (!Map.Contains(TEXT("MainMenu")))
		{
			UISmokeFailures.Add(FString::Printf(TEXT("-SkyUISmoke needs the MainMenu map, got %s"), *Map));
			Finish();
			return false;
		}
		APlayerController* PC = World->GetFirstPlayerController();
		if (!PC)
		{
			// The menu controller is spawned before BeginPlay finishes; wait for it
			if (Elapsed < DurationSeconds) return true;
			UISmokeFailures.Add(TEXT("no player controller on MainMenu"));
			Finish();
			return false;
		}

		auto Build = [&](UUserWidget* W, TFunctionRef<FString(UUserWidget*)> Setup)
		{
			if (!W)
			{
				UISmokeFailures.Add(TEXT("CreateWidget returned null"));
				return;
			}
			const FString ClassName = FString(W->GetClass()->GetPrefixCPP()) + W->GetClass()->GetName();
			if (!W->GetRootWidget())
			{
				UISmokeFailures.Add(FString::Printf(TEXT("%s built no widget tree"), *ClassName));
			}
			W->AddToViewport(60);
			UISmokeWidgets.Add(W);
			UISmokeNotes.Add(Setup(W));
		};

		Build(CreateWidget<USkylandersMainMenuWidget>(PC, USkylandersMainMenuWidget::StaticClass()),
			[](UUserWidget*) { return FString(); });

		Build(CreateWidget<USkylandersCharacterSelectWidget>(PC, USkylandersCharacterSelectWidget::StaticClass()),
			[this](UUserWidget* W)
			{
				USkylandersCharacterSelectWidget* Sel = CastChecked<USkylandersCharacterSelectWidget>(W);
				const TArray<FSkylanderSelectEntry>& Roster = FSkylanderSelectEntry::GetRoster();
				if (Roster.Num() != 4)
				{
					UISmokeFailures.Add(FString::Printf(TEXT("character select roster has %d entries, expected 4"), Roster.Num()));
				}
				// Every tile's selection path: info panel rebuild, preview swap, highlight
				for (const FSkylanderSelectEntry& E : Roster)
				{
					Sel->SelectCharacter(E.ID);
				}
				return FString::Printf(TEXT("roster=%d"), Roster.Num());
			});

		// No pawn on the menu, so the shop has no owner: it must still build,
		// and the RECOMMENDED filter falls back to the bruiser build
		Build(CreateWidget<USkylandersShopWidget>(PC, USkylandersShopWidget::StaticClass()),
			[this](UUserWidget* W)
			{
				USkylandersShopWidget* Shop = CastChecked<USkylandersShopWidget>(W);
				Shop->SetFilter(USkylandersShopWidget::FilterRecommended);
				const int32 Recommended = Shop->GetVisibleCardCount();
				if (Recommended != 6)
				{
					UISmokeFailures.Add(FString::Printf(TEXT("shop RECOMMENDED shows %d cards, expected 6"), Recommended));
				}
				Shop->SetFilter(-1);
				const int32 All = Shop->GetVisibleCardCount();
				if (All != USkylandersItemCatalog::GetAllItems().Num())
				{
					UISmokeFailures.Add(FString::Printf(TEXT("shop ALL shows %d cards, catalog has %d"), All, USkylandersItemCatalog::GetAllItems().Num()));
				}
				return FString::Printf(TEXT("recommended=%d all=%d"), Recommended, All);
			});

		Build(CreateWidget<USkylandersVictoryWidget>(PC, USkylandersVictoryWidget::StaticClass()),
			[](UUserWidget* W)
			{
				// No player: the end screen must still fill in (gods only, "-" tiles)
				USkylandersVictoryWidget* V = CastChecked<USkylandersVictoryWidget>(W);
				V->ShowResult(false, nullptr);
				V->ShowResult(true, nullptr);
				return FString(TEXT("ShowResult(null) x2"));
			});

		Build(CreateWidget<USkylandersSettingsWidget>(PC, USkylandersSettingsWidget::StaticClass()),
			[](UUserWidget*) { return FString(); });

		UISmokeFrames = 0;
		return true;
	}

	// Let the widgets live through a few engine frames before checking them
	if (++UISmokeFrames < UISmokeFrameCount && Elapsed < DurationSeconds) return true;

	for (int32 i = 0; i < UISmokeWidgets.Num(); i++)
	{
		UUserWidget* W = UISmokeWidgets[i];
		if (!W) continue;
		const FString ClassName = FString(W->GetClass()->GetPrefixCPP()) + W->GetClass()->GetName();
		if (W->IsInViewport())
		{
			UISmokeOk++;
			SKY_COUNT("ui_widget_ok");
			const FString& Note = UISmokeNotes.IsValidIndex(i) ? UISmokeNotes[i] : FString();
			UE_LOG(LogTemp, Warning, TEXT("[Smoke] ui %s ok%s%s"), *ClassName, Note.IsEmpty() ? TEXT("") : TEXT(" - "), *Note);
		}
		else
		{
			UISmokeFailures.Add(FString::Printf(TEXT("%s left the viewport on its own"), *ClassName));
		}
		W->RemoveFromParent();
	}
	UISmokeWidgets.Reset();
	Finish();
	return false;
}

void USkylandersSmokeTestSubsystem::BuildEndScreenProbe(UWorld* World)
{
	if (!World) return;
	APlayerController* PC = World->GetFirstPlayerController();
	ASkylandersCharacter* P = Cast<ASkylandersCharacter>(UGameplayStatics::GetPlayerPawn(World, 0));

	// Same construction as ASkylandersTitan::ShowEndScreen, minus the pause and
	// the input capture, so the process can still exit right after
	USkylandersVictoryWidget* W = PC
		? CreateWidget<USkylandersVictoryWidget>(PC, USkylandersVictoryWidget::StaticClass())
		: CreateWidget<USkylandersVictoryWidget>(World, USkylandersVictoryWidget::StaticClass());
	if (!W)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Smoke] ui end screen: CreateWidget returned null"));
		return;
	}
	W->AddToViewport(100);
	W->ShowResult(true, P); // counts ui_endscreen_built
	UE_LOG(LogTemp, Warning, TEXT("[Smoke] ui end screen built (player=%s)"), P ? *P->CharacterName : TEXT("none"));
	W->RemoveFromParent();
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
		// Before reading the player, so this sample already shows the restored state
		if (bItemTestRequested && !bItemTestRan)
		{
			RunItemTest(World, P);
		}
		S.PlayerHealth = P->CurrentHealth;
		S.PlayerLevel = P->PlayerLevel;
		S.PlayerCoins = P->Coins;
		S.PlayerKills = P->Kills;
		S.PlayerDeaths = P->Deaths;
	}

	MaxMinionsSeen = FMath::Max(MaxMinionsSeen, S.Minions);
	Samples.Add(S);
}

void USkylandersSmokeTestSubsystem::RunItemTest(UWorld* World, ASkylandersCharacter* P)
{
	bItemTestRan = true;
	UE_LOG(LogTemp, Warning, TEXT("[Smoke] item_test: starting on %s"), *P->GetName());

	// The test buys/sells ~90 times and lands a few test hits; none of that is
	// gameplay, so every counter is put back afterwards and the normal
	// invariants (item_bought, god_damage, ...) still measure the real match.
	const TMap<FName, int32> SavedCounters = Counters;

	// Starting state, restored at the end so the rest of the match proceeds
	const TArray<int32> SavedInventory = P->Inventory;
	const int32 SavedCoins = P->Coins;
	const int32 SavedTotalGold = P->TotalGoldEarned;
	const bool bSavedInSpawn = P->bIsInSpawnArea;
	const float SavedHealth = P->CurrentHealth;
	const float SavedMana = P->CurrentMana;
	const float SavedWalkSpeed = P->BaseMaxWalkSpeed;

	// BuyItem/SellItem only work in the fountain. The pawn stands there at the
	// first sample anyway, but the check must not depend on where it happens to be.
	P->bIsInSpawnArea = true;

	auto Fail = [this](const FString& Msg)
	{
		ItemTestFailures.Add(Msg);
		UE_LOG(LogTemp, Warning, TEXT("[Smoke] item_test FAIL: %s"), *Msg);
	};
	auto Expect = [&](const FString& Ctx, const TCHAR* What, float Got, float Want, float Tol)
	{
		ItemTestChecks++;
		if (!FMath::IsNearlyEqual(Got, Want, Tol))
		{
			Fail(FString::Printf(TEXT("%s: %s = %.4f, expected %.4f"), *Ctx, What, Got, Want));
		}
	};
	auto ExpectTrue = [&](const FString& Ctx, bool bOk, const FString& What)
	{
		ItemTestChecks++;
		if (!bOk) Fail(Ctx + TEXT(": ") + What);
	};
	struct FStatField { const TCHAR* Name; float FSkylandersItemStats::* Member; };
	static const FStatField StatFields[] = {
		{ TEXT("Power"), &FSkylandersItemStats::Power },
		{ TEXT("AttackSpeed"), &FSkylandersItemStats::AttackSpeed },
		{ TEXT("CritChance"), &FSkylandersItemStats::CritChance },
		{ TEXT("MaxHealth"), &FSkylandersItemStats::MaxHealth },
		{ TEXT("MaxMana"), &FSkylandersItemStats::MaxMana },
		{ TEXT("HealthRegen"), &FSkylandersItemStats::HealthRegen },
		{ TEXT("ManaRegen"), &FSkylandersItemStats::ManaRegen },
		{ TEXT("Protections"), &FSkylandersItemStats::Protections },
		{ TEXT("Lifesteal"), &FSkylandersItemStats::Lifesteal },
		{ TEXT("MovementSpeed"), &FSkylandersItemStats::MovementSpeed },
		{ TEXT("CooldownReduction"), &FSkylandersItemStats::CooldownReduction },
		{ TEXT("Penetration"), &FSkylandersItemStats::Penetration },
	};
	auto ExpectStats = [&](const FString& Ctx, const FSkylandersItemStats& Got, const FSkylandersItemStats& Want)
	{
		for (const FStatField& F : StatFields)
		{
			Expect(Ctx, *FString::Printf(TEXT("ItemBonusStats.%s"), F.Name), Got.*F.Member, Want.*F.Member, 1e-3f);
		}
	};
	auto ClearInventory = [P]()
	{
		for (int32& Id : P->Inventory) Id = 0;
		P->RecalculateItemBonuses();
	};
	// Through AddCoins, not a direct write: it keeps a Blueprint HUD's coin accumulator in sync
	auto SetCoins = [P](int32 N) { P->AddCoins(N - P->Coins); };
	auto BuyAll = [&](const FString& Ctx, const TArray<FSkylandersItemData>& Items) -> bool
	{
		int32 Total = 0;
		for (const FSkylandersItemData& It : Items) Total += It.Cost;
		SetCoins(Total + 100);
		bool bAll = true;
		for (const FSkylandersItemData& It : Items)
		{
			const bool bBought = P->BuyItem(It.ItemID);
			ExpectTrue(Ctx, bBought, FString::Printf(TEXT("BuyItem(%s) returned false"), *It.ItemName));
			bAll &= bBought;
		}
		return bAll;
	};

	const TArray<FSkylandersItemData>& Catalog = USkylandersItemCatalog::GetAllItems();

	// ---- 1. Every item alone: stats land in ItemBonusStats and in every getter
	// that consumes them; selling refunds 67% and clears them again.
	ClearInventory();
	const float EmptyProtections = P->GetEffectiveProtections();
	const float EmptyWalkSpeed = P->BaseMaxWalkSpeed;
	{
		// The empty-inventory baseline must itself be the base/level formula
		const float LevelProtections = P->BaseProtections + P->ProtectionsPerLevel * FMath::Max(P->PlayerLevel - 1, 0) + P->BuffProtections;
		Expect(TEXT("empty inventory"), TEXT("GetEffectiveProtections()"), EmptyProtections, LevelProtections, 1e-3f);
		ExpectStats(TEXT("empty inventory"), P->ItemBonusStats, FSkylandersItemStats());
	}
	for (const FSkylandersItemData& Item : Catalog)
	{
		const FString Ctx = FString::Printf(TEXT("%s (id %d)"), *Item.ItemName, Item.ItemID);
		const FSkylandersItemStats& S = Item.Stats;
		ClearInventory();
		SetCoins(Item.Cost + 100);
		const int32 CoinsBeforeBuy = P->Coins;
		if (!P->BuyItem(Item.ItemID))
		{
			ItemTestChecks++;
			Fail(FString::Printf(TEXT("%s: BuyItem returned false with %d coins (cost %d)"), *Ctx, CoinsBeforeBuy, Item.Cost));
			continue;
		}
		ItemTestItemsChecked++;
		Expect(Ctx, TEXT("coins after buy"), (float)P->Coins, (float)(CoinsBeforeBuy - Item.Cost), 0.5f);
		ExpectTrue(Ctx, P->Inventory[0] == Item.ItemID, FString::Printf(TEXT("slot 0 holds id %d"), P->Inventory[0]));
		ExpectStats(Ctx, P->ItemBonusStats, S);
		Expect(Ctx, TEXT("GetEffectivePower()"), P->GetEffectivePower(), P->BasePower + S.Power, 1e-3f);
		Expect(Ctx, TEXT("GetEffectiveAttackSpeed()"), P->GetEffectiveAttackSpeed(), P->FireRate + S.AttackSpeed, 1e-3f);
		Expect(Ctx, TEXT("GetEffectiveProtections()"), P->GetEffectiveProtections(), EmptyProtections + S.Protections, 1e-3f);
		Expect(Ctx, TEXT("BaseMaxWalkSpeed"), P->BaseMaxWalkSpeed, EmptyWalkSpeed + S.MovementSpeed, 1e-3f);
		Expect(Ctx, TEXT("GetPenetrationOf(player)"), ASkylandersCharacter::GetPenetrationOf(P), S.Penetration, 1e-3f);
		Expect(Ctx, TEXT("GetCooldownMultiplier()"), P->GetCooldownMultiplier(), 1.0f - FMath::Min(0.40f, S.CooldownReduction), 1e-3f);
		Expect(Ctx, TEXT("GetUltCooldownMultiplier()"), P->GetUltCooldownMultiplier(), 1.0f - FMath::Min(0.25f, S.CooldownReduction), 1e-3f);

		const int32 CoinsBeforeSell = P->Coins;
		ExpectTrue(Ctx, P->SellItem(0), TEXT("SellItem(0) returned false"));
		Expect(Ctx, TEXT("coins after sell"), (float)P->Coins, (float)(CoinsBeforeSell + Item.GetSellPrice()), 0.5f);
		ExpectStats(Ctx + TEXT(" after sell"), P->ItemBonusStats, FSkylandersItemStats());
		Expect(Ctx + TEXT(" after sell"), TEXT("GetCooldownMultiplier()"), P->GetCooldownMultiplier(), 1.0f, 1e-3f);
	}
	ItemTestNotes += FString::Printf(TEXT("\"catalog_items\": %d, "), Catalog.Num());

	// ---- 2a. CDR stacking: the three highest-CDR items sum past both caps;
	// abilities 1-3 clamp at 40% (x0.60), the ult at 25% (x0.75).
	{
		TArray<FSkylandersItemData> ByCdr = Catalog;
		ByCdr.StableSort([](const FSkylandersItemData& A, const FSkylandersItemData& B) { return A.Stats.CooldownReduction > B.Stats.CooldownReduction; });
		ByCdr.SetNum(3);
		float Sum = 0.0f;
		FString Ids;
		for (const FSkylandersItemData& It : ByCdr) { Sum += It.Stats.CooldownReduction; Ids += FString::Printf(TEXT("%s%d"), Ids.IsEmpty() ? TEXT("") : TEXT(", "), It.ItemID); }
		const FString Ctx = FString::Printf(TEXT("CDR stack [%s] sum %.2f"), *Ids, Sum);
		ClearInventory();
		BuyAll(Ctx, ByCdr);
		ExpectTrue(Ctx, Sum > 0.40f, TEXT("the top three CDR items no longer sum past the 40% cap"));
		Expect(Ctx, TEXT("ItemBonusStats.CooldownReduction (uncapped sum)"), P->ItemBonusStats.CooldownReduction, Sum, 1e-3f);
		Expect(Ctx, TEXT("GetCooldownMultiplier()"), P->GetCooldownMultiplier(), 0.60f, 1e-3f);
		Expect(Ctx, TEXT("GetUltCooldownMultiplier()"), P->GetUltCooldownMultiplier(), 0.75f, 1e-3f);
		ItemTestNotes += FString::Printf(TEXT("\"cdr_stack\": {\"items\": [%s], \"sum\": %.2f, \"multiplier\": %.3f, \"ult_multiplier\": %.3f}, "),
			*Ids, Sum, P->GetCooldownMultiplier(), P->GetUltCooldownMultiplier());
	}

	// ---- 2b/3. Penetration: two pen items sum, and a 100-damage hit on a red god
	// loses exactly 100 * (1 - P'/(P'+100)), P' = max(0, Prot - Pen). Checked with
	// no pen, the smallest pen item alone, and the two biggest together, so both
	// the partial strip and (usually) the floor at 0 are covered.
	{
		TArray<FSkylandersItemData> PenItems = Catalog.FilterByPredicate([](const FSkylandersItemData& It) { return It.Stats.Penetration > 0.0f; });
		PenItems.StableSort([](const FSkylandersItemData& A, const FSkylandersItemData& B) { return A.Stats.Penetration > B.Stats.Penetration; });
		if (PenItems.Num() < 2)
		{
			ItemTestChecks++;
			Fail(FString::Printf(TEXT("catalog has %d penetration items, need 2"), PenItems.Num()));
		}
		else
		{
			// A living red god with health to spare (the hit must not kill it)
			ASkylandersEnemyGod* God = nullptr;
			TArray<AActor*> Gods;
			USkylandersActorRegistry::GetAllActorsOfClass(World, ASkylandersEnemyGod::StaticClass(), Gods);
			for (AActor* A : Gods)
			{
				ASkylandersEnemyGod* G = Cast<ASkylandersEnemyGod>(A);
				if (G && G->Team == ETowerTeam::Enemy && G->CurrentState != EGodAIState::Dead && G->CurrentHealth > 200.0f)
				{
					God = G;
					break;
				}
			}

			const TArray<FSkylandersItemData> Configs[] = {
				{},
				{ PenItems.Last() },
				{ PenItems[0], PenItems[1] },
			};
			FString MathJson;
			for (const TArray<FSkylandersItemData>& Config : Configs)
			{
				float WantPen = 0.0f;
				FString Ids;
				for (const FSkylandersItemData& It : Config) { WantPen += It.Stats.Penetration; Ids += FString::Printf(TEXT("%s%d"), Ids.IsEmpty() ? TEXT("") : TEXT(", "), It.ItemID); }
				const FString Ctx = FString::Printf(TEXT("pen [%s]"), *Ids);
				ClearInventory();
				BuyAll(Ctx, Config);
				const float Pen = ASkylandersCharacter::GetPenetrationOf(P);
				Expect(Ctx, TEXT("GetPenetrationOf(player)"), Pen, WantPen, 1e-3f);
				if (Config.Num() == 2)
				{
					ItemTestNotes += FString::Printf(TEXT("\"pen_stack\": {\"items\": [%s], \"penetration\": %.1f}, "), *Ids, Pen);
				}
				if (!God)
				{
					ItemTestChecks++;
					Fail(Ctx + TEXT(": no living red god to hit"));
					continue;
				}
				const float Prot = God->GetEffectiveProtections();
				const float Eff = FMath::Max(0.0f, Prot - Pen);
				const float WantLost = 100.0f * (1.0f - Eff / (Eff + 100.0f));
				const float HealthBefore = God->CurrentHealth;
				God->TakeDamage_Custom(100.0f, P);
				const float Lost = HealthBefore - God->CurrentHealth;
				God->CurrentHealth = HealthBefore;
				Expect(Ctx + FString::Printf(TEXT(" vs %s (prot %.1f)"), *God->GodName, Prot), TEXT("god health lost to 100 damage"), Lost, WantLost, 0.5f);
				MathJson += FString::Printf(TEXT("%s{\"pen\": %.1f, \"god\": \"%s\", \"god_prot\": %.1f, \"expected_loss\": %.2f, \"loss\": %.2f}"),
					MathJson.IsEmpty() ? TEXT("") : TEXT(", "), Pen, *JsonEscape(God->GodName), Prot, WantLost, Lost);
			}
			ItemTestNotes += FString::Printf(TEXT("\"pen_math\": [%s], "), *MathJson);
		}
	}

	// ---- 4. Full inventory: a seventh purchase is refused and costs nothing
	{
		TArray<FSkylandersItemData> Cheapest = Catalog;
		Cheapest.StableSort([](const FSkylandersItemData& A, const FSkylandersItemData& B) { return A.Cost < B.Cost; });
		const FSkylandersItemData Seventh = Cheapest[ASkylandersCharacter::MaxInventorySlots];
		Cheapest.SetNum(ASkylandersCharacter::MaxInventorySlots);
		const FString Ctx = TEXT("full inventory");
		ClearInventory();
		BuyAll(Ctx, Cheapest);
		SetCoins(Seventh.Cost + 100);
		const int32 CoinsBefore = P->Coins;
		const TArray<int32> InvBefore = P->Inventory;
		ExpectTrue(Ctx, !P->BuyItem(Seventh.ItemID), FString::Printf(TEXT("BuyItem(%s) succeeded with six items held"), *Seventh.ItemName));
		Expect(Ctx, TEXT("coins after refused buy"), (float)P->Coins, (float)CoinsBefore, 0.5f);
		ExpectTrue(Ctx, P->Inventory == InvBefore, TEXT("inventory changed by a refused buy"));
	}

	// ---- Restore the player exactly as found
	P->Inventory = SavedInventory;
	P->RecalculateItemBonuses();
	SetCoins(SavedCoins);
	P->TotalGoldEarned = SavedTotalGold;
	P->bIsInSpawnArea = bSavedInSpawn;
	P->CurrentHealth = SavedHealth;
	P->CurrentMana = SavedMana;
	P->BaseMaxWalkSpeed = SavedWalkSpeed;
	P->UpdateHUD();

	Counters = SavedCounters;
	Count(TEXT("item_test_fail"), 0); // present in the report even when clean
	for (int32 i = 0; i < ItemTestFailures.Num(); i++)
	{
		SKY_COUNT("item_test_fail");
	}
	UE_LOG(LogTemp, Warning, TEXT("[Smoke] item_test: %d/%d items checked, %d checks, %d failure(s)"),
		ItemTestItemsChecked, Catalog.Num(), ItemTestChecks, ItemTestFailures.Num());
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

	if (bUISmokeRequested)
	{
		// UI construction run on the menu: no match, so none of the match invariants apply
		Failures.Append(UISmokeFailures);
		Require(UISmokeOk == UISmokeExpected, FString::Printf(TEXT("%d/%d UI widgets built and stayed in the viewport"), UISmokeOk, UISmokeExpected));
		Require(ErrorSink.ErrorCount == 0, FString::Printf(TEXT("%d error(s) logged"), ErrorSink.ErrorCount));
		return Failures;
	}

	Require(Samples.Num() > 0, TEXT("no samples taken (world never began play?)"));
	if (Samples.Num() == 0) return Failures;
	const FSample& Last = Samples.Last();

	Require(Last.Gods >= 5, FString::Printf(TEXT("expected 5 gods, saw %d"), Last.Gods));
	Require(Last.Towers == 4, FString::Printf(TEXT("expected 4 towers, saw %d"), Last.Towers));
	Require(Last.Titans == 2, FString::Printf(TEXT("expected 2 titans, saw %d"), Last.Titans));
	Require(Last.Camps >= 4, FString::Printf(TEXT("expected >=4 buff camps, saw %d"), Last.Camps));
	// The map builder instances its rock/tree/landmark dressing at BeginPlay
	Require(C(TEXT("dressing_placed")) >= 50, FString::Printf(TEXT("dressing_placed=%d < 50"), C(TEXT("dressing_placed"))));
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
	if (DurationSeconds >= 120.0f)
	{
		// The bot is levelled to 5 from 25 s and casts the ult on anything red in
		// reach, so a full run must land at least one (counted on completion).
		Require(C(TEXT("player_ult")) >= 1, TEXT("player bot never completed an ultimate"));
		// The shop probe (t=60) and the end-screen probe (Finish) must have built both screens
		Require(C(TEXT("ui_shop_opened")) >= 1, FString::Printf(TEXT("ui_shop_opened=%d: the shop probe never constructed the shop"), C(TEXT("ui_shop_opened"))));
		Require(C(TEXT("ui_endscreen_built")) >= 1, FString::Printf(TEXT("ui_endscreen_built=%d: the end screen was never built"), C(TEXT("ui_endscreen_built"))));
	}
	if (bItemTestRequested)
	{
		Require(bItemTestRan, TEXT("-SkyItemTest given but the item test never ran (no player pawn?)"));
		Require(C(TEXT("item_test_fail")) == 0, FString::Printf(TEXT("item_test_fail=%d (see item_test.failures)"), C(TEXT("item_test_fail"))));
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
	if (!bUISmokeRequested)
	{
		BuildEndScreenProbe(GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr);
	}
	TArray<FString> Failures = EvaluateInvariants();

	FString Json = TEXT("{\n");
	Json += FString::Printf(TEXT("  \"duration\": %.0f,\n  \"passed\": %s,\n"), DurationSeconds, Failures.Num() == 0 ? TEXT("true") : TEXT("false"));
	Json += FString::Printf(TEXT("  \"character\": \"%s\",\n  \"character_id\": \"%s\",\n"), *JsonEscape(CharacterName), *JsonEscape(CharacterID));
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
	Json += TEXT("},\n");
	if (bItemTestRequested)
	{
		Json += FString::Printf(TEXT("  \"item_test\": {\"ran\": %s, \"items_checked\": %d, %s\"checks\": %d, \"fail_count\": %d, \"failures\": ["),
			bItemTestRan ? TEXT("true") : TEXT("false"), ItemTestItemsChecked, *ItemTestNotes, ItemTestChecks, ItemTestFailures.Num());
		for (int32 i = 0; i < ItemTestFailures.Num(); i++)
		{
			Json += FString::Printf(TEXT("%s\"%s\""), i ? TEXT(", ") : TEXT(""), *JsonEscape(ItemTestFailures[i]));
		}
		Json += TEXT("]},\n");
	}
	Json += TEXT("  \"errors\": [");
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

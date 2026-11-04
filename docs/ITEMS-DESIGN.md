# Items Design — 36-item catalog, CDR + Penetration, AI build archetypes

Spec for a coder. Nothing here is implemented yet. All file paths are under
`Source/Skylanders_Conquest/`. Line numbers are as of 2026-11-04 and are hints,
not contracts — match on the quoted code.

---

## 1. Two new stat fields

### 1.1 Semantics

| Field | Type / units | Semantics | Cap |
|---|---|---|---|
| `CooldownReduction` | float, fraction (0.10 = 10%) | Summed across all owned items. Applied **once, at the moment a cooldown is set**: `Remaining = BaseCooldown * RankScale * (1 - CDR)`. Never applied to a running timer, never to auto-attack fire rate. | Sum clamped to **0.40** at use (not at purchase; a 45% build simply wastes 5%). |
| `Penetration` | float, flat protections | Summed across all owned items. Subtracted from the **target's** effective protections before the reduction formula: `EffProt = max(0, TargetProt - AttackerPen)`; `Reduction = EffProt / (EffProt + 100)`. Applies to autos and abilities alike (anything that routes through a `TakeDamage_Custom` with a causer). Minions / towers / titans / camps have no protections, so pen is a no-op on them. | Floor at 0 protections (never negative, never bonus damage). No cap on the stat itself. |

Both are per-item flat values that add in `FSkylandersItemStats::operator+`.

### 1.2 Code sites (exhaustive)

**A. `SkylandersItem.h` — `struct FSkylandersItemStats`**
- Add after `MovementSpeed`:
  ```cpp
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
  float CooldownReduction = 0.0f; // 0.0 - 1.0; sum of items, clamped to 0.40 where used

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
  float Penetration = 0.0f; // Flat; subtracted from target protections before reduction (floor 0)
  ```
- `operator+`: add `Result.CooldownReduction = CooldownReduction + Other.CooldownReduction;` and `Result.Penetration = Penetration + Other.Penetration;`.

**B. `SkylandersItemCatalog.cpp` — `USkylandersItemCatalog::InitializeItems()`**
- Extend the `St` lambda with two trailing defaulted params (see §2.3 for the exact replacement). Items 1–20 untouched. Append items 21–36.

**C. `SkylandersCharacter.h`**
- Line ~462: change `void TakeDamage_Custom(float DamageAmount);` to
  `void TakeDamage_Custom(float DamageAmount, AActor* DamageCauser = nullptr);`
  (default keeps every existing minion/tower/titan/camp/spawn-area caller compiling unchanged).
- Next to `GetEffectiveProtections()` declare:
  ```cpp
  float GetCooldownMultiplier() const;                        // 1 - clamp(ItemBonusStats.CooldownReduction, 0, 0.40)
  static float GetPenetrationOf(const AActor* DamageCauser);  // pen of the pawn behind a hit (0 if none)
  ```

**D. `SkylandersCharacter_Shop.cpp`**
- After `ASkylandersCharacter::GetEffectiveProtections()` (line ~352) add:
  ```cpp
  float ASkylandersCharacter::GetCooldownMultiplier() const
  {
      return 1.0f - FMath::Clamp(ItemBonusStats.CooldownReduction, 0.0f, 0.40f);
  }
  ```
- `RecalculateItemBonuses()` log (lines ~337–339): optionally append CDR/Pen to the format string. Not functional.

**E. `SkylandersCharacter_Stats.cpp`**
- `ASkylandersCharacter::TakeDamage_Custom` (line 74): new signature; replace
  `float Prots = GetEffectiveProtections();` with
  `float Prots = FMath::Max(0.0f, GetEffectiveProtections() - GetPenetrationOf(DamageCauser));`
- Add (needs `#include "SkylandersEnemyGod.h"`, which this file does not include today):
  ```cpp
  float ASkylandersCharacter::GetPenetrationOf(const AActor* DamageCauser)
  {
      if (!DamageCauser) return 0.0f;
      // Projectiles and AoEs pass themselves (or GetInstigator()) as the causer with the shooter as instigator.
      const AActor* Src = DamageCauser->GetInstigator() ? static_cast<const AActor*>(DamageCauser->GetInstigator()) : DamageCauser;
      if (const ASkylandersCharacter* C = Cast<ASkylandersCharacter>(Src)) return C->ItemBonusStats.Penetration;
      if (const ASkylandersEnemyGod*  G = Cast<ASkylandersEnemyGod>(Src))  return G->ItemBonusStats.Penetration;
      return 0.0f;
  }
  ```

**F. Player cooldown-set sites — 12 lines, same edit each**
Every `AbilityN_RemainingCooldown = AbilityN_Cooldown * CooldownScale;` becomes
`AbilityN_RemainingCooldown = AbilityN_Cooldown * CooldownScale * GetCooldownMultiplier();`
- `SkylandersCharacter_HUD.cpp` — `UseAbility1..4` (Trigger Happy): lines ~401, 464, 533, 583.
- `SkylandersHexCharacter.cpp` — `UseAbility1..4`: lines ~149, 191, 239, 278.
- `SkylandersTreeRexCharacter.cpp` — `UseAbility1..4`: lines ~201, 240, 284, 339.
- `UpdateCooldownUI()` (`SkylandersCharacter_HUD.cpp` ~330) only prints seconds-left; no change.
- `UseAbilityByIndex` in `SkylandersCharacter_Abilities.cpp` (~526) only reads remaining; no change.

**G. `SkylandersEnemyGod.h`**
- Declare `float GetCooldownMultiplier() const;` (same body as the player's, on the god's `ItemBonusStats`).
- Add the archetype enum + member and change the priority getter (see §3.3).

**H. `SkylandersEnemyGod.cpp`**
- `TakeDamage_Custom` (line ~1219): the `CauserPawn` resolution block (~1226–1232) currently sits *after* the formula. Keep it where it is for `LastDamageCauser`, and change the formula line to
  `float EffProtections = FMath::Max(0.0f, GetEffectiveProtections() - ASkylandersCharacter::GetPenetrationOf(DamageCauser));`
- `PerformAttack` (line ~1010): the two lifesteal pre-mitigation blocks (~1069–1073 player target, ~1090–1094 god target) compute `TargetProtections` for lifesteal only. Subtract own pen there too (`FMath::Max(0.0f, X->GetEffectiveProtections() - ItemBonusStats.Penetration)`) and pass the causer to the player: `PlayerTarget->TakeDamage_Custom(EffectiveDamage, this);`.
- `UseAbility` (line ~1136): same two edits at ~1184–1196 (`PlayerHero->TakeDamage_Custom(BurstDamage, this);` and pen-adjusted `TargetProtections`).
- God CDR: the two `AbilityCooldownTimer = 8.0f;` that follow a `UseAbility()` call (~939 and ~965) become `AbilityCooldownTimer = 8.0f * GetCooldownMultiplier();`. Leave the respawn reset (~1408) and the constructor alone.

**I. `SkylandersShopWidget.cpp` — `BuildStatSummary()` (line ~101)**
- Add two lines in the same style:
  `if (Stats.CooldownReduction > 0) Parts.Add(Printf("+%.0f%% CDR", CDR*100));`
  `if (Stats.Penetration > 0) Parts.Add(Printf("+%.0f Pen", Pen));`

**J. `SkylandersShopWidget` buy buttons — pre-existing bug, blocks this work**
- `BuildLayout()` (~324) binds each BUY button with `BindUFunction(this, FName("OnBuyItem%d"))`, but only `OnBuyItem1..12` exist (`.h` 106–117, `.cpp` 607–618). **Items 13–20 are already unbuyable from the shop UI** (unbound delegate = dead button). Fix before adding 21–36: either add `OnBuyItem13..36` in the same style (24 UFUNCTIONs), or replace the scheme with a tiny `UButton` subclass holding `int32 ItemID` and one `UFUNCTION() HandleClicked()`. The second is cleaner; the first matches existing style and is 48 lines.

Not affected: `SkylandersProjectile.cpp` / `SkylandersAoEExplosion.cpp` (they already pass a causer whose instigator is the shooter), `SkylandersMinion/Tower/Titan/BuffCamp` (no protections), telemetry.

---

## 2. Catalog

### 2.1 Stat-to-gold budget (defined once; every item is costed from this)

| Stat | Gold | Stat | Gold |
|---|---|---|---|
| 1 Power | **30** | 1 Protection | 25 |
| 0.1 Attack Speed | 120 | 1% Lifesteal | 40 |
| 1% Crit | 30 | 10 Move Speed | 60 |
| 10 Max Health | 25 | 1% CDR | 35 |
| 10 Max Mana | 15 | 1 Penetration | 35 |
| 1 HP/s | 40 | 1 MP/s | 40 |

Power is 30 g (not 20) because that is what items 1–8 are actually priced at; at 20 g the new offense items would obsolete every legacy one. Items may sit 5–15% over/under budget for identity. "Δ" below = (budget − cost) / cost; positive = good value.

Tiers per category: 3 starters (450–700), 5 mids (900–1500), 4 capstones (1700–2600). Item 12 (1700) is counted as a capstone.

Legacy note (not changed in this pass, flagged for a later re-cost): items 15–20 are 25–60% over budget (mana/MP-s items worst: Crystal Wand ≈ 250 g of stats for 600), and 7/13/14 are 15–20% over. Items 1–20 keep their IDs, names and stats because `GetItemPurchasePriority()` and telemetry reference them.

### 2.2 Full table

**OFFENSE**

| ID | Name | Tier | Cost | Stats | Budget | Δ |
|---|---|---|---|---|---|---|
| 1 | Short Sword | starter | 500 | +15 Pow | 450 | −10% |
| 21 | Chompy Fang | starter | 450 | +6 Pow, +8 Pen | 460 | +2% |
| 22 | Ghost Pirate Cutlass | starter | 650 | +0.3 AS, +10% Crit | 660 | +2% |
| 3 | Berserker Gauntlets | mid | 900 | +0.6 AS, +10% Crit | 1020 | +13% |
| 4 | Vampire Fangs | mid | 950 | +15 Pow, +12% LS | 930 | −2% |
| 2 | Blade of Fire | mid | 1150 | +35 Pow | 1050 | −9% |
| 23 | Arkeyan Crossbow | mid | 1300 | +25 Pow, +15 Pen | 1275 | −2% |
| 5 | Storm Hammer | mid | 1400 | +25 Pow, +0.4 AS | 1230 | −12% |
| 6 | Golden Gatling | capstone | 1900 | +45 Pow, +20% Crit | 1950 | +3% |
| 8 | Bloodthirster | capstone | 2200 | +40 Pow, +20% LS | 2000 | −9% |
| 7 | Executioner | capstone | 2400 | +30 Pow, +0.3 AS, +25% Crit | 2010 | −16% |
| 24 | Traptanium Shard | capstone | 2500 | +45 Pow, +25 Pen, +10% CDR | 2575 | +3% |

**DEFENSE**

| ID | Name | Tier | Cost | Stats | Budget | Δ |
|---|---|---|---|---|---|---|
| 25 | Mabu Hide Cloak | starter | 450 | +40 HP, +15 Prot | 475 | +6% |
| 9 | Skystone Shield | starter | 600 | +100 HP, +12 Prot | 550 | −8% |
| 26 | Healing Elixir | starter | 650 | +150 HP, +5 HP/s, +2 MP/s | 655 | +1% |
| 10 | Heart of Skylands | mid | 950 | +250 HP, +6 HP/s | 865 | −9% |
| 27 | Sky-Iron Shield | mid | 1050 | +100 HP, +20 Prot, +10% CDR | 1100 | +5% |
| 11 | Guardian Plate | mid | 1200 | +150 HP, +30 Prot | 1125 | −6% |
| 28 | Giant's Girdle | mid | 1250 | +250 HP, +15 Pow, +4 HP/s | 1235 | −1% |
| 29 | Lifespring Pendant | mid | 1300 | +200 HP, +25 Prot, +4 MP/s | 1285 | −1% |
| 12 | Portal Master's Armor | capstone | 1700 | +100 HP, +45 Prot, +8 HP/s | 1695 | 0% |
| 14 | Aegis of Life | capstone | 2000 | +150 HP, +25 Prot, +15% LS | 1600 | −20% |
| 30 | Core of Light Bulwark | capstone | 2200 | +300 HP, +40 Prot, +15% CDR | 2275 | +3% |
| 13 | Titan's Bulwark | capstone | 2300 | +400 HP, +20 Prot, +10 HP/s | 1900 | −17% |

**UTILITY**

| ID | Name | Tier | Cost | Stats | Budget | Δ |
|---|---|---|---|---|---|---|
| 15 | Winged Boots | starter | 500 | +60 MS | 360 | −28% |
| 16 | Crystal Wand | starter | 600 | +60 Mana, +4 MP/s | 250 | −58% |
| 31 | Time Twist Hourglass | starter | 600 | +40 Mana, +5 MP/s, +10% CDR | 610 | +2% |
| 17 | Swiftstrike Charm | mid | 1100 | +0.3 AS, +80 MS | 840 | −24% |
| 32 | Hugo's Spellbook | mid | 1100 | +15 Pow, +60 Mana, +15% CDR | 1065 | −3% |
| 19 | Boots of Celerity | mid | 1300 | +100 HP, +110 MS | 910 | −30% |
| 33 | Hand of Fate | mid | 1300 | +15 Pow, +80 MS, +10% CDR | 1280 | −2% |
| 18 | Sage's Ring | mid | 1400 | +25 Pow, +80 Mana, +6 MP/s | 1110 | −21% |
| 20 | Arcane Battery | capstone | 1800 | +20 Pow, +150 Mana, +10 MP/s | 1225 | −32% |
| 35 | Dragon's Peak Wings | capstone | 2000 | +0.5 AS, +20% Crit, +120 MS | 1920 | −4% |
| 34 | Eon's Staff | capstone | 2200 | +40 Pow, +100 Mana, +5 MP/s, +20% CDR | 2250 | +2% |
| 36 | Sky-Chi Sigil | capstone | 2300 | +250 HP, +25 Prot, +60 MS, +20% CDR | 2310 | 0% |

Virtual "trees" (no real building; these pairs are what the recommended tab should suggest selling two starters into):
Offense `1 + 21 ≈ 23`, `1 + 22 ≈ 5`; Defense `25 + 9 ≈ 11`, `26 + 9 ≈ 29`; Utility `31 + 16 ≈ 32`, `15 + 31 ≈ 33`.

CDR sources and sums: 31 (10) · 27 (10) · 33 (10) · 24 (10) · 32 (15) · 30 (15) · 34 (20) · 36 (20). Any two capstones hit the 40% cap. Pen sources: 21 (8) · 23 (15) · 24 (25) — max 48, enough to zero a Guardian Plate + Skystone Shield (42).

### 2.3 Ready-to-paste C++ (`SkylandersItemCatalog.cpp`, inside `InitializeItems()`)

Replace the `St` lambda:

```cpp
	// Compact stat builder: (power, atkSpeed, crit, health, mana, hpRegen, mpRegen, prot, lifesteal, moveSpeed, cdr, pen)
	auto St = [](float Pow = 0, float AS = 0, float Crit = 0, float HP = 0, float Mana = 0,
		float HPR = 0, float MPR = 0, float Prot = 0, float LS = 0, float MS = 0,
		float CDR = 0, float Pen = 0) -> FSkylandersItemStats
	{
		FSkylandersItemStats S;
		S.Power = Pow; S.AttackSpeed = AS; S.CritChance = Crit; S.MaxHealth = HP; S.MaxMana = Mana;
		S.HealthRegen = HPR; S.ManaRegen = MPR; S.Protections = Prot; S.Lifesteal = LS; S.MovementSpeed = MS;
		S.CooldownReduction = CDR; S.Penetration = Pen;
		return S;
	};
```

Append after item 20 (IDs 1–20 stay exactly as they are):

```cpp
	// ===== OFFENSE additions (21-24) =====
	AllItems.Add(MakeItem(21, TEXT("Chompy Fang"),          TEXT("+6 Power, +8 Penetration"),                      450,  OFF, St(6,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 8)));
	AllItems.Add(MakeItem(22, TEXT("Ghost Pirate Cutlass"), TEXT("+0.3 Attack Speed, +10% Crit"),                  650,  OFF, St(0,  0.3f, 0.10f)));
	AllItems.Add(MakeItem(23, TEXT("Arkeyan Crossbow"),     TEXT("+25 Power, +15 Penetration"),                    1300, OFF, St(25, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 15)));
	AllItems.Add(MakeItem(24, TEXT("Traptanium Shard"),     TEXT("+45 Power, +25 Penetration, +10% CDR"),          2500, OFF, St(45, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0.10f, 25)));

	// ===== DEFENSE additions (25-30) =====
	AllItems.Add(MakeItem(25, TEXT("Mabu Hide Cloak"),      TEXT("+40 Health, +15 Protections"),                   450,  DEF, St(0,  0, 0, 40,  0, 0, 0, 15)));
	AllItems.Add(MakeItem(26, TEXT("Healing Elixir"),       TEXT("+150 Health, +5 HP/s, +2 MP/s"),                 650,  DEF, St(0,  0, 0, 150, 0, 5, 2)));
	AllItems.Add(MakeItem(27, TEXT("Sky-Iron Shield"),      TEXT("+100 Health, +20 Protections, +10% CDR"),        1050, DEF, St(0,  0, 0, 100, 0, 0, 0, 20, 0, 0, 0.10f)));
	AllItems.Add(MakeItem(28, TEXT("Giant's Girdle"),       TEXT("+250 Health, +15 Power, +4 HP/s"),               1250, DEF, St(15, 0, 0, 250, 0, 4)));
	AllItems.Add(MakeItem(29, TEXT("Lifespring Pendant"),   TEXT("+200 Health, +25 Protections, +4 MP/s"),         1300, DEF, St(0,  0, 0, 200, 0, 0, 4, 25)));
	AllItems.Add(MakeItem(30, TEXT("Core of Light Bulwark"),TEXT("+300 Health, +40 Protections, +15% CDR"),        2200, DEF, St(0,  0, 0, 300, 0, 0, 0, 40, 0, 0, 0.15f)));

	// ===== UTILITY additions (31-36) =====
	AllItems.Add(MakeItem(31, TEXT("Time Twist Hourglass"), TEXT("+40 Mana, +5 MP/s, +10% CDR"),                   600,  UTL, St(0,  0, 0, 0, 40,  0, 5, 0, 0, 0, 0.10f)));
	AllItems.Add(MakeItem(32, TEXT("Hugo's Spellbook"),     TEXT("+15 Power, +60 Mana, +15% CDR"),                 1100, UTL, St(15, 0, 0, 0, 60,  0, 0, 0, 0, 0, 0.15f)));
	AllItems.Add(MakeItem(33, TEXT("Hand of Fate"),         TEXT("+15 Power, +80 Movement Speed, +10% CDR"),       1300, UTL, St(15, 0, 0, 0, 0,   0, 0, 0, 0, 80, 0.10f)));
	AllItems.Add(MakeItem(34, TEXT("Eon's Staff"),          TEXT("+40 Power, +100 Mana, +5 MP/s, +20% CDR"),       2200, UTL, St(40, 0, 0, 0, 100, 0, 5, 0, 0, 0, 0.20f)));
	AllItems.Add(MakeItem(35, TEXT("Dragon's Peak Wings"),  TEXT("+0.5 Attack Speed, +20% Crit, +120 Movement Speed"), 2000, UTL, St(0, 0.5f, 0.20f, 0, 0, 0, 0, 0, 0, 120)));
	AllItems.Add(MakeItem(36, TEXT("Sky-Chi Sigil"),        TEXT("+250 Health, +25 Protections, +60 Movement Speed, +20% CDR"), 2300, UTL, St(0, 0, 0, 250, 0, 0, 0, 25, 0, 60, 0.20f)));
```

---

## 3. AI purchase priority

### 3.1 Current behavior (and why the lists below assume a rule change)

`ASkylandersEnemyGod::TryPurchaseItems()` (`SkylandersEnemyGod.cpp` ~358) walks `GetItemPurchasePriority()` in order and buys the **first** affordable ID, one per tick, up to 2 copies per ID, 6 slots, only within 500 uu of base. Two problems:

1. The comments in `GetItemPurchasePriority()` (~347) describe a stale ID scheme ("10=Winged Boots", "6=Skystone Shield", "2=Elemental Blade"). The real list `{1,10,6,3,4,2,7,8,5,9}` buys Short Sword → Heart of Skylands → Golden Gatling → … It works by accident only.
2. "First affordable" + "2 copies" means a god that returns to base with 1500 gold buys the cheapest list entry twice before anything else, so builds end as 3 starters doubled.

### 3.2 Purchase rule (replace the loop body)

- **One copy per ID** unless the ID appears twice in the list (count occurrences in `Priority`, compare against owned count). Replace `if (OwnedCountOfThis >= 2) continue;` with that comparison.
- **Pick the most expensive affordable un-owned entry** (lists are ascending in cost). Iterate the whole list, remember the best candidate, buy it once after the loop. Keeps "one purchase per tick".
- Optional, later: when 6/6 and the candidate costs ≥ 2× the cheapest owned item, sell the cheapest (67%, `GetSellPrice()`) and buy. Not required for this pass.

### 3.3 Archetypes and assignment

Add to `SkylandersEnemyGod.h`:

```cpp
UENUM(BlueprintType)
enum class EGodBuildArchetype : uint8 { Bruiser, Assassin, Tank };

UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
EGodBuildArchetype BuildArchetype = EGodBuildArchetype::Bruiser;

static const TArray<int32>& GetItemPurchasePriority(EGodBuildArchetype Archetype); // was: ()
```

`TryPurchaseItems()` calls `GetItemPurchasePriority(BuildArchetype)`. Set the archetype in `SkylandersMapBuilder.cpp`'s `SpawnGod` lambda (~427): add a 5th parameter and set `God->BuildArchetype` before `FinishSpawning`. Calls (~443–449):

| God | Team | Archetype |
|---|---|---|
| Spyro | Friendly | Bruiser |
| Stealth Elf | Friendly | Assassin |
| Kaos | Enemy | Assassin |
| Chompy Mage | Enemy | Bruiser |
| Cynder | Enemy | Tank |

Any god spawned without an explicit archetype defaults to Bruiser.

### 3.4 Lists (ascending cost; final 6 fall out of the rule in §3.2)

```cpp
	// Bruiser: health + power.   500, 650, 950, 1200, 1250, 1900, 2000, 2200, 2300
	static TArray<int32> Bruiser  = { 1, 26, 4, 11, 28, 6, 14, 8, 13 };
	// Assassin: power + pen + crit.   450, 500, 650, 900, 1300, 1900, 2200, 2400, 2500
	static TArray<int32> Assassin = { 21, 1, 22, 3, 23, 6, 8, 7, 24 };
	// Tank: prot + health + CDR.   450, 600, 600, 1050, 1200, 1300, 1700, 2200, 2300, 2300
	static TArray<int32> Tank     = { 25, 9, 31, 27, 11, 29, 12, 30, 13, 36 };
```

Bruiser: Short Sword, Healing Elixir, Vampire Fangs, Guardian Plate, Giant's Girdle, Golden Gatling, Aegis of Life, Bloodthirster, Titan's Bulwark.
Assassin: Chompy Fang, Short Sword, Ghost Pirate Cutlass, Berserker Gauntlets, Arkeyan Crossbow, Golden Gatling, Bloodthirster, Executioner, Traptanium Shard.
Tank: Mabu Hide Cloak, Skystone Shield, Time Twist Hourglass, Sky-Iron Shield, Guardian Plate, Lifespring Pendant, Portal Master's Armor, Core of Light Bulwark, Titan's Bulwark, Sky-Chi Sigil (Tank CDR tops out at 45% → capped 40%, acceptable).

---

## 4. Recommended builds (shop "recommended" tab)

Buy order is left-to-right; "sell X" is the one swap needed to reach six items. Totals are the final six.

**Trigger Happy — ranged crit** (base FireRate 2.0/s, Power 20 +2/level)
Order: 22 → 15 → 6 → 23 → 7 → 35 → sell 22, buy 8.
Final: **15 Winged Boots · 6 Golden Gatling · 23 Arkeyan Crossbow · 7 Executioner · 35 Dragon's Peak Wings · 8 Bloodthirster** — 10,300 g.
Totals: +140 Pow, +0.8 AS (→ 2.8 shots/s), 65% Crit, 15 Pen, 20% LS, +180 MS.

**Hex — mage CDR / pen** (base Power 25 +2.6/level; Skull Storm = 10× Power)
Order: 31 → 21 → 23 → 34 → 24 → 2 → sell 21, buy 20.
Final: **31 Time Twist Hourglass · 34 Eon's Staff · 24 Traptanium Shard · 23 Arkeyan Crossbow · 2 Blade of Fire · 20 Arcane Battery** — 9,550 g.
Totals: +165 Pow, 40 Pen, 40% CDR (exactly at cap), +290 Mana, +20 MP/s. Skull Storm 75 s → 45 s at rank 5.

**Tree Rex — tank** (base HP 165 +34/level; Healing Grove heals 14–24% of effective max HP, Titan's Wrath heals 6% max HP per hit, both scale with item HP)
Order: 25 → 26 → 29 → 12 → 30 → 13 → sell 25, buy 36.
Final: **26 Healing Elixir · 29 Lifespring Pendant · 12 Portal Master's Armor · 30 Core of Light Bulwark · 13 Titan's Bulwark · 36 Sky-Chi Sigil** — 10,450 g.
Totals: +1400 HP, 155 Prot (61% reduction before pen), 35% CDR, +23 HP/s, +6 MP/s, +60 MS.

---

## 5. Balance note — gold curve

Sources (player), from code:
- Passive: `PassiveGoldPerSecond = 2.0` (`SkylandersCharacter.cpp` ~338) → **1200 g by 10:00**.
- Lane: one lane, 6 minions/wave (3 melee @ 8 g, 3 ranged @ 12 g; `SkylandersMinion.cpp` 79/136), +1 g per minion per wave after the first (`SkylandersMinionSpawner.cpp` ~123), first wave at 10 s then every 30 s → 20 waves by 10:00, **2340 g if every minion is last-hit**; gold only goes to the killer, and two allied gods + friendly minions compete, so plan on 40–60% → **950–1400 g**.
- God kills: `CoinReward = 150 + 15*(Level-1)`, gods level every 60 s (`SkylandersEnemyGod.cpp` ~112/248) → 225 g at 5:00, 300 g at 10:00; 2–4 kills → **450–1100 g**.
- Objectives / ability-hit coins: camps 30 (Bad Juju 40, Golden Queen 150), tower 50, +1 g per ability hit (Hex orb, TR slam) or +5 (Hex nova, TH minigun) → **150–400 g**.

**Expected player total by 10:00: ~2,750–4,100, plan around 3,300 (≈330 g/min).** Resulting curve with the catalog above: first starter ~1:30, second ~3:00, first mid ~5:00, **first capstone 8:00–10:00**, full six items (~10 k) around minute 28–30. That meets the "capstone at 8–12, not 3" target without touching income. If matches are meant to end near 20 min, raise the player's `PassiveGoldPerSecond` to 3.0 (+600 g by 10:00, full build ~minute 22); do not raise minion bounties, that would reward lane-only play.

Enemy-god income is a separate problem: `TickPassiveGold` uses `2 + 0.5*Level` g/s with Level unbounded (`SkylandersEnemyGod.cpp` ~293). That is 2,850 g passive by 10:00 (parity with the player) but 8,700 g by 20:00 (≈2× the player) plus their own last hits and kills, so gods reach six items by ~minute 18 while the player sits on four. Recommend `float GoldRate = PassiveGoldPerSecond + 0.5f * FMath::Min(Level, 6);` (caps at 5 g/s → ~5,400 g passive by 20:00).

Two stat-side caveats for the first playtest:
- Both the player (`GetEffectiveProtections()` = items + Barkskin buff) and gods (`Protections = 0` in the constructor) have **zero base protections**, so Penetration is dead against an un-itemized target. That is why every pen item also carries Power. If pen feels useless in the first 5 minutes, give heroes 10 base prot + 1/level rather than buffing the items.
- The 40% CDR cap interacts with the rank scale (`1.2 − 0.04 × rank`): a rank-1 ability with 40% CDR sits at 69.6% of base, rank 5 at 60%. Ultimates (60–80 s) at 40% become 36–48 s; if that proves too strong, cap CDR on ability 4 at 0.25 at the four `Ability4_RemainingCooldown` sites only.

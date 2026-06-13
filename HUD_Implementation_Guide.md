# Skylanders Conquest - MOBA HUD Implementation Guide

## Overview
This guide will help you create a SMITE-style HUD for your Skylanders Conquest game. The HUD will include health, mana, abilities, coins, level, and targeting systems.

---

## Part 1: Main Player HUD Widget

### Step 1: Create the Main HUD Widget

1. In Content Browser, navigate to `Content/UserInterface/`
2. Right-click → User Interface → Widget Blueprint
3. Name it `WBP_MainHUD`

### Step 2: Design the HUD Layout

Open `WBP_MainHUD` and add the following elements:

#### A. Bottom Center - Player Stats Bar
Create a container at the bottom center of the screen (similar to SMITE's layout):

**Canvas Panel Structure:**
```
Canvas Panel
└── Bottom Stats Container (Horizontal Box)
    ├── Character Portrait (Image)
    ├── Stats Vertical Box
    │   ├── Health Bar (Progress Bar)
    │   ├── Mana Bar (Progress Bar)
    │   └── Level Badge (Text Block)
    └── Abilities Container (Horizontal Box)
        ├── Ability 1 Slot
        ├── Ability 2 Slot
        ├── Ability 3 Slot
        └── Ability 4 (Ultimate) Slot
```

**Layout Details:**

1. **Bottom Stats Container** (Horizontal Box)
   - Anchors: Bottom Center (X=0.5, Y=1.0)
   - Position: X=0, Y=-100
   - Size: 800x100
   - Alignment: Center

2. **Character Portrait** (Image)
   - Size: 80x80
   - Padding: Left=10, Right=10
   - Use Trigger Happy portrait texture

3. **Health Bar** (Progress Bar)
   - Size: X=300, Y=25
   - Fill Color: Red (#FF0000)
   - Background Color: Dark Red (#440000)
   - Percent: 1.0 (100%)

4. **Mana Bar** (Progress Bar)
   - Size: X=300, Y=20
   - Fill Color: Blue (#0080FF)
   - Background Color: Dark Blue (#000044)
   - Percent: 1.0 (100%)

5. **Level Badge** (Text Block)
   - Font Size: 24
   - Text: "LVL 1"
   - Color: Gold (#FFD700)
   - Padding: Top=5

#### B. Top Right - Resources Display

**Create a Vertical Box in Top Right:**
```
Vertical Box (Top Right)
├── Coins Display (Horizontal Box)
│   ├── Coin Icon (Image)
│   └── Coin Count Text (Text Block)
└── Score Display (Horizontal Box)
    ├── Score Icon (Image)
    └── Score Text (Text Block)
```

**Layout Details:**

1. **Coins Display Container**
   - Anchors: Top Right (X=1.0, Y=0.0)
   - Position: X=-200, Y=20
   - Size: 180x40

2. **Coin Icon**
   - Size: 32x32
   - Image: Use TriggerHappy_CoinBig texture

3. **Coin Count Text**
   - Font Size: 28
   - Text: "0"
   - Color: Gold (#FFD700)
   - Padding: Left=10

#### C. Ability Slots (Bottom Center, Right of Stats)

**Each Ability Slot Structure:**
```
Overlay
├── Ability Background (Image)
├── Ability Icon (Image)
├── Cooldown Overlay (Image)
├── Cooldown Text (Text Block)
└── Hotkey Text (Text Block)
```

**Details for Each Ability Slot:**

1. **Ability 1 (Left Mouse/Q)**
   - Size: 60x60
   - Background: Dark gray rounded square
   - Icon: Placeholder (will add later)
   - Hotkey Text: "LMB" or "Q"
   - Position: Bottom left of ability container

2. **Ability 2 (E)**
   - Size: 60x60
   - Hotkey Text: "E"

3. **Ability 3 (R)**
   - Size: 60x60
   - Hotkey Text: "R"

4. **Ability 4 - Ultimate (F)**
   - Size: 70x70 (larger for ultimate)
   - Border Color: Gold to indicate ultimate
   - Hotkey Text: "F"

**Cooldown Overlay:**
- Image with tint: Black (#000000)
- Opacity: 0.7
- Visibility: Initially Hidden
- Render Opacity: Bound to cooldown percentage

---

## Part 2: Variable Setup in WBP_MainHUD

### Variables to Create

In the Graph view of `WBP_MainHUD`, create these variables:

**Player Stats:**
1. `CurrentHealth` (Float) - Default: 100.0
2. `MaxHealth` (Float) - Default: 100.0
3. `CurrentMana` (Float) - Default: 100.0
4. `MaxMana` (Float) - Default: 100.0
5. `PlayerLevel` (Integer) - Default: 1
6. `CoinCount` (Integer) - Default: 0

**Ability Cooldowns:**
7. `Ability1_Cooldown` (Float) - Default: 0.0
8. `Ability2_Cooldown` (Float) - Default: 0.0
9. `Ability3_Cooldown` (Float) - Default: 0.0
10. `Ability4_Cooldown` (Float) - Default: 0.0

**References:**
11. `PlayerCharacterRef` (BP_ThirdPersonCharacter reference)
12. `bIsVisible` (Boolean) - Default: true

---

## Part 3: Blueprint Logic

### Event Graph Functions

#### Function 1: Update Health Bar

1. Create Function: `UpdateHealthDisplay`
2. Add Input: `NewHealth` (Float), `MaxHealth` (Float)
3. Logic:
   ```
   Set CurrentHealth = NewHealth
   Set MaxHealth = MaxHealth

   Calculate HealthPercent = CurrentHealth / MaxHealth
   Set HealthBar Percent = HealthPercent

   Update Health Text = CurrentHealth + " / " + MaxHealth
   ```

#### Function 2: Update Mana Bar

1. Create Function: `UpdateManaDisplay`
2. Add Input: `NewMana` (Float), `MaxMana` (Float)
3. Logic:
   ```
   Set CurrentMana = NewMana
   Set MaxMana = MaxMana

   Calculate ManaPercent = CurrentMana / MaxMana
   Set ManaBar Percent = ManaPercent

   Update Mana Text = CurrentMana + " / " + MaxMana
   ```

#### Function 3: Update Coin Count

1. Create Function: `AddCoins`
2. Add Input: `Amount` (Integer)
3. Logic:
   ```
   Set CoinCount = CoinCount + Amount
   Set CoinCountText Text = ToString(CoinCount)

   // Optional: Play coin collect animation
   Play Animation: CoinCollectPulse
   ```

#### Function 4: Update Level

1. Create Function: `SetPlayerLevel`
2. Add Input: `NewLevel` (Integer)
3. Logic:
   ```
   Set PlayerLevel = NewLevel
   Set LevelText = "LVL " + ToString(NewLevel)

   // Optional: Play level up animation
   ```

#### Function 5: Update Ability Cooldown

1. Create Function: `UpdateAbilityCooldown`
2. Add Inputs: `AbilityIndex` (Integer), `RemainingCooldown` (Float), `MaxCooldown` (Float)
3. Logic:
   ```
   Calculate CooldownPercent = RemainingCooldown / MaxCooldown

   Switch on AbilityIndex:
     Case 0: Update Ability1 overlay and text
     Case 1: Update Ability2 overlay and text
     Case 2: Update Ability3 overlay and text
     Case 3: Update Ability4 overlay and text

   If RemainingCooldown > 0:
     Show cooldown overlay
     Set cooldown text = Round(RemainingCooldown)
   Else:
     Hide cooldown overlay
     Clear cooldown text
   ```

### Event Tick Logic

In the `Event Tick` node:

```
1. Get Player Character Reference (cast to BP_ThirdPersonCharacter)
2. If valid:
   - Get current health/mana from character
   - Call UpdateHealthDisplay
   - Call UpdateManaDisplay

3. Update ability cooldowns (loop through all abilities)
```

---

## Part 4: Creating Target Health Bar Widget

### Step 1: Create Widget

1. Navigate to `Content/UserInterface/`
2. Create new Widget Blueprint: `WBP_TargetHealthBar`

### Step 2: Design

**Canvas Panel Structure:**
```
Canvas Panel (Screen Center Top)
└── Target Container (Vertical Box)
    ├── Target Name (Text Block)
    ├── Health Bar (Progress Bar)
    └── Health Text (Text Block)
```

**Layout:**
- Anchors: Top Center (X=0.5, Y=0.0)
- Position: X=0, Y=150
- Size: 400x80

**Target Name:**
- Font Size: 24
- Color: White
- Alignment: Center

**Health Bar:**
- Size: X=350, Y=20
- Fill Color: Red
- Background: Dark Red

### Step 3: Variables

1. `TargetName` (Text)
2. `TargetHealth` (Float)
3. `TargetMaxHealth` (Float)
4. `bIsTargeting` (Boolean)

### Step 4: Logic

**Function: SetTargetInfo**
```
Inputs: Name (Text), CurrentHP (Float), MaxHP (Float)

Set TargetName Text = Name
Calculate HP Percent = CurrentHP / MaxHP
Set HealthBar Percent = HP Percent
Set Health Text = CurrentHP + " / " + MaxHP
Set Visibility = Visible
```

**Function: ClearTarget**
```
Set Visibility = Hidden
Set bIsTargeting = false
```

---

## Part 5: Connecting HUD to Character

### Modify BP_ThirdPersonCharacter

1. Open `Content/ThirdPerson/Blueprints/BP_ThirdPersonCharacter`

2. **Add Variables:**
   - `CurrentHealth` (Float) - Default: 100.0
   - `MaxHealth` (Float) - Default: 100.0
   - `CurrentMana` (Float) - Default: 100.0
   - `MaxMana` (Float) - Default: 100.0
   - `PlayerLevel` (Integer) - Default: 1
   - `Coins` (Integer) - Default: 0
   - `MainHUDRef` (WBP_MainHUD reference)
   - `TargetHUDRef` (WBP_TargetHealthBar reference)

3. **Event BeginPlay:**
   ```
   Event BeginPlay
   └── Create Widget (WBP_MainHUD)
       ├── Add to Viewport (Z-Order: 0)
       └── Store in MainHUDRef variable

   └── Create Widget (WBP_TargetHealthBar)
       ├── Add to Viewport (Z-Order: 1)
       ├── Store in TargetHUDRef variable
       └── Set Visibility: Hidden
   ```

4. **Create Function: TakeDamage**
   ```
   Input: DamageAmount (Float)

   CurrentHealth = CurrentHealth - DamageAmount
   Clamp CurrentHealth (0 to MaxHealth)

   Call MainHUDRef -> UpdateHealthDisplay(CurrentHealth, MaxHealth)

   If CurrentHealth <= 0:
     Call Death Function
   ```

5. **Create Function: UseMana**
   ```
   Input: ManaCost (Float)

   If CurrentMana >= ManaCost:
     CurrentMana = CurrentMana - ManaCost
     Call MainHUDRef -> UpdateManaDisplay(CurrentMana, MaxMana)
     Return True
   Else:
     Return False (not enough mana)
   ```

6. **Create Function: AddCoins**
   ```
   Input: Amount (Integer)

   Coins = Coins + Amount
   Call MainHUDRef -> AddCoins(Amount)
   ```

### Modify BP_TriggerHappy_Projectile

When a coin hits something, add coin collection:

1. Open `BP_TriggerHappy_Projectile`
2. Find the `On Hit` event
3. Add logic:
   ```
   On Component Hit
   └── Get Instigator (player character)
       └── Cast to BP_ThirdPersonCharacter
           └── Call AddCoins(1)
   ```

---

## Part 6: Ability System Setup

### Create Base Ability Structure

1. Create new folder: `Content/Abilities/`
2. Create struct: `S_AbilityData`

**Struct Variables:**
- `AbilityName` (Text)
- `AbilityIcon` (Texture2D)
- `Cooldown` (Float)
- `ManaCost` (Float)
- `HotkeyText` (Text)

### In BP_ThirdPersonCharacter

**Add Ability Variables:**
1. `Ability1_Data` (S_AbilityData)
2. `Ability2_Data` (S_AbilityData)
3. `Ability3_Data` (S_AbilityData)
4. `Ability4_Data` (S_AbilityData)

**Add Cooldown Tracking:**
5. `Ability1_RemainingCooldown` (Float)
6. `Ability2_RemainingCooldown` (Float)
7. `Ability3_RemainingCooldown` (Float)
8. `Ability4_RemainingCooldown` (Float)

**Event Tick:**
```
For each ability:
  If RemainingCooldown > 0:
    RemainingCooldown -= DeltaTime
    Clamp to 0
    Update HUD cooldown display
```

---

## Part 7: Optional Enhancements

### Damage Numbers

Create floating damage text:

1. Create Widget: `WBP_DamageNumber`
2. Single Text Block with large font
3. Add Animation: Float up and fade out
4. Spawn at damage location
5. Destroy after animation complete

### Minimap (Advanced)

1. Create Widget: `WBP_Minimap`
2. Add Scene Capture Component to level
3. Render to texture
4. Display in corner of HUD

### Kill Feed

1. Create Widget: `WBP_KillFeed`
2. Vertical box of kill notifications
3. Auto-scroll and fade out old entries

---

## Quick Implementation Checklist

- [ ] Create WBP_MainHUD widget
- [ ] Design health/mana bars
- [ ] Add coin counter
- [ ] Create 4 ability slots
- [ ] Add level display
- [ ] Create WBP_TargetHealthBar widget
- [ ] Add health/mana variables to BP_ThirdPersonCharacter
- [ ] Connect HUD to character in BeginPlay
- [ ] Implement TakeDamage function
- [ ] Implement UseMana function
- [ ] Implement AddCoins function
- [ ] Add coin collection to projectile hit
- [ ] Create ability cooldown system
- [ ] Test all HUD elements in-game

---

## Testing the HUD

1. **Test Health:**
   - In BP_ThirdPersonCharacter Event BeginPlay, add: `Set Timer by Event` → Every 2 seconds → `TakeDamage(10)`
   - Watch health bar decrease

2. **Test Mana:**
   - On jump, call `UseMana(20)`
   - Watch mana bar decrease

3. **Test Coins:**
   - Shoot coins at walls
   - On hit, should add to counter

4. **Test Level:**
   - Press a key (like "L") to call `SetPlayerLevel(PlayerLevel + 1)`

---

## Color Scheme Recommendations

**Skylanders Theme:**
- Primary: Purple/Violet (#8B00FF)
- Secondary: Orange (#FF6600)
- Health: Red to Green gradient
- Mana: Bright Blue (#00BFFF)
- Gold/Coins: Bright Gold (#FFD700)
- Background: Dark Purple/Black (#1A0033)

---

## Next Steps After HUD

Once HUD is complete, you'll want to:
1. Create actual ability effects
2. Build the 3-lane MOBA map
3. Add enemy/AI characters
4. Implement tower/base structures
5. Add minion waves
6. Create win/loss conditions

Good luck with your implementation!

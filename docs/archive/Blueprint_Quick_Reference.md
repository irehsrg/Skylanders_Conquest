# Blueprint Quick Reference for HUD Implementation

## Common Blueprint Nodes You'll Need

### Widget Creation and Display

#### Create Widget
- **Node**: `Create Widget`
- **Class**: Select your widget class (e.g., WBP_MainHUD)
- **Owning Player**: `Get Player Controller`
- **Returns**: Widget reference

#### Add to Viewport
- **Node**: `Add to Viewport`
- **Target**: Your widget reference
- **Z Order**: 0 (lower numbers = behind)

#### Remove from Parent
- **Node**: `Remove from Parent`
- **Target**: Widget reference to remove

### Binding Widget Elements

#### Get Widget by Name
In the Widget Blueprint Graph:
- Drag from widget element in hierarchy (left panel)
- This creates a reference to that element

#### Set Percent (for Progress Bars)
- **Node**: `Set Percent`
- **Target**: Progress Bar reference
- **In Percent**: Float value 0.0 to 1.0

#### Set Text (for Text Blocks)
- **Node**: `Set Text`
- **Target**: Text Block reference
- **In Text**: Text value

### Math Operations

#### Divide (Float ÷ Float)
- **Node**: `Float / Float` or just type "/"
- **Use**: Calculate percentages (CurrentHP / MaxHP)

#### Clamp
- **Node**: `Clamp (float)`
- **Value**: Input value
- **Min**: Minimum value
- **Max**: Maximum value
- **Returns**: Clamped value

#### Round
- **Node**: `Round`
- **A**: Float input
- **Returns**: Rounded integer

### Type Conversions

#### Float to Text
- **Node**: `ToText (float)`
- **Value**: Float input
- **Returns**: Text output

#### Integer to Text
- **Node**: `ToText (integer)`
- **Value**: Integer input
- **Returns**: Text output

#### Append Text
- **Node**: `Append` or `Format Text`
- **Use**: Combine text strings
- **Example**: "HP: " + ToString(Health)

### Player/Character References

#### Get Player Character
- **Node**: `Get Player Character`
- **Player Index**: 0 (first player)
- **Returns**: Pawn reference

#### Cast To (Character)
- **Node**: `Cast to BP_ThirdPersonCharacter`
- **Object**: Result from Get Player Character
- **Returns**: Your character type

#### Get Player Controller
- **Node**: `Get Player Controller`
- **Player Index**: 0
- **Returns**: Player Controller reference

### Visibility Control

#### Set Visibility
- **Node**: `Set Visibility`
- **Target**: Widget element
- **In Visibility**:
  - `Visible`
  - `Hidden`
  - `Collapsed`
  - `Hit Test Invisible`

### Timers

#### Set Timer by Event
- **Node**: `Set Timer by Event`
- **Event**: Custom event to call
- **Time**: Delay in seconds
- **Looping**: True/False

#### Set Timer by Function Name
- **Node**: `Set Timer by Function Name`
- **Function Name**: Name of function to call
- **Time**: Delay in seconds
- **Looping**: True/False

#### Clear Timer
- **Node**: `Clear Timer by Handle`
- **Handle**: Timer handle to clear

### Animation

#### Play Animation
- **Node**: `Play Animation`
- **In Animation**: Select animation from dropdown
- **Start at Time**: 0.0
- **Num Loops**: 1
- **Play Mode**: Forward
- **Playback Speed**: 1.0

### Color/Material

#### Set Brush Color
- **Node**: `Set Brush Tint Color`
- **Target**: Image widget
- **Color**: Linear Color value

#### Make Literal Color
- **Node**: `Make Color`
- **R, G, B, A**: 0.0 to 1.0 values

### Common Patterns

---

## Pattern 1: Update Health Bar

```
Function: UpdateHealthDisplay
Inputs: NewHealth (Float), NewMaxHealth (Float)

1. [Set] CurrentHealth = NewHealth
2. [Set] MaxHealth = NewMaxHealth
3. [Divide] HealthPercent = CurrentHealth / MaxHealth
4. [Set Percent] HealthBar->SetPercent(HealthPercent)
5. [Append] HealthText = ToString(CurrentHealth) + " / " + ToString(MaxHealth)
6. [Set Text] HealthTextBlock->SetText(HealthText)
```

---

## Pattern 2: Event Tick Update

```
Event Tick
├─ [Get Player Character] → Cast to BP_ThirdPersonCharacter
│  ├─ [Success] →
│  │  ├─ Get CurrentHealth
│  │  ├─ Get MaxHealth
│  │  └─ Call UpdateHealthDisplay(CurrentHealth, MaxHealth)
│  └─ [Failed] → Do Nothing
```

---

## Pattern 3: Create and Show HUD

```
Event BeginPlay (in BP_ThirdPersonCharacter)
├─ [Create Widget] Class: WBP_MainHUD
│  └─ [Add to Viewport] Z-Order: 0
│     └─ [Set] MainHUDRef = Widget Reference
```

---

## Pattern 4: Take Damage

```
Function: TakeDamage
Input: Damage (Float)

1. [Subtract] CurrentHealth = CurrentHealth - Damage
2. [Clamp] CurrentHealth (Min: 0, Max: MaxHealth)
3. [Is Valid?] Check if MainHUDRef exists
   ├─ [True] → Call MainHUDRef->UpdateHealthDisplay()
   └─ [False] → Do nothing
4. [Branch] If CurrentHealth <= 0
   ├─ [True] → Call DeathFunction()
   └─ [False] → Continue
```

---

## Pattern 5: Ability Cooldown Timer

```
Function: UseAbility1

1. [Branch] Check if Ability1_RemainingCooldown <= 0
   ├─ [True] → Ability is ready
   │  ├─ [Branch] Check if CurrentMana >= Ability1_ManaCost
   │  │  ├─ [True] → Can use ability
   │  │  │  ├─ Perform ability effect
   │  │  │  ├─ [Set] Ability1_RemainingCooldown = Ability1_MaxCooldown
   │  │  │  └─ [Subtract] CurrentMana -= Ability1_ManaCost
   │  │  └─ [False] → Print "Not enough mana"
   │  └─ Return
   └─ [False] → Print "Ability on cooldown"

In Event Tick:
├─ [Branch] If Ability1_RemainingCooldown > 0
│  ├─ [Subtract] Ability1_RemainingCooldown -= DeltaTime
│  ├─ [Clamp] Ability1_RemainingCooldown (Min: 0)
│  ├─ [Divide] CooldownPercent = Ability1_RemainingCooldown / Ability1_MaxCooldown
│  └─ Update HUD ability slot cooldown display
```

---

## Pattern 6: Add Coins with Animation

```
Function: AddCoins
Input: Amount (Integer)

1. [Add] Coins = Coins + Amount
2. [ToText] CoinText = ToString(Coins)
3. [Set Text] CoinCountTextBlock->SetText(CoinText)
4. [Play Animation] CoinPulseAnimation
5. [Optional] Play Sound: CoinCollectSound
```

---

## Pattern 7: Show/Hide Target Health

```
Function: SetTargetInfo
Inputs: TargetActor (Actor), TargetName (Text)

1. [Cast To] Cast TargetActor to Enemy type
   ├─ [Success]
   │  ├─ Get TargetHealth from enemy
   │  ├─ Get TargetMaxHealth from enemy
   │  ├─ [Set Text] TargetNameText = TargetName
   │  ├─ [Divide] HP% = TargetHealth / TargetMaxHealth
   │  ├─ [Set Percent] TargetHealthBar->SetPercent(HP%)
   │  └─ [Set Visibility] TargetHUD = Visible
   └─ [Failed]
      └─ [Set Visibility] TargetHUD = Hidden

Function: ClearTarget

1. [Set Visibility] TargetHUD = Hidden
```

---

## Pattern 8: Lerp (Smooth Transitions)

For smooth health bar transitions instead of instant:

```
Instead of direct SetPercent:

1. Store "DesiredHealthPercent" as variable
2. Store "CurrentDisplayedPercent" as variable

In Event Tick:
├─ [FInterp To] CurrentDisplayedPercent = Lerp(
│     CurrentDisplayedPercent,
│     DesiredHealthPercent,
│     DeltaTime,
│     InterpSpeed: 5.0)
└─ [Set Percent] HealthBar->SetPercent(CurrentDisplayedPercent)
```

---

## Keyboard Input Setup

### In Project Settings → Input

Add these Action Mappings:
- `UseAbility1` → E key
- `UseAbility2` → R key
- `UseAbility3` → F key (Ultimate)
- `LevelUp` → L key (for testing)

### In Character Blueprint

```
InputAction UseAbility1 (E)
└─ Call UseAbility1 Function

InputAction UseAbility2 (R)
└─ Call UseAbility2 Function

InputAction UseAbility3 (F)
└─ Call UseAbility3 Function
```

---

## Color Values Reference

### Linear Color Values (0-1 range)

```
Red:         R:1.0, G:0.0, B:0.0, A:1.0
Green:       R:0.0, G:1.0, B:0.0, A:1.0
Blue:        R:0.0, G:0.0, B:1.0, A:1.0
Yellow:      R:1.0, G:1.0, B:0.0, A:1.0
Purple:      R:0.55, G:0.0, B:1.0, A:1.0
Orange:      R:1.0, G:0.4, B:0.0, A:1.0
Gold:        R:1.0, G:0.84, B:0.0, A:1.0
White:       R:1.0, G:1.0, B:1.0, A:1.0
Black:       R:0.0, G:0.0, B:0.0, A:1.0
Gray (Dark): R:0.15, G:0.15, B:0.15, A:1.0
```

---

## Testing Shortcuts

### Add These to Character Blueprint for Testing

```
Keyboard Event: H (for testing damage)
└─ Call TakeDamage(10)

Keyboard Event: J (for testing healing)
└─ CurrentHealth += 20
    └─ Clamp to MaxHealth

Keyboard Event: M (for testing mana)
└─ CurrentMana -= 25
    └─ Clamp to 0

Keyboard Event: K (for testing coins)
└─ Call AddCoins(10)

Keyboard Event: L (for testing level up)
└─ PlayerLevel += 1
    └─ Call MainHUDRef->SetPlayerLevel(PlayerLevel)
```

---

## Common Mistakes to Avoid

1. **Forgetting to check IsValid** before calling functions on widget references
2. **Not clamping values** - health can go negative, percents can exceed 1.0
3. **Dividing by zero** - always check MaxHealth > 0 before dividing
4. **Wrong Z-Order** - HUD should be high Z-order to appear on top
5. **Not setting anchors** - widgets will move on different resolutions
6. **Hardcoding values** - use variables for MaxHealth, etc.
7. **No null checks** - always validate references exist before using them

---

## Performance Tips

1. **Don't update every tick unless necessary**
   - Use event-driven updates when possible
   - Only update widgets when values actually change

2. **Cache widget references**
   - Store references to frequently accessed widgets
   - Don't use "Get All Widgets of Class" in Tick

3. **Use Event Dispatchers**
   - Character can broadcast when health changes
   - HUD listens and updates only when needed

4. **Limit text updates**
   - Text rendering is expensive
   - Only update when value changes, not every frame

---

## Debugging Tips

### Print String Nodes
Add these to debug your logic:

```
[Print String] "Current Health: " + ToString(CurrentHealth)
Duration: 2.0
Text Color: Yellow
```

### Breakpoints
- Right-click any node → Add Breakpoint
- Game will pause when that node executes
- Inspect variable values in debugger

### Watch Variables
- Right-click variable → Watch this value
- See live updates in debug window

---

## Next Steps Checklist

1. ✓ Read this guide
2. ☐ Create WBP_MainHUD widget
3. ☐ Add health/mana progress bars
4. ☐ Create UpdateHealthDisplay function
5. ☐ Add variables to BP_ThirdPersonCharacter
6. ☐ Connect HUD in BeginPlay
7. ☐ Test with keyboard shortcuts (H for damage, J for heal)
8. ☐ Add ability slots
9. ☐ Implement cooldown system
10. ☐ Add coin counter
11. ☐ Create target health bar
12. ☐ Polish with animations

---

## Useful Resources

- **UMG Documentation**: Search "UMG UI Designer Quick Start" in Unreal Docs
- **Progress Bar**: Search "Progress Bar UMG" in Unreal Docs
- **Text Block**: Search "Text Block UMG" in Unreal Docs
- **Widget Blueprint**: Search "Widget Blueprint" in Unreal Docs

---

Good luck with your HUD implementation! Start with the basics (health bar) and build up from there.

# Skylanders Conquest - Character Designs

## Character Overview

Three proof-of-concept characters with unique playstyles:
1. **Trigger Happy (TH)** - Ranged DPS with gold theme
2. **Tree Rex (TR)** - Tank/Support with nature theme
3. **Hex (HX)** - Mage with skull/bone theme

---

## 1. TRIGGER HAPPY (TH)

**Role:** Ranged DPS / Carry
**Theme:** Gold, guns, coins

### Auto Attack
**Golden Guns** - Standard ranged projectile attack

### Ability 1: Golden Super Charge
- **Type:** Line skillshot
- **Effect:** Piercing damage in a line
- **Purpose:** Minion clear and poke damage
- **Behavior:** Passes through minions, hits all enemies in line

### Ability 2: Pot O' Gold
- **Type:** Small AoE damage
- **Effect:** Area damage around target location
- **Purpose:** Burst damage, wave clear

### Ability 3: Golden Machine Gun
- **Type:** Stance change / Duration ability
- **Effect:**
  - Makes TH stationary (cannot move)
  - Transforms attack into rapid-fire minigun
  - Higher DPS during duration
  - Likely won't benefit from auto-attack items
- **Cooldown:** Longer than other basic abilities
- **Purpose:** High sustained damage when positioned well

### Ultimate: Golden Yamato Blast
- **Type:** Charged blast
- **Effect:**
  - Long charge-up time
  - Massive damage in close range
  - Very high single-target damage
- **Purpose:** Burst down single target, high-risk high-reward

### Passive
**Gold-based passive** (TBD exact mechanic)
- Possibly bonus gold on kills?
- Gold increases power?
- To be determined

---

## 2. TREE REX (TR)

**Role:** Tank / Support
**Theme:** Nature, wood, support

### Auto Attack
**Ground Slam** - Melee slam attack

### Ability 1: Photosynthesis Cannon
- **Type:** Triple projectile
- **Effect:** Fires 3 shots in spread pattern
- **Damage:** Slight damage per shot
- **Purpose:** Ranged poke for a melee character

### Ability 2: Sequoia Stampede
- **Type:** Steerable charge
- **Effect:**
  - Dash/charge forward
  - Can be steered during charge
  - Knocks up enemies hit
  - Modest damage
- **Purpose:** Engage, disengage, CC

### Ability 3: Titanic Elbow Drop
- **Type:** Point-blank melee stun
- **Effect:**
  - Very short range
  - Stunning attack
  - Close-range CC
- **Purpose:** Lockdown enemies in melee range

### Ultimate: Woodpecker Pal
- **Type:** Summon / AoE effect
- **Effect:** Small AoE around Tree Rex
- **Options:**
  - Healing version (heal allies nearby)
  - Damage version (damage enemies nearby)
- **Purpose:** Support ultimate for team fights

### Passive
**Support Passive**
- Heals nearby allies over time?
- Bonus stats when near allies?
- To be determined

---

## 3. HEX (HX)

**Role:** Mage / Zone Control
**Theme:** Skulls, bones, death magic

### Auto Attack
**Phantom Orb**
- **Important:** NOT homing (unlike original games)
- Standard projectile skillshot

### Ability 1: Wall of Bones
- **Type:** Zoning wall
- **Effect:**
  - Creates a wall/barrier of bones
  - Deals tick damage on touch
  - Blocks movement/vision?
- **Purpose:** Zone control, area denial

### Ability 2: Storm of Skulls
- **Type:** Slow-moving AoE
- **Effect:**
  - Raining skulls in area
  - Slides/moves slowly across ground
  - Persistent damage zone
- **Purpose:** Area control, zoning

### Ability 3: Skull Shield
- **Type:** Defensive/Offensive buff
- **Effect:**
  - Spinning skulls orbit Hex
  - Provides defense OR offense boost
  - Skulls deal damage on contact
- **Purpose:** Self-peel, close-range damage

### Ultimate: TBD Skull Ability
**Option A: Massive Skull**
- Single large skull projectile
- High single-target damage
- Very small radius

**Option B: Curse**
- Apply curse debuff to enemy
- Reduces max HP temporarily
- Duration-based effect

### Passive
**Death Power Passive**
- Bonus power on minion death
- Bonus power on enemy kills
- Stacks reset on death
- Encourages aggressive play

---

## Implementation Notes

### Ability Slot Mapping
- **Fire (LMB):** Auto Attack (projectile or melee)
- **Ability1 (Q/E):** First ability
- **Ability2 (R/E):** Second ability
- **Ability3 (F/E):** Third ability
- **Ability4 (Ultimate):** Ultimate ability

### Default Cooldowns (can be adjusted per character)
- Ability1: ~5 seconds
- Ability2: ~10 seconds
- Ability3: ~15 seconds
- Ability4: ~90 seconds (ultimate)

### Mana Costs (can be adjusted per character)
- Ability1: 20 mana
- Ability2: 40 mana
- Ability3: 60 mana
- Ability4: 100 mana

### Character Differentiation

**Trigger Happy:**
- High mobility ranged DPS
- Risk/reward with stationary minigun
- High burst ultimate

**Tree Rex:**
- Low mobility tank
- Support-oriented
- Team fight focused
- CC and knockups

**Hex:**
- Medium range mage
- Zone control specialist
- Passive scaling mechanics
- Area denial

---

## Next Steps for Implementation

1. ✅ 4 ability slots coded
2. ⏳ Create base character class for each
3. ⏳ Implement auto-attack variants:
   - Projectile for TH
   - Melee for TR
   - Projectile for HX
4. ⏳ Implement ability effects per character
5. ⏳ Balance stats and cooldowns
6. ⏳ Add VFX/SFX
7. ⏳ Test in MOBA environment

---

## Code Structure

Each character will inherit from `ASkylandersCharacter`:
- `ATriggerHappyCharacter : public ASkylandersCharacter`
- `ATreeRexCharacter : public ASkylandersCharacter`
- `AHexCharacter : public ASkylandersCharacter`

Override ability functions to implement character-specific effects:
```cpp
void ATriggerHappyCharacter::UseAbility1()
{
    // Golden Super Charge implementation
    // Line skillshot with piercing
}
```

---

Let me know when you want to start implementing specific abilities!

# Projectile Not Detecting Hits - Debug Guide

## The Problem
Your "Projectile Hit!" print string isn't showing, which means the Event Hit (or OnComponentHit) is never firing. This is 99% a collision settings issue.

---

## Step 1: Check Projectile Components

1. **Open BP_TriggerHappy_Projectile**
2. **Go to Components panel (top-left)**
3. Find the root component - likely one of:
   - Sphere Collision
   - Capsule Collision
   - StaticMeshComponent
   - ProjectileMovementComponent

**What you're looking for:**
- A component with collision (usually Sphere or Capsule)
- This component needs proper collision settings

---

## Step 2: Check Collision Settings

### Select the Collision Component (Sphere/Capsule)

In the **Details panel** (right side), look for **Collision** section:

1. **Collision Presets**: Should be one of:
   - `OverlapAllDynamic`
   - `BlockAllDynamic`
   - `Custom...`

2. **If set to Custom, check:**
   - **Collision Enabled**: Should be `Collision Enabled (Query and Physics)`
   - **Object Type**: Should be `WorldDynamic` or `Projectile`
   - **Collision Responses**:
     - WorldStatic: **Block** (so it hits walls/floor)
     - WorldDynamic: **Block** (so it hits movable objects)
     - Pawn: **Block** (so it hits characters)

**CRITICAL:** If any of these are set to "Ignore", the projectile will pass through without triggering events!

---

## Step 3: Check Event Type

In the **Event Graph** of BP_TriggerHappy_Projectile:

### Which event are you using?

#### Option A: Event Hit (Actor level)
```
Event Hit
├─ My Comp (Component that was hit)
├─ Other (Actor that was hit)
├─ Other Comp (Other's component)
├─ Self Moved (Boolean)
├─ Hit Location (Vector)
├─ Hit Normal (Vector)
├─ Normal Impulse (Vector)
└─ Hit (Hit Result struct)
```

#### Option B: OnComponentHit (Component level)
```
OnComponentHit (YourCollisionComponent)
├─ Hit Component (Component that was hit)
├─ Other Actor (Actor that was hit)
├─ Other Comp (Other's component)
├─ Normal Impulse (Vector)
└─ Hit (Hit Result struct)
```

**Try this:**
1. Delete your current hit event
2. In Components panel, **select your collision component**
3. In Details panel, scroll down to **Events**
4. Click **+ On Component Hit**
5. This creates the correct event in your graph

---

## Step 4: Add Comprehensive Debug Logging

Replace your current hit logic with this debug version:

```
Event BeginPlay
└─ [Print String] "Projectile spawned! Instigator: " + Get Display Name(Get Instigator)
   └─ Duration: 5.0
   └─ Color: Green

OnComponentHit (or Event Hit)
│
├─ [Print String] "========== HIT DETECTED =========="
│  └─ Duration: 5.0
│  └─ Color: Yellow
│
├─ [Print String] "Hit Actor: " + Get Display Name(Other Actor)
│  └─ Duration: 5.0
│
├─ [Print String] "Hit Location: " + ToString(Hit Location)
│  └─ Duration: 5.0
│
├─ [Print String] "My Instigator: " + Get Display Name(Get Instigator)
│  └─ Duration: 5.0
│
├─ [Branch] Is Valid? Get Instigator
│  ├─ [True]
│  │  ├─ [Print String] "Instigator is valid!"
│  │  │  └─ Color: Green
│  │  │
│  │  └─ [Cast to BP_ThirdPersonCharacter]
│  │     ├─ [Success]
│  │     │  ├─ [Print String] "Cast SUCCESS! Adding coins..."
│  │     │  │  └─ Color: Green
│  │     │  └─ [Call] AddCoins(1)
│  │     │
│  │     └─ [Failed]
│  │        └─ [Print String] "Cast FAILED! Instigator class: " + GetClass(Instigator)
│  │           └─ Color: Red
│  │
│  └─ [False]
│     └─ [Print String] "Instigator is NULL!"
│        └─ Color: Red
│
└─ [Destroy Actor]
```

---

## Step 5: Check Projectile Spawning

### In BP_ThirdPersonCharacter (where you fire the coin)

Find where you spawn the projectile. Make sure it looks like this:

```
Spawn Actor from Class
├─ Class: BP_TriggerHappy_Projectile
├─ Spawn Transform: [Gun muzzle location/rotation]
└─ Spawn Collision Handling Method: Always Spawn, Ignore Collisions

Return Value (the spawned actor):
├─ [Set Instigator] = Self (THIS IS CRITICAL!)
├─ [Set Owner] = Self
└─ [Print String] "Projectile spawned by: " + Get Display Name(Self)
   └─ Duration: 2.0
   └─ Color: Cyan
```

**The "Set Instigator" is crucial!** Without this, Get Instigator returns null in the projectile.

---

## Step 6: Check ProjectileMovementComponent

If your projectile has a **Projectile Movement Component**:

1. Select it in Components panel
2. In Details panel, find **Projectile** section:
   - **Initial Speed**: Should be > 0 (try 3000)
   - **Max Speed**: Should be > Initial Speed
   - **Should Bounce**: False (unless you want bouncing)

3. Find **Projectile Bounces** section:
   - **On Projectile Stop**: Has an event you can bind to

---

## Step 7: Alternative - Use OnComponentBeginOverlap

If blocking collision isn't working, try overlap:

1. In collision settings, set:
   - **Collision Enabled**: `Query Only (No Physics Collision)`
   - **Collision Responses**: Set everything to **Overlap**

2. Use this event instead:
```
Event ActorBeginOverlap (or OnComponentBeginOverlap)
├─ [Print String] "OVERLAP DETECTED!"
├─ Get Instigator
│  └─ Cast to BP_ThirdPersonCharacter
│     └─ Call AddCoins(1)
└─ Destroy Actor
```

---

## Step 8: Check What You're Shooting At

The thing you're shooting (wall, floor, etc.) also needs collision!

1. Select the object in your level
2. Check its collision settings
3. Make sure it's set to **Block** or **Overlap** for WorldDynamic

---

## Quick Test Setup

### Test 1: Verify Projectile Spawns

Play the game and shoot. You should see:
- "Projectile spawned by: BP_ThirdPersonCharacter_C_0" (or similar)
- This confirms spawning works

### Test 2: Verify Projectile Exists

1. Play game
2. Shoot
3. Press **~** key to open console
4. Type: `stat game`
5. Look for actor count increasing when you shoot

### Test 3: Verify Collision

In the editor:
1. Select your projectile in Components
2. Click **Collision** preset dropdown
3. Change to **BlockAllDynamic**
4. Compile and test

---

## Common Issues and Fixes

### Issue: No prints at all, even BeginPlay
**Fix:** Event not wired up. Check Event Graph.

### Issue: BeginPlay print shows, but no hit prints
**Fix:** Collision settings wrong. Set to BlockAllDynamic.

### Issue: Hit prints show, but "Instigator is NULL"
**Fix:** Not setting instigator when spawning. Add Set Instigator node after Spawn Actor.

### Issue: "Cast FAILED" message
**Fix:** Instigator is not BP_ThirdPersonCharacter type. Check what class your character is.

### Issue: Projectile disappears immediately
**Fix:** Check Lifespan setting. In projectile details, search "Initial Life Span" - set to 0 for infinite, or higher value like 10.

### Issue: Projectile doesn't move
**Fix:** Check Projectile Movement Component's Initial Speed. Set to 3000+.

---

## Nuclear Option: Recreate Projectile

If nothing works, the projectile blueprint might be corrupted. Here's how to recreate:

1. **Content Browser → Characters/TriggerHappy/Blueprints**
2. **Right-click → Blueprint Class → Actor**
3. **Name it: BP_TriggerHappy_Projectile_New**
4. **Open it:**

### Components Setup:
```
DefaultSceneRoot
└─ SphereCollision (Add: Sphere Collision)
   └─ StaticMesh (Add: Static Mesh Component)
└─ ProjectileMovement (Add: Projectile Movement Component)
```

### SphereCollision Settings:
- Sphere Radius: 12.0
- Collision Presets: BlockAllDynamic
- Simulate Physics: False

### StaticMesh Settings:
- Static Mesh: Choose your coin mesh
- Collision: NoCollision (the sphere handles collision)

### ProjectileMovement Settings:
- Initial Speed: 3000
- Max Speed: 3000
- Should Bounce: False

### Event Graph:
```
Event BeginPlay
└─ Print String: "New projectile spawned!"

OnComponentHit (SphereCollision)
├─ Print String: "HIT!"
├─ Get Instigator → Cast to BP_ThirdPersonCharacter
│  └─ AddCoins(1)
└─ Destroy Actor
```

5. **Test with new projectile**
6. **Update character to spawn new projectile class**

---

## Report Back

After trying these steps, tell me:
1. Do you see the "Projectile spawned!" message?
2. Do you see any hit-related messages?
3. What collision preset is your projectile using?
4. Does the projectile move when you shoot it?

This will help me narrow down the exact issue!

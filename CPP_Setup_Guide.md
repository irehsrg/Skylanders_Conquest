# Converting Skylanders Conquest to C++ - Setup Guide

## Prerequisites

You need a C++ compiler. For Unreal Engine 5:
- **Windows**: Visual Studio 2022 (Community Edition is free)
- Download: https://visualstudio.microsoft.com/downloads/
- During install, select: "Game development with C++"

If you already have VS installed, you're good to go!

---

## Step 1: Close Unreal Engine

Make sure your project is closed. This is important for the next steps.

---

## Step 2: Add C++ Class to Project

### Option A: Through Unreal Editor (Easiest)

1. **Open your project** in Unreal Engine
2. **Tools → New C++ Class** (or File → New C++ Class)
3. Choose Parent Class: **Actor**
4. Name it: `TestActor` (we'll delete this later, just to generate files)
5. Click **Create Class**

Unreal will:
- Generate Visual Studio project files
- Create a Source folder
- Open Visual Studio
- Compile the project

**This will take 5-10 minutes the first time** as it compiles the engine.

### Option B: Manual Setup (If Option A doesn't work)

1. Right-click `Skylanders_Conquest.uproject` in Windows Explorer
2. Select **Generate Visual Studio project files**
3. Wait for it to complete
4. Double-click `Skylanders_Conquest.sln` to open Visual Studio

---

## Step 3: Verify Setup

After compilation finishes:
1. Unreal Editor should open automatically
2. You should see a **Source** folder in your project directory
3. In Content Browser, you should see C++ Classes folder

---

## Step 4: I'll Create All The C++ Files

Once you complete steps 1-3, I'll create:
- Character class (health, mana, coins, damage, abilities)
- Projectile class (working collision)
- Game mode class
- HUD widget class
- All the header files

You just copy-paste my files into your project.

---

## Step 5: Compile

After I create the files:
1. In Visual Studio: **Build → Build Solution** (or Ctrl+Shift+B)
2. Wait for compile (2-3 minutes)
3. Open Unreal Editor
4. You're done!

---

## What You'll Get

Instead of clicking nodes for hours, you'll have:
- Full health/mana/coin system working
- Projectile that detects hits properly
- HUD that updates automatically
- Ability system ready to use
- All in C++ that I can modify instantly

---

## Time Estimate

- Installing Visual Studio: 20-30 min (if not installed)
- Generating C++ project: 5-10 min
- My code creation: 10 min
- Compilation: 2-3 min
- **Total: 15-45 min** depending on if you have VS

---

## Do You Have Visual Studio?

**Reply with:**
- "Yes, have VS" → I'll create the C++ files now
- "No, installing now" → Install first, then let me know
- "Already have Source folder" → Perfect! I'll create files now
- "Need help with VS" → I'll guide you through install

Once you're ready, I'll write all the code!

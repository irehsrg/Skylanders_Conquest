# Skylanders Asset Extraction — Working Pipeline

**Status as of 2026-08-06: the full chain works, end to end, from a legitimately
owned Switch copy.** One FBX has been produced and verified.

This exists because the extraction tooling did *not* support Imaginators on
Switch out of the box — a bug we found and patched. That patch is the key piece
of knowledge in this document; without it nothing downstream works.

---

## The chain

```
Switch (owned copy, modchipped OLED)
  └─ Lockpick_RCM (via Hekate → Payloads)      → /switch/prod.keys
  └─ nxdumptool (title takeover, NOT applet)   → RomFS dump
       └─ archives/*.pak  (5,620 paks, meaningful names)
            └─ igArchiveExtractor  [PATCHED — see below]  profile: `switch si`
                 └─ *.igz  (1,962 files, 100% valid)
                      └─ IgzModelConverter GUI
                           └─ *.fbx  ✓
```

---

## THE PATCH (most important section)

`igArchiveExtractor` ships with **no Switch support for Imaginators** — its menu
offers only `PS3/Xbox360/WiiU` and `PS4`. Using the PS4 profile *appears* to work
(the file table parses, names look right) but **silently emits undecoded data**:
every output is 32 KB-aligned, has no `IGZ` magic, and still reports "succeeded".

**Root cause.** In `IGA_File.ExtractFile`, compression mode `0x20`:

```csharp
chunkSize = 0x8000;                                  // 32768
compressedSize = (version&0xFF <= 0x0B) ? ReadUInt16() : ReadUInt32();
properties = ReadBytes(5);
if (properties[0] == 0x5D && ToUInt32(properties,1) <= chunkSize) { LZMA decode }
else { seek(-7); copy chunkSize RAW bytes; }         // ← silent garbage path
```

Imaginators-NX uses LZMA props byte **`0xAB`** with a **1 MB dictionary**. Both are
legal LZMA, but the check demands `0x5D` and a ≤32 KB dict, so valid data was
rejected and fell through to the raw-copy branch.

**Three changes fix it** (repo: `NefariousTechSupport/igArchiveExtractor`):

1. `IGA/IGA_Version.cs` — add the version:
   ```csharp
   SkylandersImaginatorsNX = 0x3000000B,
   ```
2. `IGA/IGA_Structure.cs` — duplicate the entire `SkylandersImaginatorsPS4`
   entry in the `headerData` dictionary, keyed to `SkylandersImaginatorsNX`.
   The header layout is **identical** (that's why the file table always parsed).
3. `IGA/IGA_File.cs` — in the `case 0x20:` decode loop:
   ```csharp
   bool isNX = (_version == IGA_Version.SkylandersImaginatorsNX);
   // size: NX reads 32-bit, not 16-bit
   if (!isNX && ((uint)_version & 0x000000FF) <= 0x0B) { ReadUInt16 } else { ReadUInt32 }
   // props: NX accepts ANY legal lzma props byte
   bool propsOk = isNX ? (properties[0] < 225)
                       : (properties[0] == 0x5D && ToUInt32(properties,1) <= chunkSize);
   if (propsOk) { ...decode... }
   // raw fallback seek must be -9 for NX (not -7)
   if (!isNX && ((uint)_version & 0x000000FF) <= 0x0B) { Seek(-7) }
   ```
   Also map it in `Program.cs`:
   ```csharp
   case "si" when platform == "switch" || platform == "nx":
       alchemyVersion = IGA_Version.SkylandersImaginatorsNX; break;
   ```

**The patch is published** (so it is no longer only on the D: drive):
<https://github.com/irehsrg/igArchiveExtractor/tree/add-imaginators-switch-support>
(commit `49d11e6`). To recreate the working tree from scratch:

```bash
git clone -b add-imaginators-switch-support https://github.com/irehsrg/igArchiveExtractor.git
cd igArchiveExtractor && dotnet build -c Release -f net6.0-windows
```

Upstream (`NefariousTechSupport/igArchiveExtractor`) is **archived / read-only**,
so a PR cannot be opened against it. The fork branch is the canonical home.

### Build & run

```powershell
cd D:\SkylandersExtract\igArchiveExtractor
dotnet build -c Release -f net6.0-windows      # net6.0-windows, .NET 6 SDK — no Visual Studio needed
.\bin\Release\net6.0-windows\igArchiveExtractor.exe extractAll switch si <pak> <outDir>
.\bin\Release\net6.0-windows\igArchiveExtractor.exe listFiles  switch si <pak> <outFile>
```

`"Data Error but let's ignore that"` messages during extraction are **non-fatal**
chunk skips — the run still produced 1,962/1,962 valid IGZ.

---

## Paths on this machine

| What | Where |
|---|---|
| Working dir | `D:\SkylandersExtract\` |
| Patched extractor | `D:\SkylandersExtract\igArchiveExtractor\` |
| **Good extraction** | `D:\SkylandersExtract\nx4\` |
| Level geometry | `nx4\level_505\Temporary\BuildServer\nx\Output\models\LevelAssets\2016\Level_505\Statics\` (133 igz) |
| Model converter | `D:\SkylandersExtract\converter\IgzModelConverterGUI.v1.4.exe` |
| Noesis + plugin | `D:\SkylandersExtract\noesis\noesisv4474\` |
| Source pak | `D:\SkylandersExtract\level_505.pak` |
| Asset rip clone | `C:\Users\ajoin\Downloads\Skylanders-Models\` |

**Ignore `D:\SkylandersExtract\level505\`, `try_*`, `t2_*`, `nx_out*`, `nx3\`** —
those are pre-patch garbage extractions.

### Dead ends (don't repeat)

- **Noesis + `fmt_alchemy_igz.py`** — plugin registers as *Skylanders SuperChargers*;
  magic check passes but it can't parse Imaginators IGZ internals. "File could not
  be previewed" even on valid input. (Its `noesis.logPopup()` also deadlocks CLI
  mode — already patched out locally.)
- **Noesis CLI** (`?cmode`) — silent no-op, no logs. Never got it working.
- **`IgzModelConverter` on pre-patch files** — infinite loops (CPU pegged, RAM flat).
  That was *garbage input*, not a tool bug. It works fine on valid IGZ.
- **JKSV "Mount Process RomFS"** — not present in the current version.
- **`igModelConverter`** (successor) — no prebuilt release, needs a C++/premake build.

---

## The Switch dump (if it needs redoing)

- Keys: Lockpick_RCM via **Hekate → Payloads** → `/switch/prod.keys`
- Dump: **nxdumptool**, launched via **title takeover** (hold R on a game — the
  Album/applet path has a RAM cap that fails on multi-GB dumps)
- Choose: *installed SD/eMMC titles* → Imaginators → **Base Application** →
  NCA FS section → **Program #0** → **RomFS** (not NSP — RomFS is the raw file tree)
- Output lands in **`/nxdt_rw_poc/`** on the SD (pre-release path), *not*
  `/switch/nxdumptool/`
- Took ~981 s. **13 more level paks remain undumped** (`level_500`–`level_516`).

MTP access: run **DBI** on the Switch; it appears in `This PC` as a portable
device. PowerShell `Get-PSDrive` will NOT see it — use `Shell.Application`
namespace(17) and `.GetFolder` for traversal.

---

## Game project state (all merged to `master`)

**Done:**
- 3v3 team-aware AI (`ETowerTeam Team` on `ASkylandersEnemyGod`), NavMesh pathing,
  tower↔god interaction, friendly-fire prevention
- **Real animated Skylanders**: gods = Spyro, Stealth Elf, Kaos Sensei, Cynder,
  Chompy Mage. Minions = Terrabite/Drobit. Camps = Shaman, GraveClobberer,
  GoldenQueen, BadJuju
- Map on **authentic Imaginators textures** — `Barrens!Sand01` ground,
  `Verdant!VRD_Rock_Medium` terrain (`Scripts/ImportLevelTextures.py`)
- Telemetry (`Saved/Telemetry/*.jsonl`), kill feed, match timer, team gold,
  20-item shop, pause/settings/end screens

**Outstanding:**
1. **Minion/camp models are wrong** (user-flagged, valid): Terrabite and Drobit are
   *playable Minis* — they'd never be lane fodder. Replace with actual enemies:
   `ArcherGoat`, `ArcherRobot`, `CrockWarrior`, `Gargoyle`, `Scorpion`, `Chopper`.
   Villain camps (GoldenQueen etc.) are defensible; playable Minis as minions are not.
   Verify textures exist per candidate first — `BullTrain`/`CatGryphon`/`Gargoyle`
   had models+anims but no discoverable body texture.
2. **Batch the IGZ→FBX step** — converter is GUI-only, needs clicking per file.
   133 statics in level_505 alone.
3. **Import geometry to Unreal** and rebuild the Joust map on real meshes.
4. Towers/titans still colored cylinders.

**Character import pipeline** (already built, reusable):
- `Scripts/StageCharacterTextures.py <Name>[=pattern]` — finds body/normal maps
  among mangled rip filenames, converts palettized PNG → RGBA
- `Scripts/ImportGodCharacters.py` — set `SKY_CHARS=A,B,C`; imports mesh +
  filtered anims + textures, builds `M_<Name>_Body`
  (**must** set `used_with_skeletal_mesh` or it renders gray)
- Per-model `Scale`/`MeshZ` derived from real bounds:
  `MeshZ = <groundOffset> - bottomZ*Scale` (gods −50, minions −30, camps −75)
- **227 characters have full animation sets upstream** in the rip repo; the local
  clone is *sparse*. Fetch more with:
  `git sparse-checkout add Assets/Skylanders/Animations/<Name>`

---

## Hard-won gotchas

- `HighResShot` does **not** capture UMG/Slate overlays, and does not update while
  the game is paused. Screenshot unpaused.
- Remote Python (`ue_remote.py`) is **single-statement only**; use `;` not newlines.
- Materials built from a mesh's default slot render **gray** — the engine
  `DefaultMaterial` has no `Color` param. Build from `BasicShapeMaterial`.
- `SpawnActor` + `SetRootComponent` **discards the spawn transform** — re-apply
  `SetActorLocationAndRotation` after `RegisterComponent`.
- Editor file-locks `.uasset`s; close it before git operations that touch Content.
- After merging a PR whose branch added Content, `git checkout master` leaves the
  assets untracked and blocks the pull — verify they're in `origin/master`, delete
  the local copies, then pull.

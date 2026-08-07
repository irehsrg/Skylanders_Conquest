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
                      └─ IgzModelConverterGUI  [HIDDEN CLI — see below]
                           └─ *.fbx  ✓
```

---

## THE CONVERTER'S HIDDEN CLI (batching, no clicking)

`IgzModelConverterGUI.v1.4.exe` is documented as GUI-only, but it is **not**.
Its `Main(String[] args)` opens the WinForms window only when `args.Length <= 2`;
with **three or more** args it converts headlessly and exits:

```
IgzModelConverterGUI.v1.4.exe <file.igz> <game> <type>
    game: nst | ctr | skyl          (skyl = Skylanders)
    type: act | mod | sky           (mod = static model)
```

The FBX is written **next to its input file**, so batch workers must each stage
their own copy or same-named outputs collide.

How this was found: the exe is a **mixed-mode C++/CLI x86 assembly** (FBX SDK
statically linked), so `Assembly.LoadFrom` fails with *"unverifiable executable
with fixups"* and `GetTypes()` throws. `ReflectionOnlyLoadFrom` + catching
`ReflectionTypeLoadException.Types` gets the type list anyway; the arg strings
came out of the `#US` metadata heap at the `ldstr` tokens in `Main`'s IL.

**Batch driver: `D:\SkylandersExtract\convert_batch.sh`**

```bash
JOBS=6 TIMEOUT=90 ./convert_batch.sh <srcDir> <outDir> skyl mod
```
Parallel workers, skips already-converted files, hard per-file timeout.

### Yield on level_505 Statics (133 igz)

| Outcome | Count |
|---|---|
| FBX with real vertex data | **91** |
| Converter infinite-loops (any game/type combo) | 39 |
| FBX produced but **zero vertex data** | 13 (deleted) |

- **A per-file timeout is mandatory.** ~30% of these statics send the converter
  into an infinite loop — confirmed, not slowness: `l505_fence.igz` (96 KB) ran
  7 minutes under `mod` and produced nothing.
- **`act` is not a rescue for `mod` failures.** It *appears* to work — several
  `mod`-hanging files convert in seconds — but every one of those FBX contains
  **no `Vertices` array**. Always verify with `grep -ac Vertices <fbx>`; hollow
  files are otherwise 40–130 KB and look plausible.
- Running 6 workers in parallel causes occasional segfaults/rc=127; harmless,
  the file is just retried on the next pass.

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

### Three more patches: reading NX *textures* (2026-08-07)

`igArchiveExtractor` could not open a Switch IGZ at all. Three bugs, same family
as the archive one. With these it reads them and reports e.g.
`Image Found: 512, 512, 8, dxt1_nx`:

1. **`IGZ/IGZ_File.cs`** - the little-endian magic constant was `0x48475A01`.
   Correct is **`0x49475A01`** (`'H'` where `'I'` belongs). Every little-endian
   IGZ was thrown out as *"File is corrupt."*
2. **`IGZ/IGZ_Fixup.cs`** - fixup magics are stored byte-reversed on *both*
   endiannesses, but `Process()` only flipped them for Big. On little-endian the
   `RVTB`/`TMET` lookups in `IGZ_File.Init` missed and it threw *"Sequence
   contains no matching element"*. Fix: always flip.
3. **`Types/igImage2.cs`** - 64-bit (Switch) IGZ keep version 9 but use 8-byte
   pointers, pushing the width/height/depth/mip/array block **0x14 later**
   (`offset + 0x48`, not `+0x34`). There is no platform flag to key off, so it
   re-reads at `+0x48` when width comes back 0. `IGZTEX.cpp` in
   AdventureT/IgzModelConverter independently uses 72 == 0x48, confirming this.

**All three are worth upstreaming**, like the `switch si` profile.

### The texture swizzle is NOT solved - do not redo this ground

Getting *pixels* out still fails. Established facts, so nobody repeats the work:

- The payload is at the **largest section in the header table** (`off`/`size`
  pairs from 0x18), e.g. offset 2628 size 174760 for `SH_Dirt01_C`.
- It **is** BC1: 65% of 8-byte blocks have `c0>c1` and 6.5% have `c0==c1`,
  against 50%/0% for random bytes.
- 512x512, 8 mips. Payload size is **exactly the UNPADDED mip chain** (174760).
  Standard GOB-padded Tegra block-linear would be 176128, so the layout is a
  **size-preserving permutation** - it is not padded block-linear.
- **Neither existing tool deswizzles NX.** `igArchiveExtractor` has only
  `DeswizzleWiiU` (GX2); `IGZTEX.cpp` writes a DDS header then copies the bytes
  raw for Switch formats exactly as for PC. Both were read in full.

Ruled out by experiment: linear at ~600 candidate offsets; Tegra block-linear at
3 offsets x 6 block_heights; Morton/Z-order (both axis orders); block tiling at
4x4, 8x8, 16x16, 8x4, 4x8, 32x32.

Untried next steps: port a known-good Tegra deswizzler verbatim (Switch Toolbox)
rather than hand-rolling the address maths, or ask the Skylanders Reverse
Engineering Discord.

### TEXTURES ARE LINEAR, NOT SWIZZLED (solved 2026-08-07)

`Scripts/igz_texture.py` extracts them. The payload is **plain linear BC1/BC3**
despite the `dxt1_nx` format name - a straight read gives the image; every
Tegra block-linear variant gives noise.

The earlier "it must be swizzled" conclusion came from testing `SH_Dirt01_C`
(512x512), where the padded and unpadded mip-chain sizes differ and the linear
read is genuinely broken. On a **256x256** they coincide, and the linear read is
obviously correct. So: **256x256 textures decode perfectly; 512x512 do not yet.**
Whatever is wrong with the larger ones is size-dependent, not a global swizzle.

- dimensions come from `igImage2` at **object + 0x48** (64-bit NX), never guessed
- BC1 vs BC3 is chosen by which full mip chain matches the declared payload size
- convert to **RGB, not RGBA** - DXT1's 1-bit alpha otherwise punches transparent
  texels that read as black stripes in the unlit ground material
- some textures carry ~20 defective rows at the bottom. `repair_tail()` mirrors
  the band above them. Two separate defects need two tests: NOISE rows (high
  horizontal delta) and DEAD rows (near-black, which have *low* delta and slip
  past a noise-only check). Mirror rather than replicate one row, and back off
  8 rows first - the defect fades rather than stopping cleanly, so a naive
  repair streaks dark dashes across every tile.

**In use now**: `SH_Grass01_C`, `L505_Cobblestone_01__C` and `SH_Mud_C` are the
live ground textures in `M_Ground` (via `Saved/GroundTex/`). The map renders the
real Skylanders cobbled path through real Skylanders grass.

### THE EXTRACTION WAS TRUNCATING FILES (found + fixed 2026-08-07)

**This was the root cause of nearly everything downstream.** 75 of 133 level_505
statics were being written out incomplete, always cut at a multiple of 32768.

`igArchiveExtractor`'s LZMA loop assumes every chunk decodes to the full 32 KiB
`chunkSize` and asks the bundled SevenZip decoder for exactly that many bytes.
**Chunks are variable** - one real chunk in `l505_yeti_right_Arm.igz` decodes to
2704 bytes. The decoder runs out of input, throws `Data Error`, writes *nothing*,
and the loop does `break`, abandoning the rest of the file:

```csharp
catch(Exception e) {
    Console.WriteLine($"{e.Message} but let's ignore that");
    break;                     // <-- silently truncates the file
}
```

Those "Data Error but let's ignore that" messages were never harmless. They were
one lost file each. **372 of them on level_505 alone.**

Consequences, all of which looked like separate bugs:
- statics converted from cut-off vertex buffers -> NaN positions -> Unreal
  rejects them with *"all polygons are degenerate"* / *"infinite value in the data"*
- the model converter scanned past the end of short files -> the sentinel search
  never terminates -> apparent "infinite loop on actors"
- only 33 of 91 statics ever imported

**Fix: `Scripts/pak_extract.py`.** Python's `lzma` returns partial output instead
of throwing, so the same chunk walk just works. Verified on `l505_yeti_right_Arm`:
chunks decode to `[32768, 2704, 32768, 27788]` = 96028 = exactly the declared size.

```bash
# once per pak - the C# tool still parses the file table best:
igArchiveExtractor.exe dumpTable switch si <pak> <table.csv>     # command added by us
python Scripts/pak_extract.py <pak> <table.csv> <outDir> [name-filter]
```

Format notes that matter:
- chunk header is `u32 compressedSize` + 5 LZMA property bytes, then the data;
  the next chunk starts at the next **chunkAlignment (32-byte)** boundary.
- the compression scheme is the **top byte** of the table's `mode` field
  (`mode >> 24 == 0x20` for LZMA); the low 24 bits are the chunk-properties offset.
- NX props byte is `0xAB` -> `lc=0 lp=4 pb=3`, 1 MiB dictionary. `lp=4` means
  16-byte-aligned data, which fits GPU vertex/texture payloads.

**Result: 133/133 complete, 0 truncated** (was 58/133). Sizes jump dramatically -
`L505_Lighthouse` 32768 -> 639052, `L505_interior_alpine` 196616 -> 4041196.

**Still broken after the fix:** actor IGZ *still* hang the model converter even
when complete, so that is a genuinely separate bug (the unbounded sentinel scans
in `IGZ.cpp` 252-274). Statics improved markedly - files that previously timed
out or crashed Unreal now convert.

### Our own IGZ reader (Scripts/igz_parse.py) - WORKS, incomplete

`IgzModelConverter` parses these files by scanning for magic numbers. On NX that
either spins forever (actors) or yields NaN vertex positions (two thirds of
statics). `Scripts/igz_parse.py` walks the real object graph instead and already
does something the old tool cannot: **it reads actor files at all**, in
milliseconds, including the ones that hang the converter forever.

Container format, all verified against real files:

- Header: magic `0x49475A01` ("ZGI") little-endian at 0, version u32 at 4.
- **Section descriptors start at 0x18**, 16 bytes each `(offset, size, unk, unk)`,
  terminated by offset 0. Typically 4: `[0]` fixups, `[1]`/`[2]` objects,
  `[3]` the raw data blob.
- Fixups live in section 0, back to back: 4-byte tag then
  `count, length, startOfData` (u32 each); advance by `length`.
  **The tag bytes on disk already spell the canonical name** ('RVTB'). The C#
  reader byte-swaps only because it reads them as a uint32 first.
- `TSTR`/`TMET`/`TDEP`/`EXNM` are NUL-separated string tables. `TMET` is the
  **type name table**.
- `MTSZ` is the **per-type object size table** - one entry per type name. Use it
  for object length; the in-object length field reads back as 0.
- `RVTB` and friends are a **base-8 varint delta stream packed as nibbles**:
  low nibble first, continue while bit 3 set, accumulate 3 bits at a time,
  `prev += unpacked*4`. Split with `sec = v >> 27`, `off = v & 0x07FFFFFF`,
  resolving into `descriptors[sec + 1]`.
- Object header (v9): type index u32 at +0, item count u32 at +8, **fields start
  at +0x14**.
- **Pointers** use the same `sec`/`offset` split, resolving to
  `descriptors[sec+1].offset + offset + 8`. The `+8` is verified: vertex-format
  pointers land exactly on the following object's address.

What the graph looks like:

| | statics (`L505_bridge`) | actors (`Rat_Melee`) |
|---|---|---|
| draw calls | 4 | 7 |
| per draw call | `igVertexBuffer` + `igVertexFormat` + `igIndexBuffer` (+ graphics variants) | same |
| skeleton | - | `igSkeleton2` + `igSkeletonBoneList` + 71 x `igSkeletonBone` + `CGraphicsSkinInfo` |

Useful field offsets (from object start + 0x14):
`igVertexBuffer` count at +0x44; `igIndexBuffer` count at +0x3C;
`igVertexFormat` attribute-table pointer at +0x04, second pointer at +0x14.

**What is still missing: the per-buffer vertex attribute layout.** One concrete
layout is confirmed - stride **40**, `pos(3xf32) + normal(3xf32) + uv(2xf32)`,
which decodes with exactly unit-length normals. But formats differ per buffer
(the four `igVertexFormat`s in `L505_bridge` are not all alike), so the attribute
descriptor table at `igVertexFormat+0x04` has to be decoded rather than assumed.
Its raw u32 stream for format 0 is `0, 2, 1, 12, 42, 5, 24, 2, 2, 28, 44, 0, ...`
- schema not yet worked out. Scanning for stride-40 unit-normal runs finds only
1608 of the declared 18192 vertices, which is the measure of how much is left.

### What is behind that wall

The pak has exactly the material set the SMITE/Skylanders reference art uses -
`L505_Cobblestone_01__C` (a real cobblestone path), `SH_Grass01_C`,
`SH_Grass03_C`, `L505_SW_grass_1_C`, `L505_GrassBlade_C`, `l505_GrassCards02_C`,
`SH_Dirt01_C`, `SH_DirtFine01_C`, `SH_Mud_C`, `SH_StoneFlat02_C`,
`l505_palmLeaf_C`. **None of it is in the Skylanders-Models rip repo**, which
only carries 7 Barrens level textures plus VFX leaf sprites. That is why the
ground still looks like desert.

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

**Minions are now real enemies** (was: Terrabite/Drobit, playable Minis):
- melee = `CrockWarrior` (armoured croc grunt, 97 anims), ranged = `Scorpion`
  (venom-stinger beast, 48 anims). Both in `SkylandersMinion.cpp`.
- **`ArcherGoat` and `ArcherRobot` were rejected**, despite being on the original
  candidate list. Their rip paths are `actors!2016!Skylanders!S618_ArcherGoat` /
  `S625_ArcherRobot`, and the `S6xx_` prefix is the **Imaginators Sensei** series
  (cf. `S627_Sensei_Kaos` = KaosSensei, `s601_penguin`, `taekwoncrow_v1`) — i.e.
  playable characters, the exact problem being fixed. `Gargoyle` still has no
  discoverable body texture. `Chopper` (harnessed raptor) is a viable spare.
- The **pak's real enemies** (`actors/2016/Enemies/Rat_Melee|Rat_Lobber|Rat_Brute|
  Rat_Swarmer`) would be ideal but are **blocked**: actor IGZ hang the converter
  on every game/type combo, because the CLI passes null for the texture/anim
  paths the GUI normally supplies.
- `StageCharacterTextures.py` now falls back `_C` → `_diffuse1/_diffuse01/...`;
  the older enemy rips don't use the `_C` body-atlas suffix.

**Level geometry is imported and the map is built on it:**
- `Scripts/ImportLevelGeometry.py` (+ `import_level_geometry.sh` driver) →
  `/Game/Environment/Level505`. **33 of 91** FBX survive import.
- Two hard blockers, both from corrupt vertex data in the rips:
  * **Interchange asserts and takes the whole commandlet down.** Force the legacy
    importer: `Interchange.FeatureFlags.Import.FBX 0`. 15 files crash *both*
    importers, so the script is resumable and blacklists whatever was in flight.
  * 43 more are rejected as *"infinite value in the data"* / all-degenerate.
    `combine_meshes=False` + `remove_degenerates=False` does **not** rescue them.
- Several that *do* import carry garbage bounds (`l505_craneBase` is ~1e27 uu).
  Check `_bounds.json` before using a mesh; only sane ones are wired up.
- `SkylandersMapBuilder` dresses the out-of-bounds grid with real `1505_rock02` /
  `l505_rock04` / `L505_trees` and lane-edge `L505_grass`, hash-scattered so the
  layout is identical every match. The dark cubes stay underneath for gap-free
  silhouette + collision; all decor is collision-free.

**Outstanding:**
1. Towers/titans still colored cylinders.
2. The 39 unconvertible + 58 unimportable level_505 statics need either a
   converter fix or a mesh-repair pass (strip NaN/Inf verts) to be usable.

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

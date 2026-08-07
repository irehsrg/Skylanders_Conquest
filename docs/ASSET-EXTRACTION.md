# Skylanders Asset Extraction — Working Pipeline

**Status as of 2026-08-07: the full chain works, end to end, from a legitimately
owned Switch copy.** 2354/2354 files extract; 133/133 level_505 statics and 7/7
enemy actors convert to FBX with zero timeouts; all textures decode.

Two things in this document are load-bearing. The first is the `switch si`
profile patch to `igArchiveExtractor` — without it nothing downstream works. The
second, and the one that actually cost the most time, is **[THE EXTRACTION WAS
CORRUPTING FILES](#the-extraction-was-corrupting-files-root-cause-settled-2026-08-07)**:
NX paks are chunked at 1 MiB, not 32 KiB, and assuming 32 KiB silently spliced
other files' data into every file over that size while keeping the byte count
exactly right. Nearly every "separate" asset bug in this project — hanging
actors, broken 512x512 textures, degenerate statics, the supposedly undecoded
vertex schema — was that one bug wearing a different hat. Read that section
before diagnosing anything.

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

| Outcome | before the extraction fix | now (`nx9`) |
|---|---|---|
| FBX with real vertex data | 91 (then 29/60 on retest) | **133 / 133** |
| Converter infinite-loops | 39 | **0** |
| FBX produced but zero vertex data | 13 (deleted) | 0 |

Enemy actors went from *every one hangs* to **7 / 7**.

- **The infinite loops were not a converter bug.** They were corrupt input - the
  sentinel scan was walking through another file's data. Given a correct `nx9`
  extraction the converter terminates on everything, including the 4.9 MB
  `L505_ferris_wheel` that previously ran forever at a 480 s timeout. Keep the
  per-file timeout in `convert_batch.sh` as a backstop; nothing hits it now.
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

### TEXTURES ARE LINEAR, NOT SWIZZLED - and 512x512 now work too

`Scripts/igz_texture.py` extracts them. The payload is **plain linear BC1/BC3**
despite the `dxt1_nx` format name - a straight read gives the image; every
Tegra block-linear variant gives noise.

Two wrong explanations were published here before the real one; both are struck
out for good, because each cost hours:

1. *"It is Tegra block-linear and needs a deswizzler."* No. Ruled out by
   experiment across ~600 candidate offsets, 3 offsets x 6 block_heights of
   Tegra block-linear, Morton/Z-order both axis orders, and block tiling at
   4x4/8x8/16x16/8x4/4x8/32x32. Nothing was swizzled at any point.
2. *"256x256 work but 512x512 cannot, because the padded and unpadded mip-chain
   sizes coincide only at 256."* Also no. That reasoning was invented to explain
   a symptom whose cause was somewhere else entirely.

**The actual cause: the extractor was corrupting every file over 32 KiB.** A
256x256 BC1 texture fits inside one 32 KiB chunk and so came out intact; a
512x512 does not. Nothing was ever wrong with the texture code. See *THE
EXTRACTION WAS CORRUPTING FILES* below. With `nx9`, all 512x512 textures -
`SH_Dirt01_C`, `SH_Dirt01_N`, `L505_Rock_01_C`, `L505_Rock_01_N`,
`SH_Grass01_G` - decode correctly.

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

### THE EXTRACTION WAS CORRUPTING FILES (root cause, settled 2026-08-07)

**This one bug caused nearly every "separate" asset problem.** It went through
two rounds of misdiagnosis, so the history matters:

**Round 1 (wrong): "chunks are variable, the decoder throws, the loop breaks."**
`igArchiveExtractor` asked its SevenZip decoder for a full 32 KiB per chunk, got
`Data Error`, and `break`-ed - so files were truncated at a 32 KiB boundary. The
first `pak_extract.py` "fixed" this by accepting whatever each chunk produced and
carrying on. Output lengths then matched the declared sizes exactly and it looked
solved. It was not: it had converted a *truncation* bug into a *corruption* bug.

**Round 2 (correct): NX pak files are chunked at 1 MiB, not 32 KiB.**
Each chunk is a raw LZMA1 stream behind a `u32 compressedSize` + 5 LZMA property
bytes. The chunk's uncompressed size is the **dictionary size carried in those
properties** (1 MiB here). Because both readers assumed 32 KiB, they advanced by
`9 + csize` where csize spans a whole 1 MiB chunk - landing **inside the next
file** and splicing its data in until the byte count happened to reach `size`.

Damage on level_505: **807 of 2354 files (34%)**, and **540 of 570** igz over
32 KiB. Files at or under 32 KiB fit in one chunk and were correct by accident -
which is exactly why 256x256 textures always worked and 512x512 never did.

Everything below was a symptom of this, not a separate bug:
- "the model converter infinite-loops on actors" - it was scanning through
  another file's data looking for a sentinel that was not there
- "512x512 textures decode wrong" - nothing to do with Tegra swizzling or
  mip-chain padding, both of which were confidently asserted and both wrong
- "NaN positions / all polygons are degenerate" on import
- "the vertex attribute schema is undecoded and is the keystone" - the schema
  was never the blocker

**Second, subtler bug exposed once the chunk size was right.** A raw LZMA1
stream carries no end-of-stream marker, so a decoder handed the whole remaining
byte count runs the trailing bits of the final byte out into **one spurious
extra byte**. On `L505_ferris_wheel` chunk 3 yielded 1048577 instead of 1048576,
shifting every byte past the 4 MiB mark by one - enough to turn its last two
meshes into NaN while the file's total length still matched exactly. Cap each
chunk at `min(dict_size, remaining)`.

> **The lesson: a correct output length proved nothing here.** Both bugs produced
> byte-perfect file sizes. Validate *content* - exactly one IGZ magic at offset 0,
> and every declared vertex buffer decoding to unit-length normals.

**Fix: `Scripts/pak_extract.py`**, which now also parses the pak's file table
itself. The `dumpTable` round-trip through the C# tool is no longer needed:

```bash
python Scripts/pak_extract.py <pak> <outDir> [name-filter]
# a CSV table is still accepted for archive versions it does not know:
python Scripts/pak_extract.py <pak> <table.csv> <outDir> [name-filter]
```

Format notes that matter:
- chunk header is `u32 compressedSize` + 5 LZMA property bytes, then the data;
  the next chunk starts at the next **chunkAlignment (32-byte)** boundary
- the compression scheme is the **top byte** of the table's `mode` field
  (`mode >> 24 == 0x20` for LZMA)
- NX props byte is `0xAB` -> `lc=0 lp=4 pb=3`, 1 MiB dictionary. `lp=4` means
  16-byte-aligned data, which fits GPU vertex/texture payloads
- header offsets for the NX layout (version `0x0C`) are in `pak_extract.py`;
  nametable at `+0x28`, checksums at `+0x38`, 16-byte local headers

**Results, measured:**

| | before | after |
|---|---|---|
| files extracted, 0 short | - | **2354 / 2354** (~12 s) |
| statics -> FBX | 29 / 60 | **133 / 133, zero timeouts** |
| enemy actors -> FBX | 0 (hung) | **7 / 7** |
| mesh igz with all normals valid | 9 / 133 | **417 / 419** |
| large igz with foreign data spliced in | 540 / 570 | **0 / 774** |

The C++ converter's "unbounded sentinel scan in `IGZ.cpp` 252-274" **needs no
patch**. Given correct input it terminates on everything, including the 4.9 MB
`L505_ferris_wheel` that previously ran forever. Keep the per-file timeout in
`convert_batch.sh` as a backstop, but nothing hits it now.

**Good tree is `D:/SkylandersExtract/nx9/`. Delete nx4-nx8 - they are all
corrupt, and nx7 in particular looks healthy and is not.**

### Our own IGZ reader (Scripts/igz_parse.py)

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
- **Pointers** are 8-byte slots. The **high dword** carries the location, using
  the same `sec`/`offset` split, resolving to
  `descriptors[sec+1].offset + offset` — **no `+8`**. (An earlier note here
  claimed a verified `+8`; that came from reading `igVertexBuffer+0x00`, which
  happens to sit 8 bytes before the object it looked like it pointed at. The
  no-offset form is what resolves `igVertexBuffer+0x10 -> igVertexFormat`
  correctly across all 419 mesh files.) The low dword is a tag, `0x80000000`
  or 0.

What the graph looks like:

| | statics (`L505_bridge`) | actors (`Rat_Melee`) |
|---|---|---|
| draw calls | 4 | 7 |
| per draw call | `igVertexBuffer` + `igVertexFormat` + `igIndexBuffer` (+ graphics variants) | same |
| skeleton | - | `igSkeleton2` + `igSkeletonBoneList` + 71 x `igSkeletonBone` + `CGraphicsSkinInfo` |

### Scripts/igz_mesh.py - geometry without the converter

The vertex attribute schema **is decoded** (it was never the real blocker - the
"only 1608 of 18192 vertices decode" measurement recorded here previously was
reading a corrupted file). `Scripts/igz_mesh.py` reads geometry straight from
the object graph in O(objects), so it cannot spin the way the C++ scanner does.

```bash
python Scripts/igz_mesh.py --info <file.igz>
python Scripts/igz_mesh.py --obj <outDir> --glob "<dir>/**/*.igz"
```

Field offsets, from object + 0x14:

| object | field | offset |
|---|---|---|
| `igVertexBuffer` | vertex count | +0x44 |
| | stride | +0x58 |
| | pointer to its `igVertexFormat` | +0x10 |
| `igIndexBuffer` | index count (u16 triangle lists) | +0x3C |
| `igVertexFormat` | attribute table pointer | +0x00 |
| | component table pointer | +0x10 |
| | component table size **in bytes** | +0x0C |

Attribute count is `component_table_size / 4 - 1`; both tables carry one
terminator entry. Each attribute table entry is three u32 - **(type, slot,
offset-within-vertex)** - and an attribute's byte size is the next entry's
offset minus its own, the last running to the stride.

| slot | meaning | seen as |
|---|---|---|
| 0 | position | 3 x f32 |
| 1 | normal | 3 x f32 |
| 2 | tangent | 3 x f32 |
| 4 | colour | RGBA8 (4 bytes) |
| 5 | UV | half2 (4 bytes) |

**Where the data lives.** Vertex and index buffers sit in a section with *no
pointers to them* - the loader streams the section into GPU memory in order.
They appear in object order, interleaved as (vertex, index) pairs, each starting
on a **4-byte** boundary. For `L505_bridge`:

```
VB0 2518x40 | IB0 10518x2 | VB1 3629x44 | IB1 15252x2
VB2 10068x44 | IB2 50454x2 | VB3 1977x44 | IB3 4662x2   = 952148 bytes
```

which is exactly its last section's size. Usually it *is* the last section, but
not always - the cloud models keep geometry in a middle one - so the reader
totals the buffers first and finds the section whose size matches exactly,
refusing the file if none does.

That aggregate check is necessary but **not sufficient**: it proves the buffers
fill the section, not that each starts where predicted. So every mesh is also
checked vertex-by-vertex for unit-length normals and dropped if it fails.
`Gen_Crate_Debris_10` goes bad at its *second* vertex - a strided sample walked
straight past it - and `L505_Icing_Gun` (animated, strides 140/136) does not use
object order at all. Those two are the only known failures: 417 of 419 mesh igz
decode with every normal valid.

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

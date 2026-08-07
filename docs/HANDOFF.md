# Handoff — Skylanders Conquest, asset pipeline (2026-08-07)

Paste this into a new chat to pick up where we left off.

Background reading, in order: `docs/ASSET-EXTRACTION.md` (the authority on the
pipeline), then `Scripts/igz_parse.py`. Everything below is committed to `master`
(merge `da7757b`); working tree is clean.

---

## The one thing to know

**The archive extractor was silently truncating files.** Its LZMA loop assumed
every chunk decodes to a full 32 KiB and asked the bundled SevenZip decoder for
exactly that; chunks are variable, so the decoder threw `Data Error`, wrote
nothing, and `break`-ed — abandoning the rest of the file at a 32 KiB boundary.
**75 of 133 level_505 statics were incomplete.** Those *"Data Error but let's
ignore that"* messages were one lost file each, 372 of them.

That single bug masqueraded as three: "degenerate vertices" on import, "the
converter infinite-loops on actors" (it was reading past the end of a short
file), and only 33 of 91 statics importing. Animation data was worst hit —
`Rat_Melee.hka` was 65594 of 803072 bytes.

Fixed by `Scripts/pak_extract.py`. **133/133 complete.** Use it, never the C#
`extractAll`, for anything that matters.

---

## Working pipeline

```bash
# 1. file table (the C# tool parses this correctly; command added by us)
igArchiveExtractor.exe dumpTable switch si <pak> table.csv

# 2. extract  (Python; correct)
python Scripts/pak_extract.py <pak> table.csv <outDir> [name-filter]

# 3a. textures -> PNG
python Scripts/igz_texture.py <outDir> --glob "<dir>/*SH_Grass*"

# 3b. models -> FBX  (still the flawed C# converter)
JOBS=6 TIMEOUT=120 D:/SkylandersExtract/convert_batch.sh <src> <out> skyl mod

# 4. FBX -> Unreal  (crash-resumable; forces the legacy importer)
export MSYS_NO_PATHCONV=1 SKY_GEO_DIR=... SKY_GEO_DEST=/Game/...
./Scripts/import_level_geometry.sh
```

`Scripts/igz_dump.py <file.igz>` prints any IGZ's object graph — the fastest way
to see what a file actually contains.

Extracted trees live on `D:\SkylandersExtract\`: **`nx7\` is the good one**
(pak_extract output). `nx4`/`nx5`/`nx6` are truncated or diagnostic — ignore.

---

## State of play

**Done and in the game**
- Lane reads as a real path: authentic `SH_Grass01_C` off-lane, real
  `L505_Cobblestone_01__C` on-lane, `4m(1-m)` rim shoulder. Tune via
  `COBBLE_TILE` / `PATH_HALF_WIDTH` / `RIM_DARKEN`.
- Out-of-bounds dressed with real ripped rock/tree meshes, hash-scattered so the
  layout is identical every match.
- `Scripts/PreviewLaneGround.py` renders the ground offline — **tune there**, one
  second per iteration instead of a 90-second game relaunch. It mirrors what
  ships; keep it that way.

**Broken / unfinished, highest value first**

1. **Vertex attribute schema — the keystone.** `igz_parse.py` reads the container
   perfectly but not mesh geometry. The per-buffer attribute table at
   `igVertexFormat + 0x04` is undecoded; for `L505_bridge` format 0 its raw u32
   stream is `0, 2, 1, 12, 42, 5, 24, 2, 2, 28, 44, 0, ...`. Best read so far is
   offset-led triples — F0 has attributes at offsets 0/12/24/28 and F1 inserts an
   extra at 24, shifting the rest — but it does not reconcile cleanly.
   Ground truth to validate against: stride **40** with
   `pos(3xf32) + normal(3xf32) + uv(2xf32)` decodes 636 consecutive vertices with
   exactly unit-length normals at the start of the data section.
   Cracking this unlocks actors AND the ~half of statics the converter still
   times out on, including the Academy towers.

2. **Actors still hang the converter**, even with complete files (tested
   `Rat_Melee.igz` at its full 439 KB, 3 min, nothing). Separate bug: unbounded
   sentinel scans in `IGZ.cpp` 252-274, plus a genuine infinite loop in
   `GetAnimPath` when the path has no `actors` segment. Blocks all minion/camp
   work.

3. **512x512 textures decode wrong.** 256x256 are perfect. Size-dependent, not a
   global swizzle. Blocks `SH_Dirt01_C`, `L505_Rock_01_C`.

4. **Statics: 29 of 60 import** after re-conversion. The converter remains the
   weak link even with correct input — see (1).

---

## Content decisions already made

- **Minions are still wrong.** `SkylandersMinion.cpp` points at CrockWarrior and
  Scorpion — which are **Snap Shot** (Trap Team) and **Scorp** (Swap Force),
  still playable characters. Committed only because the scale/material/anim
  wiring is reusable.
  The rule that failed: screening for the Imaginators Sensei `S6xx_` prefix
  misses every Trap Team / Swap Force playable. **Root cause: the rip's
  `Assets/Skylanders/Actors` IS the playable roster** — wrong source for enemies.
- **Intended replacements**: Chompy (melee) and Spell Punk (ranged) — both
  confirmed in Imaginators, which has 99 enemies total. **Chomp Chests is Trap
  Team and NOT in this copy.** Camp suggestions filtered to Imaginators: Slam
  Clam (gold), Magic Geargolem (blue buff), Fire Spell Punk (red buff),
  Doomlander (big objective — `Doomlander_Pistol.igz` is already in level_505).
- **Towers**: `hub_2016.pak` is the Academy. Extracted and converted —
  `H2016_Tower02`, `TowerRoof05`, `kaostower`, pillars, frontgate — but they fail
  import as degenerate, pending (1).

## Assets available

- `Saved/SwitchArchives.txt` lists all **5620** paks on the Switch. Regenerate
  with `Scripts/ListSwitchArchives.ps1` (SD card, or DBI over MTP — the dump sits
  8 levels down at `SD Card\nxdt_rw_poc\NCA FS\User\Extracted\<title>\Program #0\1\archives`).
- On disk: `level_505.pak`, `hub_2016.pak`. Notable not-yet-pulled:
  `driverspellpunk.pak` (standalone Spell Punk actor). Chompies live inside level
  paks; 14 exist (500–510, 512, 515, 516).

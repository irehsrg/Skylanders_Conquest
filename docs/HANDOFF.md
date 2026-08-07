# Handoff — Skylanders Conquest, asset pipeline (2026-08-07, second pass)

## The one thing to know

**The asset extractor was corrupting a third of the pak, and that single bug was
the cause of every open item on the previous handoff.**

NX pak files are chunked at **1 MiB**, not 32 KiB. Both `igArchiveExtractor` and
the first `pak_extract.py` assumed 32 KiB, decoded the first 32 KiB of each file
correctly, then advanced by `9 + csize` — which, because csize covers a whole
1 MiB chunk, **landed inside the next file** and spliced its data in until the
byte count reached the declared size.

Output lengths matched exactly. Nothing reported an error. 807 of 2354 files
(34%) were silently wrong past their first 32 KiB.

The previous handoff ranked four problems and put the vertex attribute schema
first as "the keystone". All four were the same bug:

| previous diagnosis | actual cause |
|---|---|
| "vertex attribute schema undecoded — the keystone" | reading a corrupted file; the schema decoded in an afternoon once the data was right |
| "actors hang the converter — genuine unbounded-scan bug in `IGZ.cpp`" | the scan was walking through *another file's* data. Converter needs no patch |
| "512x512 textures decode wrong, size-dependent" | 256x256 fits in one 32 KiB chunk and survived by accident. Texture code was always correct |
| "statics: 29 of 60 import, converter is the weak link" | corrupt input |

A second, subtler bug hid behind the first: a raw LZMA1 stream has **no
end-of-stream marker**, so a decoder handed the full remaining byte count runs
the last byte's trailing bits out into one spurious extra byte. On
`L505_ferris_wheel` chunk 3 produced 1048577 instead of 1048576, shifting
everything past 4 MiB by one and turning its last two meshes into NaN — while
the file's total length still matched exactly. Every chunk is now capped at
`min(dict_size, remaining)`.

> **A correct output length proved nothing here.** Both bugs produced
> byte-perfect file sizes. Validate content — exactly one IGZ magic at offset 0,
> and every declared vertex buffer decoding to unit-length normals.

## Results

| | before | after |
|---|---|---|
| files extracted, 0 short | — | **2354 / 2354** (~12 s for the whole pak) |
| level_505 statics → FBX | 29 / 60 | **133 / 133**, zero timeouts |
| enemy actors → FBX | 0 (all hung) | **7 / 7** |
| mesh igz with every normal unit-length | 9 / 133 | **417 / 419** |
| large igz with foreign data spliced in | 540 / 570 | **0 / 774** |
| 512x512 textures | broken | all decode |

`L505_ferris_wheel` (4.9 MB) and `L505_interior_mausoleum` (2.6 MB), which never
finished converting even at a 480 s timeout, now convert in seconds and render
as recognisable geometry.

## Pipeline

```bash
# 1. extract  — the C# dumpTable step is GONE; this parses the pak table itself
python Scripts/pak_extract.py <pak> <outDir> [name-filter]

# 2a. textures -> PNG
python Scripts/igz_texture.py <outDir> --glob "<dir>/*SH_Grass*"

# 2b. models -> FBX  (C++ converter; now succeeds on everything)
JOBS=6 TIMEOUT=150 D:/SkylandersExtract/convert_batch.sh <src> <out> skyl mod

# 2c. models -> OBJ  (pure Python, no converter, cannot hang)
python Scripts/igz_mesh.py --obj <outDir> --glob "<dir>/**/*.igz"
python Scripts/igz_mesh.py --info <file.igz>

# 3. FBX -> Unreal  (crash-resumable; forces the legacy importer)
export MSYS_NO_PATHCONV=1 SKY_GEO_DIR=... SKY_GEO_DEST=/Game/...
./Scripts/import_level_geometry.sh
```

**Good tree is `D:/SkylandersExtract/nx9/`.** Delete `nx4`–`nx8`; they are all
corrupt, and **nx7 in particular looks healthy and is not** — it passes every
container check while containing other files' data.

## New tool: Scripts/igz_mesh.py

Reads geometry straight from the object graph in O(objects), so it cannot spin
the way the C++ magic-number scanner does. Full format notes are in
`docs/ASSET-EXTRACTION.md`; the short version:

- `igVertexBuffer`: count +0x44, stride +0x58, format pointer +0x10
- `igIndexBuffer`: count +0x3C, u16 triangle lists
- `igVertexFormat`: attribute table +0x00, component table +0x10, its byte size
  +0x0C. Attribute count is `size/4 - 1`; both tables carry a terminator.
- attribute entries are three u32 — **(type, slot, offset)**. Slot 0 position,
  1 normal, 2 tangent, 4 RGBA8 colour, 5 half2 UV.
- buffers live in a section with no pointers to them, in object order as
  (vertex, index) pairs on 4-byte boundaries. The reader totals them and finds
  the section matching exactly, refusing the file if none does.

## Ground textures need a re-import

The three live ground textures were byte-identical to the **nx7 + `repair_tail`**
output — their bottom ~20 rows were a fabricated mirror, because a 256x256 BC1
top mip straddles the 32768 boundary and its last rows were another file's data.
`repair_tail()` existed only to hide that, and is now **off by default**.

`Saved/GroundTex/{Grass_C,Cobble_C,Dirt_C}.png` have been restaged from `nx9`
with real pixels. **The imported `.uasset`s still hold the old ones** — they need
a re-import in the editor to take effect.

## What is left

Nothing from the previous handoff's list remains open. Two genuinely new, small
items:

1. **`Gen_Crate_Debris_10` and `L505_Icing_Gun` don't lay buffers out in object
   order.** 2 of 419 files. `L505_Icing_Gun` is animated (`CModelAnimation`,
   strides 140/136 — skinned) and starts `vvvv` rather than alternating; the
   crate goes bad at its *second* vertex. Both tile their section exactly, so
   the aggregate check passes and only the per-vertex normal check catches them.
   The reader drops the bad meshes rather than emitting NaN. Neither model is
   used by the game.
2. **The unused meshes are dropped, not recovered.** If those two files are ever
   wanted, the fix is to find the real per-buffer offset field rather than
   inferring order — likely in `igModelDrawCallData` or `igGraphicsVertexBuffer`,
   neither of which is decoded yet.

Unchanged from before: the C++ converter is a Visual Studio C++/CLI project and
the user does not use Visual Studio — but it now needs no patching, so this no
longer blocks anything.

## Verified vs inferred

Verified by measurement this session: the 1 MiB chunk size and the dict_size
cap; the 2354/2354, 133/133, 7/7 and 417/419 counts; the section-3 byte
arithmetic on `L505_bridge` (790376 vertex + 161772 index = 952148 exactly); the
attribute schema, by decoding and rendering 133 statics; textures, by eye on a
contact sheet.

Inferred, not proven: that slot 2 is *tangent* (it is a 3xf32 that is not
position or normal, which is what tangent looks like — nothing depends on the
name); that slot 4 is RGBA8 colour, same reasoning.

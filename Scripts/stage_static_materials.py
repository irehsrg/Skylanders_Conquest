"""Stage a static model for Unreal WITH its materials: OBJ + MTL + PNG textures.

Why this exists
---------------
Every rock, palm and plant in the map renders in the same flat brown because the
statics were imported with no materials at all. That was not laziness: the
converter's FBX carries no `Material::` nodes whatsoever, so however it is
imported the mesh gets exactly one slot.

The game keeps the mapping in `materialInstances/` instead, and all 133
level_505 statics have one. This joins the three readers:

    igz_material.py  ->  slot names + texture refs per material
    igz_texture.py   ->  those refs decoded to PNG
    igz_mesh.py      ->  geometry as OBJ, one `usemtl` per mesh group

OBJ+MTL is the route because Unreal names a material slot per MTL entry, which
is the only way to get more than one slot onto these meshes.

Usage
-----
    python Scripts/stage_static_materials.py <outDir> <model> [model ...]
    python Scripts/stage_static_materials.py <outDir> --all

Env / defaults point at the nx9 tree; override with SKY_NX_DIR.
"""

import glob as globmod
import os
import shutil
import sys

import igz_material
import igz_mesh
import igz_texture

NX = os.environ.get('SKY_NX_DIR',
                    'D:/SkylandersExtract/nx9/Temporary/BuildServer/nx/Output')
STATICS = NX + '/models/LevelAssets/2016/Level_505/Statics'
MATINST = NX + '/materialInstances'
TEXTURES = NX + '/textures'


def find_one(pattern):
    hits = globmod.glob(pattern)
    return hits[0] if hits else None


def stage(model, out_dir):
    igz = os.path.join(STATICS, model + '.igz')
    if not os.path.isfile(igz):
        return 'no such static: ' + model

    mi = find_one(os.path.join(MATINST, '*,' + model + ',*'))
    mats = igz_material.read_materials(mi)['materials'] if mi else []
    by_slot = {m['slot']: m for m in mats}

    # Order the materials the way the DRAW CALLS do, not the way the material
    # instance lists them -- they differ, and pairing mesh N with material N
    # textures L505_trees' trunk green and its leaves brown.
    order = igz_material.model_slot_names(igz, valid=set(by_slot))
    fallback = ''
    if order:
        ordered = [by_slot.get(nm, {'slot': nm}) for nm in order]
    else:
        ordered, fallback = mats, '  [no draw-call names; using list order]'

    # 1. textures -> PNG, one per material slot (colour map only; that is what
    #    kills the uniform brown. Normals can follow once this looks right.)
    slots = []
    for m in ordered:
        png = None
        ref = m.get('color')
        if ref:
            src = igz_material.texture_file(ref, TEXTURES)
            if src:
                try:
                    igz_texture.convert(src, out_dir)
                    # convert() names the PNG after the texture's own asset
                    name = os.path.basename(src).split('!')[-1].split('`')[0]
                    if os.path.isfile(os.path.join(out_dir, name + '.png')):
                        png = name + '.png'
                except Exception as exc:              # noqa: BLE001
                    print('   texture failed for %s: %s' % (m['slot'], exc))
        slots.append((m['slot'], png))

    # 2. geometry -> OBJ, single group so Unreal makes ONE asset, with a
    #    usemtl per mesh group so it still gets one slot per material.
    reader = igz_mesh.MeshReader(open(igz, 'rb').read())
    meshes = reader.buffers()
    good = [m for m in meshes if reader.mesh_is_sane(m)]
    if not good:
        return 'no readable meshes: ' + model
    data = [reader.read_mesh(m) for m in good]

    if not slots:
        slots = [(model + '_Mat', None)]
    igz_mesh.write_mtl(os.path.join(out_dir, model + '.mtl'), slots)
    igz_mesh.write_obj(os.path.join(out_dir, model + '.obj'), model, data,
                       single_group=True, slots=slots)

    textured = sum(1 for _, p in slots if p)
    note = ''
    if len(good) != len(slots):
        note = '  [%d mesh groups vs %d materials]' % (len(good), len(slots))
    return 'OK   %-34s %d meshes, %d slots (%d textured)%s%s' % (
        model, len(good), len(slots), textured, note, fallback)


def main():
    argv = sys.argv[1:]
    if len(argv) < 2:
        print(__doc__)
        return 2
    out_dir = argv[0]
    if not os.path.isdir(out_dir):
        os.makedirs(out_dir)

    if argv[1] == '--all':
        models = sorted(os.path.splitext(os.path.basename(f))[0]
                        for f in globmod.glob(os.path.join(STATICS, '*.igz')))
    else:
        models = argv[1:]

    for m in models:
        try:
            print(stage(m, out_dir))
        except Exception as exc:                      # noqa: BLE001
            print('FAIL %-34s %s' % (m, exc))

    # convert() leaves .dds scratch behind on failure paths; tidy up.
    for junk in globmod.glob(os.path.join(out_dir, '*.dds')):
        os.remove(junk)
    return 0


if __name__ == '__main__':
    sys.exit(main())

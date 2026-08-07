"""Read a materialInstances IGZ: which textures a model uses, per material slot.

Why this exists
---------------
The statics import with no materials at all, so every rock, palm and plant in
the map renders in the same flat brown. `IgzModelConverter` emits FBX with no
`Material::` nodes whatsoever, so the FBX route cannot carry slots -- but the
game ships the mapping separately, in `materialInstances/`, and every one of the
133 level_505 statics has one.

The filename encodes the model: `<area>_materials,<model>,<hash>.igz`.

Inside, the TSTR string table carries the texture references verbatim, and each
one names its own role:

    ColorMap,textures@!levels!2016!Level_505!l505_palmLeaf_C`tif,111
    NormalMap,textures@!levels!2016!Level_505!l505_palmLeaf_N`tif,111
    Composite3,textures@!{...l505_palmTreeTrunk_G`tif...},111

Those strings ARE the texture IGZ filenames (plus `.igz`), so they resolve
straight to files in `textures/` with no lookup table.

Which textures belong to which material comes from OBJECT ORDER, not from the
names -- see `read_materials`. The trailing non-path strings are the material
slot names (`L505_Leaf_01`, `L505_Wood_tree_palm`), one per `igGraphicsMaterial`.

TDEP also lists external dependencies but is incomplete -- for L505_trees it
names 4 of the 6 textures -- so TSTR is the source of truth here.

Usage
-----
    python Scripts/igz_material.py --info <materialInstance.igz>
    python Scripts/igz_material.py --json <out.json> --glob "<dir>/*.igz"
"""

import glob as globmod
import json
import os
import re
import sys

from igz_parse import Igz, IgzError

# Role prefixes as they appear at the start of a texture reference string.
ROLE_COLOR = 'ColorMap'
ROLE_NORMAL = 'NormalMap'
ROLE_COMPOSITE = 'Composite3'
ROLES = (ROLE_COLOR, ROLE_NORMAL, ROLE_COMPOSITE)

# Strings that are engine plumbing, never material slot names.
SHADER_PREFIXES = ('standard__', 'ig_', 'graphics_')
KNOWN_WORDS = ('Standard', 'image')


def is_texture_ref(s):
    return 'textures@' in s and s.split(',', 1)[0] in ROLES


def texture_role(s):
    return s.split(',', 1)[0]


def texture_base(s):
    """Base asset name shared by a material's colour/normal/composite maps.

    `...!l505_palmLeaf_C`tif,111` -> `l505_palmLeaf`. Composite3 references wrap
    several assets in braces, so take the one that carries a real level path.
    """
    best = ''
    for chunk in re.split(r'[{},]', s):
        if '!' not in chunk and '`' not in chunk:
            continue
        name = chunk.split('!')[-1].split('`')[0]
        if not name or name.startswith('default_'):
            continue
        if len(name) > len(best):
            best = name
    if not best:
        return ''
    # strip the single-letter role suffix (_C, _N, _G, _B, _E)
    return re.sub(r'_[A-Z]$', '', best)


def model_name(path):
    """`<area>_materials,<model>,<hash>.igz` -> `<model>`."""
    base = os.path.splitext(os.path.basename(path))[0]
    parts = base.split(',')
    return parts[1] if len(parts) >= 3 else base


def read_materials(path):
    """Split the texture references across materials by OBJECT ORDER.

    Grouping by asset name does not work: a blend material mixes textures with
    unrelated names (1505_rock02's `Rocks_blend` layers `l505_rockFake_01` over
    `SH_Algae01`, and its second slot pulls a snow decal plus a concrete normal
    and gloss). Nothing in the names ties those together.

    The object graph does. Walking objects in offset order, each
    `igGraphicsMaterial` opens a slot and every `igGraphicsTexture` after it
    belongs to that slot, consuming the next texture reference from TSTR. The
    counts line up exactly -- 6 textures / 6 refs for L505_trees, 10 / 10 for
    1505_rock02 -- which is the check `read_materials` reports as `matched`.

    (The RHND handle on each object also encodes this, but its index space
    counts materials as well as textures, so it needs the same walk anyway.)
    """
    igz = Igz(open(path, 'rb').read())

    refs, slots = [], []
    for s in igz.strings:
        if is_texture_ref(s):
            refs.append(s)
        elif (not s.startswith(SHADER_PREFIXES) and s not in KNOWN_WORDS
              and 'textures@' not in s):
            slots.append(s)

    groups = []
    used = 0
    for _, tn, _, _ in sorted(igz.objects()):
        if tn == 'igGraphicsMaterial':
            groups.append([])
        elif tn == 'igGraphicsTexture' and groups:
            if used < len(refs):
                groups[-1].append(refs[used])
            used += 1

    # Slot names are the trailing non-path strings, one per material object.
    n = len(groups)
    names = slots[-n:] if n and len(slots) >= n else slots

    out = []
    for i, g in enumerate(groups):
        by_role = {}
        for s in g:
            by_role.setdefault(texture_role(s), s)      # first of each role wins
        color = by_role.get(ROLE_COLOR)
        out.append({
            'slot': names[i] if i < len(names) else 'Material_%d' % i,
            'asset': texture_base(color) if color else '',
            'color': color,
            'normal': by_role.get(ROLE_NORMAL),
            'composite': by_role.get(ROLE_COMPOSITE),
            'textures': g,
        })
    return {'model': model_name(path), 'materials': out,
            'material_objects': n, 'refs': len(refs),
            'matched': used == len(refs)}


def model_slot_names(model_igz_path, valid=None):
    """Material slot name per draw call, in mesh order, from the MODEL file.

    Do not pair mesh N with material N: the orders differ. L505_trees lists its
    materials leaf-then-trunk, but its draw calls are trunk-then-leaf, so the
    positional guess textures the trunk green and the leaves brown.

    The model's own string table spells the correspondence out. Each draw call
    contributes a `<...>.igb (i/N)` marker, and the slot name follows it -- but
    NOT always immediately: the first marker is also followed by the shared
    material-instance reference, which is listed once for the whole file.

        Data/.../L505_trees.igb (0/2)
        L505_pirate_area_materials,L505_trees,8000000000040000   <- shared ref
        L505_Wood_tree_palm                                      <- mesh 0, trunk
        Data/.../L505_trees.igb (1/2)
        L505_Leaf_01                                             <- mesh 1, leaf

    So scan forward from each marker for the first string that is an actual slot
    name. Pass `valid` (the slot names from the material instance) to make that
    exact; without it, fall back to skipping anything that looks like a path or
    a material-instance reference.

    Returns [] when the file has no markers, in which case the caller should
    fall back to list order and say so.
    """
    igz = Igz(open(model_igz_path, 'rb').read())
    strings = igz.strings
    marker = re.compile(r'\((\d+)/(\d+)\)\s*$')

    def is_slot(s):
        if valid is not None:
            return s in valid
        return not ('/' in s or '.' in s or ',' in s) and bool(s)

    found = {}
    for i, s in enumerate(strings):
        m = marker.search(s)
        if not m:
            continue
        for j in range(i + 1, min(i + 5, len(strings))):
            if marker.search(strings[j]):
                break                       # ran into the next draw call
            if is_slot(strings[j]):
                found[int(m.group(1))] = strings[j]
                break
    if not found:
        return []
    return [found[k] for k in sorted(found)]


def texture_file(ref, texture_dir):
    """Resolve a texture reference string to its IGZ on disk."""
    cand = os.path.join(texture_dir, ref + '.igz')
    return cand if os.path.isfile(cand) else None


def main():
    argv = sys.argv[1:]
    if not argv:
        print(__doc__)
        return 2

    if argv[0] == '--info':
        for p in argv[1:]:
            try:
                info = read_materials(p)
            except (IgzError, IndexError) as e:
                print('%s: %s' % (os.path.basename(p), e))
                continue
            print('%s  (%d materials, %d texture refs, matched=%s)' % (
                info['model'], info['material_objects'], info['refs'], info['matched']))
            for m in info['materials']:
                print('   slot %-28s asset %-24s (%d textures)' % (
                    m['slot'], m['asset'], len(m['textures'])))
                for role in ('color', 'normal', 'composite'):
                    if m[role]:
                        print('        %-10s %s' % (role, m[role][:88]))
        return 0

    if argv[0] == '--json':
        dest = argv[1]
        rest = argv[2:]
        files = sorted(globmod.glob(rest[1], recursive=True)) \
            if rest[:1] == ['--glob'] else rest
        out, bad = {}, 0
        for p in files:
            try:
                info = read_materials(p)
            except (IgzError, IndexError):
                bad += 1
                continue
            out[info['model']] = info['materials']
        with open(dest, 'w') as fh:
            json.dump(out, fh, indent=1)
        print('%d models -> %s   (%d unreadable)' % (len(out), dest, bad))
        return 0

    print(__doc__)
    return 2


if __name__ == '__main__':
    sys.exit(main())

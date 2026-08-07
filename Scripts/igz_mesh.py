"""Read mesh geometry straight out of an Imaginators-NX IGZ and write OBJ.

Why this exists
---------------
`IgzModelConverter` finds geometry by scanning for magic numbers with unbounded
loops, so the largest statics (L505_ferris_wheel at 4.9 MB,
L505_interior_mausoleum at 2.6 MB) never terminate. This walks the object graph
instead, which is O(objects) and cannot spin.

Layout (verified against L505_bridge, then across every static in level_505)
---------------------------------------------------------------------------
Vertex and index data live in a section with no pointers to them: the loader
streams the section into GPU memory in order. Buffers appear in object order,
interleaved as (vertex, index) pairs, each starting on a 4-byte boundary. For
L505_bridge that is

    VB0 2518x40 | IB0 10518x2 | VB1 3629x44 | IB1 15252x2
    VB2 10068x44 | IB2 50454x2 | VB3 1977x44 | IB3 4662x2  = 952148 bytes

which is exactly the size of its last section. Usually that IS the last
section, but not always -- the cloud models keep geometry in a middle one --
so `_assign_offsets` totals the buffers first and then finds the section whose
size matches exactly, and refuses the file if none does.

`igVertexBuffer` (fields at object+0x14): vertex count at +0x44, stride at
+0x58, pointer to its `igVertexFormat` at +0x10.
`igIndexBuffer`: index count at +0x3C. Indices are u16 triangle lists.
`igVertexFormat`: +0x00 points at the attribute table, +0x10 at the component
table, +0x0C is the component table's size in bytes. The attribute count is
`(that size / 4) - 1` -- both tables carry one terminator entry.

Each attribute table entry is three u32: (type, slot, offset-within-vertex).
An attribute's byte size is the next entry's offset minus its own (the last one
runs to the stride). Slot 0 is position and slot 1 is normal; a 4-byte slot 5
is a half2 UV.

Pointers are 8-byte slots whose HIGH dword holds the encoded location:
section index in the top 5 bits, byte offset in the low 27.

Usage
-----
    python Scripts/igz_mesh.py --info  <file.igz>
    python Scripts/igz_mesh.py --obj <outDir> <file.igz> [more.igz ...]
    python Scripts/igz_mesh.py --obj <outDir> --glob "<dir>/*.igz"
"""

import glob as globmod
import os
import struct
import sys

from igz_parse import Igz, IgzError

SLOT_POSITION = 0
SLOT_NORMAL = 1
SLOT_UV = 5

# igVertexBuffer / igIndexBuffer / igVertexFormat field offsets, relative to
# the object's field block (object + 0x14).
VB_FORMAT_PTR = 0x10
VB_COUNT = 0x44
VB_STRIDE = 0x58
IB_COUNT = 0x3C
VF_ATTR_PTR = 0x00
VF_COMP_SIZE = 0x0C
VF_COMP_PTR = 0x10

FIELDS = 0x14           # object header length; fields start here
INDEX_SIZE = 2          # u16 triangle lists


class MeshError(Exception):
    pass


def _half(u):
    """IEEE 754 binary16 -> float."""
    s = -1.0 if u >> 15 else 1.0
    e = (u >> 10) & 0x1F
    m = u & 0x3FF
    if e == 0:
        return s * (m / 1024.0) * (2.0 ** -14)
    if e == 31:
        return 0.0                      # inf/nan never appear in real UVs
    return s * (1.0 + m / 1024.0) * (2.0 ** (e - 15))


class MeshReader(object):
    def __init__(self, data):
        self.igz = Igz(data)
        self.d = self.igz.d
        self.descs = self.igz.descriptors
        if len(self.descs) < 2:
            raise MeshError('no data sections')

    # ---- primitives ----
    def _u32(self, o):
        return struct.unpack_from('<I', self.d, o)[0]

    def _deref(self, encoded):
        """Encoded location -> absolute file offset."""
        sec, within = encoded >> 27, encoded & 0x07FFFFFF
        if sec + 1 >= len(self.descs):
            raise MeshError('pointer to section %d (only %d)' % (sec, len(self.descs)))
        base, size = self.descs[sec + 1]
        if within > size:
            raise MeshError('pointer past end of section %d' % sec)
        return base + within

    def _ptr(self, field_base, rel):
        """An 8-byte pointer slot; the high dword carries the location."""
        hi = self._u32(field_base + rel + 4)
        return self._deref(hi) if hi else None

    # ---- vertex format ----
    def _read_format(self, obj_off):
        f = obj_off + FIELDS
        attr_at = self._ptr(f, VF_ATTR_PTR)
        comp_size = self._u32(f + VF_COMP_SIZE)
        if attr_at is None or comp_size < 8:
            raise MeshError('vertex format has no attribute table')
        n = comp_size // 4 - 1          # both tables carry a terminator
        attrs = []
        for i in range(n):
            t, slot, off = struct.unpack_from('<3I', self.d, attr_at + i * 12)
            attrs.append({'type': t, 'slot': slot, 'offset': off})
        return attrs

    @staticmethod
    def _size_attrs(attrs, stride):
        """Fill in each attribute's byte size from the next one's offset."""
        ordered = sorted(attrs, key=lambda a: a['offset'])
        for i, a in enumerate(ordered):
            nxt = ordered[i + 1]['offset'] if i + 1 < len(ordered) else stride
            a['size'] = nxt - a['offset']
        return ordered

    # ---- buffers ----
    def buffers(self):
        """Vertex/index buffers in data order, with their section offsets."""
        vbs, ibs = [], []
        for off, tn, _, _ in self.igz.objects():
            f = off + FIELDS
            if tn == 'igVertexBuffer':
                stride = self._u32(f + VB_STRIDE)
                count = self._u32(f + VB_COUNT)
                fmt_obj = self._ptr(f, VB_FORMAT_PTR)
                attrs = self._read_format(fmt_obj) if fmt_obj else []
                vbs.append({'obj': off, 'kind': 'v', 'count': count,
                            'stride': stride, 'bytes': count * stride,
                            'attrs': self._size_attrs(attrs, stride)})
            elif tn == 'igIndexBuffer':
                count = self._u32(f + IB_COUNT)
                ibs.append({'obj': off, 'kind': 'i', 'count': count,
                            'stride': INDEX_SIZE, 'bytes': count * INDEX_SIZE})
        return self._assign_offsets(vbs, ibs)

    def _assign_offsets(self, vbs, ibs):
        """Walk buffers in object order and lay them out back-to-back.

        Buffers start on 4-byte boundaries, so an index buffer with an odd
        count is followed by 2 bytes of padding.

        Which section holds the data is not stated anywhere, and it is not
        always the last one -- the cloud models keep geometry in a middle
        section. So compute the total first and then look for the section it
        fits exactly. Requiring an exact match is what makes this safe: if no
        section matches, the layout assumption is wrong for this file and we
        refuse rather than emit plausible-looking garbage.
        """
        if not vbs:
            return []
        allb = sorted(vbs + ibs, key=lambda b: b['obj'])
        pos = 0
        for b in allb:
            b['offset'] = pos
            pos = (pos + b['bytes'] + 3) // 4 * 4
        total = pos

        match = None
        for off, size in reversed(self.descs[1:]):
            if size == total:
                match = (off, size)
                break
        if match is None:
            raise MeshError('buffer bytes %d match no section (%s)' % (
                total, ', '.join(str(s) for _, s in self.descs[1:])))
        self.data_off, self.data_size = match
        for b in allb:
            b['data'] = self.data_off + b['offset']
        # Pair each vertex buffer with the index buffer that follows it.
        meshes = []
        for i, b in enumerate(allb):
            if b['kind'] != 'v':
                continue
            ib = allb[i + 1] if i + 1 < len(allb) and allb[i + 1]['kind'] == 'i' else None
            meshes.append({'vb': b, 'ib': ib})
        return meshes

    # ---- validation ----
    def mesh_is_sane(self, mesh):
        """Check every vertex of a mesh before trusting its offset.

        The section-size match in `_assign_offsets` is an aggregate check: it
        confirms the buffers fill the section, not that each one starts where
        we think. Two files in level_505 (Gen_Crate_Debris_10, L505_Icing_Gun)
        tile the section exactly yet do not lay their buffers out in object
        order, so their later meshes land mid-vertex and decode to NaN.

        Normals are the tell: a correctly located buffer has unit-length ones.
        Every vertex gets checked, not a sample -- Gen_Crate_Debris_10 goes bad
        at its *second* vertex and a stride-2 sample walked straight past it.
        The export reads all these vertices anyway, so this costs nothing new.
        """
        vb = mesh['vb']
        nrm = [a for a in vb['attrs'] if a['slot'] == SLOT_NORMAL and a['size'] >= 12]
        if not nrm or vb['count'] == 0:
            return True                 # nothing to check against
        aoff = nrm[0]['offset']
        for i in range(vb['count']):
            o = vb['data'] + i * vb['stride'] + aoff
            if o + 12 > len(self.d):
                return False
            x, y, z = struct.unpack_from('<3f', self.d, o)
            if x != x or y != y or z != z:          # NaN
                return False
            if abs(x * x + y * y + z * z - 1.0) > 0.05:
                return False
        return True

    # ---- geometry ----
    def read_mesh(self, mesh):
        vb, ib = mesh['vb'], mesh['ib']
        by_slot = {a['slot']: a for a in vb['attrs']}
        pos_a = by_slot.get(SLOT_POSITION)
        nrm_a = by_slot.get(SLOT_NORMAL)
        uv_a = by_slot.get(SLOT_UV)
        if pos_a is None or pos_a['size'] < 12:
            raise MeshError('no float3 position attribute')

        positions, normals, uvs = [], [], []
        base, stride = vb['data'], vb['stride']
        for i in range(vb['count']):
            v = base + i * stride
            positions.append(struct.unpack_from('<3f', self.d, v + pos_a['offset']))
            if nrm_a is not None and nrm_a['size'] >= 12:
                normals.append(struct.unpack_from('<3f', self.d, v + nrm_a['offset']))
            if uv_a is not None and uv_a['size'] == 4:
                a, b = struct.unpack_from('<2H', self.d, v + uv_a['offset'])
                uvs.append((_half(a), _half(b)))

        tris = []
        if ib is not None and ib['count'] >= 3:
            n = ib['count'] - ib['count'] % 3
            idx = struct.unpack_from('<%dH' % n, self.d, ib['data'])
            tris = [idx[i:i + 3] for i in range(0, n, 3)]
        return positions, normals, uvs, tris


def write_obj(path, name, meshes_data):
    """OBJ with one group per mesh. Unreal imports this directly."""
    with open(path, 'w') as fh:
        fh.write('# %s -- extracted by Scripts/igz_mesh.py\n' % name)
        vbase = 1
        for n, (positions, normals, uvs, tris) in enumerate(meshes_data):
            fh.write('g %s_%d\n' % (name, n))
            for p in positions:
                fh.write('v %.6f %.6f %.6f\n' % p)
            for t in uvs:
                fh.write('vt %.6f %.6f\n' % (t[0], 1.0 - t[1]))
            for v in normals:
                fh.write('vn %.6f %.6f %.6f\n' % v)
            has_uv, has_n = bool(uvs), bool(normals)
            for tri in tris:
                parts = []
                for i in tri:
                    j = vbase + i
                    if has_uv and has_n:
                        parts.append('%d/%d/%d' % (j, j, j))
                    elif has_n:
                        parts.append('%d//%d' % (j, j))
                    elif has_uv:
                        parts.append('%d/%d' % (j, j))
                    else:
                        parts.append('%d' % j)
                fh.write('f %s\n' % ' '.join(parts))
            vbase += len(positions)


def do_info(path):
    r = MeshReader(open(path, 'rb').read())
    meshes = r.buffers()
    print('%s' % os.path.basename(path))
    print("  data section: offset %d size %d" % (r.data_off, r.data_size))
    for n, m in enumerate(meshes):
        vb, ib = m['vb'], m['ib']
        print('  mesh %d: %d verts stride %d @%d | %s' % (
            n, vb['count'], vb['stride'], vb['data'],
            ('%d indices @%d (%d tris)' % (ib['count'], ib['data'], ib['count'] // 3))
            if ib else 'no index buffer'))
        for a in vb['attrs']:
            print('      slot %-2d type %-3d offset %-3d size %d'
                  % (a['slot'], a['type'], a['offset'], a['size']))


def main():
    argv = sys.argv[1:]
    if not argv:
        print(__doc__)
        return 2

    if argv[0] == '--info':
        rc = 0
        for p in argv[1:]:
            try:
                do_info(p)
            except (MeshError, IgzError) as e:
                print('%s: %s' % (os.path.basename(p), e))
                rc = 1
        return rc

    if argv[0] == '--obj':
        out_dir = argv[1]
        rest = argv[2:]
        if rest[:1] == ['--glob']:
            files = sorted(globmod.glob(rest[1], recursive=True))
        else:
            files = rest
        if not os.path.isdir(out_dir):
            os.makedirs(out_dir)
        ok = fail = partial = 0
        for p in files:
            name = os.path.splitext(os.path.basename(p))[0]
            try:
                r = MeshReader(open(p, 'rb').read())
                meshes = r.buffers()
                if not meshes:
                    print('SKIP %-44s no vertex buffers' % name[:44])
                    continue
                good = [m for m in meshes if r.mesh_is_sane(m)]
                dropped = len(meshes) - len(good)
                if not good:
                    print('FAIL %-44s all %d meshes failed the sanity check'
                          % (name[:44], len(meshes)))
                    fail += 1
                    continue
                data = [r.read_mesh(m) for m in good]
                dest = os.path.join(out_dir, name + '.obj')
                write_obj(dest, name, data)
                nv = sum(len(d[0]) for d in data)
                nt = sum(len(d[3]) for d in data)
                note = '  (dropped %d unreadable)' % dropped if dropped else ''
                print('OK   %-44s %d meshes %d verts %d tris%s'
                      % (name[:44], len(data), nv, nt, note))
                ok += 1
                if dropped:
                    partial += 1
            except (MeshError, IgzError, struct.error) as e:
                print('FAIL %-44s %s' % (name[:44], e))
                fail += 1
        print('\nok: %d   failed: %d' % (ok, fail))
        return 1 if fail else 0

    print(__doc__)
    return 2


if __name__ == '__main__':
    sys.exit(main())

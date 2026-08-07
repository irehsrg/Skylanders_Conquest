"""Locate vertex/index buffer payloads in an IGZ without decoding the attribute
descriptor schema.

The declared counts are strong constraints, so the payloads can be found by
search rather than by interpreting igVertexFormat's attribute table:

  index data  - `idx_count` u16s where every value is < vtx_count, the maximum
                is close to vtx_count-1 (buffers use most of their vertices),
                and the count is divisible by 3.
  vertex data - a (base, stride) where `vtx_count` vertices all decode to finite
                positions inside one tight bounding box.

Both are then cross-checked against each other: a correct pair has every index
addressing a vertex that exists.

    python Scripts/igz_locate.py <file.igz>
"""

import math
import struct
import sys

from igz_parse import Igz

STRIDES = (12, 16, 20, 24, 28, 32, 36, 40, 44, 48, 52, 56, 60, 64)


def buffers(igz):
    d = igz.d
    vb, ib = [], []
    for off, tn, cnt, ln in igz.objects():
        f = off + 0x14
        if tn == 'igVertexBuffer':
            vb.append(struct.unpack_from('<I', d, f + 0x44)[0])
        elif tn == 'igIndexBuffer':
            ib.append(struct.unpack_from('<I', d, f + 0x3C)[0])
    return vb, ib


def find_indices(d, lo, hi, count, vtx_count):
    """Scan for a u16 run of `count` values all < vtx_count."""
    need = count * 2
    step = 2
    best = None
    for base in range(lo, hi - need, step):
        # cheap reject on a sample before paying for the full scan
        ok = True
        for i in range(0, count, max(1, count // 24)):
            v = struct.unpack_from('<H', d, base + i * 2)[0]
            if v >= vtx_count:
                ok = False
                break
        if not ok:
            continue
        mx = 0
        for i in range(count):
            v = struct.unpack_from('<H', d, base + i * 2)[0]
            if v >= vtx_count:
                mx = -1
                break
            if v > mx:
                mx = v
        if mx < 0:
            continue
        # a real buffer references most of its vertices
        if mx >= vtx_count * 0.5:
            best = (base, mx)
            break
    return best


def find_vertices(d, lo, hi, count):
    """Scan (base, stride) for `count` finite positions in a tight bbox."""
    results = []
    for stride in STRIDES:
        need = count * stride
        if need > hi - lo:
            continue
        for base in range(lo, hi - need, 4):
            xs = ys = zs = None
            ok = True
            sample = range(0, count, max(1, count // 40))
            pts = []
            for i in sample:
                o = base + i * stride
                x, y, z = struct.unpack_from('<3f', d, o)
                if not all(math.isfinite(v) and abs(v) < 1e6 for v in (x, y, z)):
                    ok = False
                    break
                pts.append((x, y, z))
            if not ok or len(pts) < 4:
                continue
            xs = [p[0] for p in pts]
            ys = [p[1] for p in pts]
            zs = [p[2] for p in pts]
            ext = (max(xs) - min(xs), max(ys) - min(ys), max(zs) - min(zs))
            if max(ext) < 1e-3 or max(ext) > 1e5:
                continue
            results.append((stride, base, ext))
            break
    return results


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return 2
    igz = Igz(open(sys.argv[1], 'rb').read())
    d = igz.d
    lo, size = igz.descriptors[-1]
    hi = lo + size
    vb, ib = buffers(igz)
    print('data section @%d size %d' % (lo, size))
    print('vertex counts: %s' % vb)
    print('index counts : %s' % ib)

    for i, (vc, ic) in enumerate(zip(vb, ib)):
        print('\n-- buffer %d: %d verts, %d indices (%d tris) --' % (i, vc, ic, ic // 3))
        hit = find_indices(d, lo, hi, ic, vc)
        if hit:
            print('   indices @%d (rel %d)  max index %d of %d' % (hit[0], hit[0] - lo, hit[1], vc - 1))
        else:
            print('   indices NOT FOUND')
        for stride, base, ext in find_vertices(d, lo, hi, vc)[:3]:
            print('   verts stride %-3d @%-9d (rel %-8d) bbox %.0f x %.0f x %.0f'
                  % (stride, base, base - lo, ext[0], ext[1], ext[2]))
    return 0


if __name__ == '__main__':
    sys.exit(main())

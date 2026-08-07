"""Extract textures from Imaginators-NX IGZ to PNG.

The payload is plain **linear** BC1/BC3 - not Tegra block-linear, despite the
`dxt1_nx` format name. Every block-linear variant decodes to noise; a straight
linear read gives the image. (The earlier conclusion that it must be swizzled
came from testing a 512x512 texture where the padded and unpadded mip-chain
sizes differ; on a 256x256 they coincide, which made the linear read obvious.)

Dimensions come from the `igImage2` object rather than being inferred:
on 64-bit (Switch) IGZ the width/height/depth/mip/array block sits at
`object + 0x48`, not `+0x34` - see docs/ASSET-EXTRACTION.md.

    python Scripts/igz_texture.py <out_dir> <file.igz> [more.igz ...]
    python Scripts/igz_texture.py <out_dir> --glob "<dir>/*SH_Grass*"
"""

import glob as globmod
import os
import struct
import sys

from PIL import Image

from igz_parse import Igz

BC1, BC3 = 'DXT1', 'DXT5'


def dds_header(w, h, fourcc, payload_len):
    hdr = b'DDS ' + struct.pack('<IIIIIII', 124, 0x1007, h, w, payload_len, 0, 0)
    hdr += b'\0' * 44
    hdr += struct.pack('<II', 32, 0x4) + fourcc.encode('ascii')
    hdr += struct.pack('<IIIII', 0, 0, 0, 0, 0)
    hdr += struct.pack('<IIIII', 0x1000, 0, 0, 0, 0)
    return hdr


def image_info(igz):
    """(width, height, mips) from the first igImage2 object."""
    for off, tn, cnt, ln in igz.objects():
        if tn == 'igImage2':
            w, h, depth, mips, array = struct.unpack_from('<5H', igz.d, off + 0x48)
            return w, h, mips
    return None


def payload(igz):
    """The largest declared section is the pixel data."""
    best = (0, 0)
    for o, s in igz.descriptors:
        if s > best[1]:
            best = (o, s)
    return best


def mip_chain(w, h, block_bytes, levels):
    total = 0
    for _ in range(levels):
        total += max(1, (w + 3) // 4) * max(1, (h + 3) // 4) * block_bytes
        w = max(1, w // 2)
        h = max(1, h // 2)
    return total


def convert(path, out_dir):
    igz = Igz(open(path, 'rb').read())
    info = image_info(igz)
    if not info:
        return '%s: no igImage2' % os.path.basename(path)
    w, h, mips = info
    off, size = payload(igz)

    # Pick the block size whose full chain matches the declared payload.
    for block_bytes, fourcc in ((8, BC1), (16, BC3)):
        if abs(mip_chain(w, h, block_bytes, mips) - size) <= 16:
            break
    else:
        return '%s: %dx%d mips=%d payload=%d - no BC1/BC3 chain fits' % (
            os.path.basename(path), w, h, mips, size)

    main = max(1, (w + 3) // 4) * max(1, (h + 3) // 4) * block_bytes
    blob = igz.d[off:off + main]
    if len(blob) < main:
        return '%s: payload short (%d/%d)' % (os.path.basename(path), len(blob), main)

    name = os.path.splitext(os.path.basename(path))[0]
    repaired = 0
    # rip filenames are mangled: ColorMap,textures@!levels!...!SH_Grass01_C`tga,111
    if '!' in name:
        name = name.split('!')[-1].split('`')[0]
    dds = os.path.join(out_dir, name + '.dds')
    with open(dds, 'wb') as fh:
        fh.write(dds_header(w, h, fourcc, main) + blob)
    png = os.path.join(out_dir, name + '.png')
    # RGB, not RGBA: these are opaque ground surfaces, and DXT1's 1-bit alpha
    # otherwise punches transparent texels through that read as black in the
    # unlit ground material.
    im = Image.open(dds).convert('RGB')
    os.remove(dds)

    # A few textures carry noise in their last block rows (L505_Cobblestone_01__C
    # ends with 8 such rows). The mip-chain arithmetic is exact and no offset
    # shift removes them, so treat it as a tail defect: overwrite the affected
    # rows with the last clean row. Invisible on these stone/ground textures and
    # it keeps the image tileable, which cropping would not.
    repaired = repair_tail(im)

    im.save(png)
    return 'OK  %-30s %dx%d %s mips=%d%s -> %s.png' % (
        os.path.basename(path)[:30], w, h, fourcc, mips,
        ('  repaired %d rows' % repaired) if repaired else '', name)


def repair_tail(im):
    """Replace trailing defective rows with a copy of the last clean row.

    Two distinct defects appear at the bottom of some textures and they need
    different tests: NOISE rows (high horizontal delta) and DEAD rows (near
    black, which have *low* delta and so slip past a noise-only check -- these
    are what produced black stripes across the lane).
    """
    w, h = im.size
    px = im.load()

    def stats(y):
        rough = 0
        lum = 0
        for x in range(0, w - 1, 2):
            a, b = px[x, y], px[x + 1, y]
            rough += abs(a[0] - b[0]) + abs(a[1] - b[1]) + abs(a[2] - b[2])
            lum += a[0] + a[1] + a[2]
        n = max(1, w // 2)
        return rough / n, lum / (3.0 * n)

    rows = [stats(y) for y in range(h)]
    mid_rough = sorted(r for r, _ in rows)[h // 2]
    mid_lum = sorted(l for _, l in rows)[h // 2]
    rough_limit = mid_rough * 2.5 + 3
    dead_limit = max(6.0, mid_lum * 0.25)

    def bad(y):
        rough, lum = rows[y]
        return rough > rough_limit or lum < dead_limit

    # Repair every bad row in the tail region, not just a run reaching the last
    # row: on some textures the defect stops a few rows short of the bottom, and
    # a bottom-up scan bails out immediately when the final row happens to be OK.
    tail_start = h - max(4, h // 6)
    bad_rows = [y for y in range(tail_start, h) if bad(y)]
    if not bad_rows:
        return 0
    first = min(bad_rows)
    while first > 0 and bad(first - 1):
        first -= 1
    # Back off further: the defect fades out rather than stopping cleanly, so
    # rows just above the detection threshold are still slightly corrupt and
    # would be mirrored back down as visible dashes.
    first = max(1, first - 8)
    if first <= 0:
        return 0
    # MIRROR the band immediately above rather than replicating one row. A single
    # source row carries its own defective spans, and stamping it down the whole
    # band streaks them into dark rectangles at every tile corner.
    n = h - first
    for i in range(n):
        src_y = max(0, first - 1 - i)
        for x in range(w):
            px[x, first + i] = px[x, src_y]
    return n


def main():
    if len(sys.argv) < 3:
        print(__doc__)
        return 2
    out_dir = sys.argv[1]
    if not os.path.isdir(out_dir):
        os.makedirs(out_dir)
    if sys.argv[2] == '--glob':
        files = globmod.glob(sys.argv[3])
    else:
        files = sys.argv[2:]
    for f in files:
        try:
            print(convert(f, out_dir))
        except Exception as exc:                       # noqa: BLE001
            print('ERR %-30s %s' % (os.path.basename(f)[:30], exc))
    return 0


if __name__ == '__main__':
    sys.exit(main())

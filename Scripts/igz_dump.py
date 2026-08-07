"""Print the object graph of an IGZ file. Diagnostic entry point for igz_parse.

    python Scripts/igz_dump.py <file.igz> [--objects]
"""

import collections
import struct
import sys

from igz_parse import Igz


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return 2
    path = sys.argv[1]
    show_objects = '--objects' in sys.argv

    igz = Igz(open(path, 'rb').read())
    print('%s' % path.split('/')[-1].split('\\')[-1])
    print('  version %d  endian %s  descriptors %d' % (
        igz.version, 'LE' if igz.endian == '<' else 'BE', len(igz.descriptors)))
    for i, (o, s) in enumerate(igz.descriptors):
        print('    section %d: offset %-9d size %d' % (i, o, s))
    print('  fixups: %s' % ', '.join(
        '%s(%d)' % (t, f.count) for t, f in sorted(igz.fixups.items())))
    print('  strings %d   type names %d' % (len(igz.strings), len(igz.type_names)))

    objs = igz.objects()
    print('  objects: %d' % len(objs))
    tally = collections.Counter(t for _, t, _, _ in objs)
    for name, n in tally.most_common():
        print('     %-40s x%d' % (name, n))

    if show_objects:
        print('  --- detail ---')
        for off, tn, cnt, ln in objs:
            print('     @%-9d %-40s items=%-6d len=%d' % (off, tn, cnt, ln))

    # --fields <TypeName>: hexdump the field block of each object of that type.
    # Object fields start at offset+0x14 (matching igObject.ReadObjectFields).
    if '--fields' in sys.argv:
        want = sys.argv[sys.argv.index('--fields') + 1]
        print('  --- fields of %s ---' % want)
        for off, tn, cnt, ln in objs:
            if tn != want:
                continue
            base = off + 0x14
            blob = igz.d[base:base + ln]
            print('   @%d items=%d len=%d' % (off, cnt, ln))
            for r in range(0, min(len(blob), 128), 16):
                row = blob[r:r + 16]
                hexs = ' '.join('%02X' % b for b in row)
                u32s = ' '.join('%10d' % v for v in
                                struct.unpack_from('<%dI' % (len(row) // 4), row, 0)) \
                    if len(row) % 4 == 0 else ''
                print('     +%03X  %-47s %s' % (r, hexs, u32s))
    return 0


if __name__ == '__main__':
    sys.exit(main())

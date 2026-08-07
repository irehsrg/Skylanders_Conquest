"""Extract a Skylanders Imaginators (Switch/NX) .pak, correctly.

Why this exists
---------------
`igArchiveExtractor` silently truncated 75 of 133 level_505 statics. Its LZMA
chunk loop assumes every chunk decodes to the full 32 KiB chunk size and asks
the bundled SevenZip decoder for exactly that many bytes. Chunks are variable:
one real chunk in `l505_yeti_right_Arm.igz` decodes to 2704 bytes. The decoder
then runs out of input, throws "Data Error", writes NOTHING, and the loop
`break`s -- abandoning the rest of the file at a 32 KiB boundary.

That truncation is the root cause of the downstream damage: the model converter
read past the end of short files (infinite loop on actors) or decoded cut-off
vertex buffers (NaN positions, "all polygons are degenerate" on import).

Python's lzma returns partial output instead of throwing, so the same walk just
works: accumulate what each chunk actually produces, and cap the last one at the
remaining byte count.

Usage
-----
    # once per pak, to get the file table out of the C# tool:
    igArchiveExtractor.exe dumpTable switch si <pak> <table.csv>

    python Scripts/pak_extract.py <pak> <table.csv> <outDir> [name-filter]

`name-filter` is an optional substring; only matching entries are extracted.
"""

import lzma
import os
import struct
import sys

CHUNK = 0x8000          # nominal uncompressed chunk size
# The compression scheme is the TOP byte of `mode`; the low 24 bits are the
# chunk-properties offset (see IGA_File.cs: `switch (mode >> 24)`).
MODE_LZMA = 0x20


def lzma_filters(props):
    """LZMA1 filter spec from the 5-byte properties blob."""
    d = props[0]
    lc = d % 9
    rest = d // 9
    lp = rest % 5
    pb = rest // 5
    return [{'id': lzma.FILTER_LZMA1, 'lc': lc, 'lp': lp, 'pb': pb,
             'dict_size': struct.unpack('<I', props[1:5])[0]}]


def extract_one(pak, start, size, align):
    """Walk the chunk list at `start`, returning `size` decompressed bytes."""
    out = bytearray()
    pos = start
    while len(out) < size:
        pak.seek(pos)
        head = pak.read(9)
        if len(head) < 9:
            break
        csize = struct.unpack_from('<I', head, 0)[0]
        props = head[4:9]
        if csize == 0 or csize > 0x400000 or props[0] >= 225:
            break
        data = pak.read(csize)
        want = min(CHUNK, size - len(out))
        try:
            dec = lzma.LZMADecompressor(format=lzma.FORMAT_RAW,
                                        filters=lzma_filters(props))
            # Short chunks are normal; lzma returns what it has rather than
            # throwing, which is exactly the behaviour the C# decoder lacks.
            out += dec.decompress(data, want)
        except lzma.LZMAError:
            break
        end = pos + 9 + csize
        pos = (end + align - 1) // align * align
    return bytes(out)


def main():
    if len(sys.argv) < 4:
        print(__doc__)
        return 2
    pak_path, table_path, out_dir = sys.argv[1], sys.argv[2], sys.argv[3]
    name_filter = sys.argv[4] if len(sys.argv) > 4 else None

    align = 32
    entries = []
    with open(table_path, encoding='utf-8') as fh:
        for line in fh:
            line = line.rstrip('\n')
            if line.startswith('#'):
                if line.startswith('#chunkAlignment='):
                    align = int(line.split('=', 1)[1])
                continue
            if not line:
                continue
            # name may contain commas, so split only the first three fields
            start, size, mode, name = line.split(',', 3)
            entries.append((int(start), int(size), int(mode), name))

    if name_filter:
        entries = [e for e in entries if name_filter in e[3]]

    print('%d entries, chunkAlignment %d' % (len(entries), align))
    ok = short = skipped = 0
    with open(pak_path, 'rb') as pak:
        for start, size, mode, name in entries:
            if (mode >> 24) != MODE_LZMA:
                skipped += 1
                continue
            blob = extract_one(pak, start, size, align)
            dest = os.path.join(out_dir, name.replace('/', os.sep))
            d = os.path.dirname(dest)
            if d and not os.path.isdir(d):
                os.makedirs(d)
            with open(dest, 'wb') as fh:
                fh.write(blob)
            if len(blob) == size:
                ok += 1
            else:
                short += 1
                print('  SHORT %-70s %d/%d' % (name[-70:], len(blob), size))
    print('\ncomplete: %d   short: %d   skipped(non-LZMA): %d' % (ok, short, skipped))
    return 0


if __name__ == '__main__':
    sys.exit(main())

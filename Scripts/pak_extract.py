"""Extract a Skylanders Imaginators (Switch/NX) .pak, correctly.

Why this exists
---------------
Files in an NX .pak are chunked at **1 MiB**, not 32 KiB. Each chunk is a raw
LZMA1 stream introduced by a 4-byte compressed size and 5 bytes of LZMA
properties; the chunk's uncompressed size is the dictionary size carried in
those properties. Most files are smaller than one chunk, so they are a single
stream spanning the whole file.

Both `igArchiveExtractor` and the first version of this script assumed 32 KiB
chunks and asked the decoder for at most 0x8000 bytes. That yields the first
32 KiB correctly and then walks `start + 9 + csize` -- which, because csize
covers a whole 1 MiB chunk, overshoots into the *next file's* data. The loop
then keeps consuming other files' streams until the byte count happens to
reach `size`.

The result looked healthy (output length matched the declared size exactly) but
every file larger than 32 KiB was corrupt after its first 32 KiB, with other
files' data spliced in at 0x8000 boundaries. In level_505 that is 807 of 2354
files (34%). Files at or under 32 KiB were unaffected, which is why 256x256
textures decoded perfectly and 512x512 ones did not, and why roughly half the
statics imported while the rest produced degenerate geometry.

Walking real 1 MiB chunks fixes it: L505_bridge.igz then contains exactly one
IGZ magic (at offset 0) and all four of its vertex buffers decode to precisely
their declared vertex counts with unit-length normals. See `chunk_size` for the
second, subtler bug this exposed.

The file table is parsed straight from the pak, so the `dumpTable` step of the
C# tool is no longer needed. A CSV table is still accepted for other archive
versions whose header layout this script does not know.

Usage
-----
    python Scripts/pak_extract.py <pak> <outDir> [name-filter]
    python Scripts/pak_extract.py <pak> <table.csv> <outDir> [name-filter]

`name-filter` is an optional substring; only matching entries are extracted.
"""

import lzma
import os
import struct
import sys

MODE_LZMA = 0x20        # compression scheme lives in the TOP byte of `mode`
MODE_NONE = 0xFF        # stored uncompressed
HEADER = 9              # 4-byte compressed size + 5-byte LZMA properties

# Header field offsets for the Imaginators NX layout (archive version 0x0C),
# mirroring IGA_Structure.headerData[SkylandersImaginatorsNX].
NX_VERSION = 0x0C
NX_NUM_FILES = 0x0C
NX_CHUNK_ALIGN = 0x10
NX_NAMETABLE_LOC = 0x28
NX_CHECKSUM_LOC = 0x38
NX_CHECKSUM_LEN = 0x04
NX_LOCAL_HEADER_LEN = 0x10
NX_LOCAL_START = 0x00
NX_LOCAL_SIZE = 0x08
NX_LOCAL_MODE = 0x0C


class PakError(Exception):
    pass


def lzma_filters(props):
    """LZMA1 filter spec from the 5-byte properties blob."""
    d = props[0]
    lc = d % 9
    rest = d // 9
    lp = rest % 5
    pb = rest // 5
    return [{'id': lzma.FILTER_LZMA1, 'lc': lc, 'lp': lp, 'pb': pb,
             'dict_size': struct.unpack('<I', props[1:5])[0]}]


def chunk_size(props):
    """Uncompressed bytes per chunk: the encoder used the dictionary size.

    This matters for more than efficiency. A raw LZMA1 stream carries no
    end-of-stream marker, so a decoder handed the whole remaining byte count
    will run the trailing bits of the last byte out into one spurious extra
    byte. On L505_ferris_wheel that made chunk 3 yield 1048577 instead of
    1048576, shifting every byte after the 4 MiB mark by one -- enough to turn
    the last two meshes into NaN soup while the file's total length still
    matched exactly. Capping each chunk at its real size drops the stray byte.
    """
    return struct.unpack('<I', props[1:5])[0]


def read_table(d):
    """Parse the pak's own file table. Returns [(start, size, mode, name)]."""
    magic, version = struct.unpack_from('<II', d, 0)
    if magic != 0x1A414749:
        raise PakError('not an IGA archive (magic %08X)' % magic)
    if version != NX_VERSION:
        raise PakError('archive version %d is not the Imaginators NX layout '
                       '(0x0C); pass a table.csv instead' % version)

    def u32(o):
        return struct.unpack_from('<I', d, o)[0]

    n = u32(NX_NUM_FILES)
    nametable = u32(NX_NAMETABLE_LOC)
    local = NX_CHECKSUM_LOC + NX_CHECKSUM_LEN * n

    entries = []
    for i in range(n):
        base = local + NX_LOCAL_HEADER_LEN * i
        start = u32(base + NX_LOCAL_START)
        size = u32(base + NX_LOCAL_SIZE)
        mode = u32(base + NX_LOCAL_MODE)
        noff = nametable + u32(nametable + 4 * i)
        end = d.index(b'\0', noff)
        entries.append((start, size, mode, d[noff:end].decode('latin-1')))

    # Sanity-check before trusting it: a wrong layout produces nonsense here.
    bad = [e for e in entries if e[0] + HEADER > len(d) or e[1] == 0]
    if bad:
        raise PakError('parsed table looks wrong (%d entries out of range); '
                       'pass a table.csv instead' % len(bad))
    return entries


def read_table_csv(path):
    align = 32
    entries = []
    with open(path, encoding='utf-8') as fh:
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
    return entries, align


def extract_one(d, start, size, limit, align):
    """Decompress one archive entry.

    `limit` is the next entry's starting address. Nothing beyond it is ever
    read, so a malformed stream can truncate a file but can never splice
    another file's data into it -- the failure mode that made corrupt output
    indistinguishable from good output.
    """
    out = bytearray()
    pos = start
    while len(out) < size and pos + HEADER <= limit:
        csize = struct.unpack_from('<I', d, pos)[0]
        props = d[pos + 4:pos + HEADER]
        if csize == 0 or props[0] >= 225:
            break
        blob = d[pos + HEADER:min(pos + HEADER + csize, limit)]
        want = min(chunk_size(props), size - len(out))
        try:
            dec = lzma.LZMADecompressor(format=lzma.FORMAT_RAW,
                                        filters=lzma_filters(props))
            piece = dec.decompress(blob, want)
            # Drain anything the decoder buffered when it hit max_length.
            while len(piece) < want and not dec.eof:
                more = dec.decompress(b'', want - len(piece))
                if not more:
                    break
                piece += more
        except lzma.LZMAError:
            break
        if not piece:
            break
        out += piece
        # Normally the whole file is one stream and the loop ends here. Some
        # archives do split a file across streams, so keep walking -- bounded.
        pos = (pos + HEADER + csize + align - 1) // align * align
    return bytes(out)


def main():
    argv = sys.argv[1:]
    if len(argv) < 2:
        print(__doc__)
        return 2

    pak_path = argv[0]
    csv_path = None
    if len(argv) >= 3 and argv[1].lower().endswith('.csv'):
        csv_path, out_dir = argv[1], argv[2]
        name_filter = argv[3] if len(argv) > 3 else None
    else:
        out_dir = argv[1]
        name_filter = argv[2] if len(argv) > 2 else None

    with open(pak_path, 'rb') as fh:
        d = fh.read()

    if csv_path:
        entries, align = read_table_csv(csv_path)
    else:
        entries = read_table(d)
        align = struct.unpack_from('<I', d, NX_CHUNK_ALIGN)[0]

    # Each entry may read up to the next entry's start, in address order.
    order = sorted(range(len(entries)), key=lambda i: entries[i][0])
    limits = {}
    for j, i in enumerate(order):
        limits[i] = entries[order[j + 1]][0] if j + 1 < len(order) else len(d)

    selected = [(i, e) for i, e in enumerate(entries)
                if not name_filter or name_filter in e[3]]

    print('%d entries (%d selected), chunkAlignment %d'
          % (len(entries), len(selected), align))
    ok = short = skipped = 0
    for i, (start, size, mode, name) in selected:
        scheme = mode >> 24
        if scheme == MODE_NONE:
            blob = d[start:start + size]
        elif scheme == MODE_LZMA:
            blob = extract_one(d, start, size, limits[i], align)
        else:
            skipped += 1
            continue
        dest = os.path.join(out_dir, name.replace('/', os.sep))
        parent = os.path.dirname(dest)
        if parent and not os.path.isdir(parent):
            os.makedirs(parent)
        with open(dest, 'wb') as fh:
            fh.write(blob)
        if len(blob) == size:
            ok += 1
        else:
            short += 1
            print('  SHORT %-70s %d/%d' % (name[-70:], len(blob), size))
    print('\ncomplete: %d   short: %d   skipped(unknown scheme): %d'
          % (ok, short, skipped))
    return 1 if short else 0


if __name__ == '__main__':
    sys.exit(main())

"""Structured reader for Alchemy IGZ files (Skylanders Imaginators, Switch/NX).

Written because IgzModelConverter parses these by scanning for magic numbers,
which on NX either spins forever (actors) or yields NaN vertex positions
(two thirds of statics). This walks the actual object graph instead: section
descriptors -> fixups -> type table -> objects, so every lookup is by type name
rather than by guessing at byte patterns.

Reference for the container layout: NefariousTechSupport/igArchiveExtractor
(IGZ_File.cs, IGZ_Fixup.cs, igObject.cs), plus the three NX fixes documented in
docs/ASSET-EXTRACTION.md.

Library module -- see igz_dump.py / igz_mesh.py for the entry points.
"""

import struct

MAGIC_LE = 0x49475A01          # "\x01ZGI" - little-endian (PS4/Switch)
MAGIC_BE = 0x015A4749

DESCRIPTOR_START = 0x18        # v9: (offset, size, unk, unk) x N, terminated by offset 0


class IgzError(Exception):
    pass


class Fixup(object):
    def __init__(self, tag, count, length, start_of_data, offset):
        self.tag = tag
        self.count = count
        self.length = length
        self.start_of_data = start_of_data
        self.offset = offset
        self.strings = []
        self.offsets = []
        self.values = []


class Igz(object):
    def __init__(self, data):
        self.d = data
        magic = struct.unpack_from('<I', data, 0)[0]
        if magic == MAGIC_LE:
            self.endian = '<'
        elif magic == MAGIC_BE:
            self.endian = '>'
        else:
            raise IgzError('not an IGZ (magic %08X)' % magic)

        self.version = self._u32(0x04)
        self.descriptors = []
        off = DESCRIPTOR_START
        while off + 16 <= len(data):
            o, s = self._u32(off), self._u32(off + 4)
            if o == 0:
                break
            self.descriptors.append((o, s))
            off += 16

        self.fixups = {}
        self._read_fixups()
        self.type_names = self.fixups['TMET'].strings if 'TMET' in self.fixups else []
        self.strings = self.fixups['TSTR'].strings if 'TSTR' in self.fixups else []

    # ---- primitives ----
    def _u16(self, o):
        return struct.unpack_from(self.endian + 'H', self.d, o)[0]

    def _u32(self, o):
        return struct.unpack_from(self.endian + 'I', self.d, o)[0]

    def _cstr(self, o):
        end = self.d.index(b'\0', o)
        return self.d[o:end].decode('latin-1'), end + 1

    # ---- fixups ----
    def _read_fixups(self):
        if not self.descriptors:
            raise IgzError('no section descriptors')
        base, size = self.descriptors[0]
        pos = base
        while pos < base + size:
            raw = self.d[pos:pos + 4]
            if len(raw) < 4:
                break
            # The tag bytes on disk already spell the canonical name ('RVTB').
            # The C# reader has to byte-swap because it reads them as a uint32
            # first; reading them as bytes here needs no swap at all.
            tag = raw.decode('latin-1')
            count = self._u32(pos + 4)
            length = self._u32(pos + 8)
            start_of_data = self._u32(pos + 12)
            if length <= 0 or pos + length > len(self.d):
                break
            fx = Fixup(tag, count, length, start_of_data, pos)
            self._parse_fixup(fx)
            self.fixups[tag] = fx
            pos += length

    def _parse_fixup(self, fx):
        data_at = fx.offset + fx.start_of_data
        if fx.tag in ('TSTR', 'TMET', 'TDEP', 'EXNM'):
            p = data_at
            for _ in range(fx.count):
                s, p = self._cstr(p)
                while p < len(self.d) and self.d[p] == 0:
                    p += 1
                fx.strings.append(s)
        elif fx.tag in ('MTSZ',):
            for i in range(fx.count):
                fx.values.append(self._u32(data_at + i * 4))
        elif fx.tag in ('RVTB', 'RSTR', 'RSTT', 'ROOT', 'RHND', 'RPID', 'REXT'):
            fx.offsets = self._decode_offset_map(fx, data_at)

    def _decode_offset_map(self, fx, data_at):
        """Offsets are a base-8 varint delta stream packed as nibbles, then
        split into (section index, offset within section) by the top bits."""
        out = []
        buf = self.d[data_at:fx.offset + fx.length]
        i = 0
        high = False
        prev = 0

        def nibble():
            nonlocal i, high
            if i >= len(buf):
                raise IgzError('offset map ran out of data')
            if not high:
                v = buf[i] & 0xF
                high = True
            else:
                v = buf[i] >> 4
                i += 1
                high = False
            return v

        for _ in range(fx.count):
            try:
                cur = nibble()
            except IgzError:
                break
            shift = 3
            unpacked = cur & 7
            while cur & 8:
                cur = nibble()
                unpacked |= (cur & 7) << (shift & 0x1F)
                shift += 3
            prev = (prev + unpacked * 4 + (4 if self.version < 9 else 0)) & 0xFFFFFFFF
            if self.version <= 0x06:
                sec, within = prev >> 0x18, prev & 0x00FFFFFF
            else:
                sec, within = prev >> 0x1B, prev & 0x07FFFFFF
            if sec + 1 < len(self.descriptors):
                out.append(self.descriptors[sec + 1][0] + within)
        return out

    # ---- objects ----
    def objects(self):
        """(offset, type_name, item_count, length) for every object in RVTB."""
        rvtb = self.fixups.get('RVTB')
        if not rvtb:
            return []
        out = []
        for off in rvtb.offsets:
            if off + 0x14 > len(self.d):
                continue
            if self.version in (0x06, 0x08):
                name = self._u16(off + 2)
                item_count = self._u32(off + 4)
            else:
                name = self._u32(off)
                item_count = self._u32(off + 8)
            tn = self.type_names[name] if name < len(self.type_names) else '?%d' % name
            # Object size comes from MTSZ (meta sizes), indexed by type -- it has
            # exactly one entry per type name. The in-object length field is not
            # what the C# reader's `length` suggests and reads back as 0 here.
            length = self.meta_sizes[name] if name < len(self.meta_sizes) else 0
            out.append((off, tn, item_count, length))
        return out

    @property
    def meta_sizes(self):
        fx = self.fixups.get('MTSZ')
        return fx.values if fx else []

#!/usr/bin/env python3
# Rosalina の日本語表示に使う font_jp.bin を、M+ BITMAP FONTS の 10 ドットの BDF から作る。
#
#   python3 mkfont_jp.py <PixelMplus の src ディレクトリ> <出力ファイル>
#
# 作ったファイルはこのフォルダに font_jp.bin として置く（build.bat が boot.firm の後ろに付け足す）。
#
# 使う BDF（https://github.com/itouhiro/PixelMplus の src/bdf.d）:
#   mplus_j10r.bdf            全角（JIS X 0208 と NEC 特殊文字）
#   mplus_j10r-jisx0213.bdf   全角（JIS X 0213 の一部）
#   mplus_j10r-jisx0201.bdf   半角カナ
#   mplus_j10r-iso-W5.bdf     Latin-1（U+00A0-U+00FF）
# と、src/ucstable.d の JISX0208.TXT・JISX0213.TXT。Shift-JIS の表は Python の cp932 で作る。
#
# ファイルの形（数値はすべてリトルエンディアン）:
#   0x00  char[4]  "LJF1"
#   0x04  u16      版（1）
#   0x06  u16      字 1 つの大きさ（22）
#   0x08  u16      ページの数
#   0x0A  u16      0
#   0x0C  u32      ページ表の位置
#   0x10  u32      ページの位置
#   0x14  u32      Shift-JIS の表の位置
#   0x18  u32      Shift-JIS の表の大きさ
#   0x1C  u32      ファイルの大きさ
#   ページ表: u16 x 256。Unicode の上位 8 ビットごとのページ番号（0xFFFF は字なし）
#   ページ: 字 x 256（下位 8 ビットの順）。字は
#           u8 印（bit 0: 字がある、bit 1: 全角＝2 桁）、u8 0、u16 x 10（上から 1 行ずつ）。
#           行の bit 11 が左端の点で、全角は 12 点、半角は 6 点（bit 11-6）を使う
#   Shift-JIS の表: 上位バイト 0x81-0x9F・0xE0-0xFC（60 個）x 下位バイト 0x40-0xFC（189 個）の
#           u16（Unicode、0 は字なし）

import os
import struct
import sys

ASCENT = 9          # BDF のベースラインより上の行数
ROWS = 10           # Rosalina の字の高さ
WIDE_CELL_X = 1     # 全角の字を 12 点の枠のどこから置くか
RECORD = 22
SJIS_LEADS = list(range(0x81, 0xA0)) + list(range(0xE0, 0xFD))
SJIS_TRAILS = range(0x40, 0xFD)


def parse_bdf(path):
    glyphs = {}
    lines = open(path, encoding='latin-1').read().split('\n')
    i = 0
    while i < len(lines):
        if lines[i].startswith('STARTCHAR'):
            enc = bbx = None
            bitmap = []
            while not lines[i].startswith('ENDCHAR'):
                line = lines[i]
                if line.startswith('ENCODING'):
                    enc = int(line.split()[1])
                elif line.startswith('BBX'):
                    bbx = tuple(map(int, line.split()[1:]))
                elif line.startswith('BITMAP'):
                    i += 1
                    while not lines[i].startswith('ENDCHAR'):
                        bitmap.append((int(lines[i], 16), len(lines[i].strip()) * 4))
                        i += 1
                    break
                i += 1
            glyphs[enc] = (bbx, bitmap)
        i += 1
    return glyphs


def pixels(glyph):
    """BDF の字の点を (x, y) で返す。y は 0 がアセントの一番上、10 がディセントの一番下"""
    (w, h, xo, yo), bitmap = glyph
    top = ASCENT - (yo + h)
    for r, (value, bits) in enumerate(bitmap):
        for x in range(w):
            if (value >> (bits - 1 - x)) & 1:
                yield xo + x, top + r


def make_rows(glyph, wide):
    pts = list(pixels(glyph))
    ys = {y for _, y in pts}
    # 11 行のうち 10 行を使う。ふつうは上の 10 行で、一番上が空いていて一番下を使う字
    # （」ｇｐ など）だけ下の 10 行にする。上下とも使う字（（）【】など）は一番下を落とす
    first = 1 if (0 not in ys and 10 in ys) else 0
    cell = 12 if wide else 6
    offset = WIDE_CELL_X if wide else 0
    rows = [0] * ROWS
    for x, y in pts:
        r = y - first
        px = x + offset
        if 0 <= r < ROWS and 0 <= px < cell:
            rows[r] |= 1 << (11 - px)
    return rows


def load_table(path):
    table = {}
    for line in open(path, encoding='latin-1'):
        if not line.startswith('0x'):
            continue
        a, b = line.split()[:2]
        if not b.startswith('0x') or '+' in b:
            continue
        table[int(a, 16)] = int(b, 16)
    return table


def jis_to_sjis(code):
    hi, lo = code >> 8, code & 0xFF
    s1 = (hi + 1) // 2 + (0x70 if hi <= 0x5E else 0xB0)
    if hi & 1:
        s2 = lo + (0x1F if lo <= 0x5F else 0x20)
    else:
        s2 = lo + 0x7E
    return bytes([s1, s2])


def cp932(data):
    try:
        text = data.decode('cp932')
    except UnicodeDecodeError:
        return None
    return ord(text) if len(text) == 1 else None


def build(src):
    bdfdir = os.path.join(src, 'bdf.d')
    ucs = os.path.join(src, 'ucstable.d')
    jis0208 = load_table(os.path.join(ucs, 'JISX0208.TXT'))
    jis0213 = load_table(os.path.join(ucs, 'JISX0213.TXT'))
    glyphs = {}     # Unicode -> (wide, rows)

    def put(code, wide, rows, replace):
        if code is None or code < 0x80 or code > 0xFFFF or 0xE000 <= code <= 0xF8FF:
            return
        if replace or code not in glyphs:
            glyphs[code] = (wide, rows)

    # 全角。JIS の表の Unicode に置いたあと、Windows（cp932）で別の Unicode になる字
    # （～ U+FF5E など）にも置く
    wide = []
    for name, table in (('mplus_j10r.bdf', jis0208), ('mplus_j10r-jisx0213.bdf', {})):
        for jis, glyph in parse_bdf(os.path.join(bdfdir, name)).items():
            rows = make_rows(glyph, True)
            put(table.get(jis, jis0213.get(jis)), True, rows, False)
            if table:
                wide.append((jis, rows))
    for jis, rows in wide:
        put(cp932(jis_to_sjis(jis)), True, rows, False)

    # 半角カナ（JIS X 0201 の 0xA1-0xDF）
    for code, glyph in parse_bdf(os.path.join(bdfdir, 'mplus_j10r-jisx0201.bdf')).items():
        if 0xA1 <= code <= 0xDF:
            put(0xFF61 + code - 0xA1, False, make_rows(glyph, False), True)

    # Latin-1。JIS にもある字（× ° など）は日本語の文の中で使うので全角のままにする
    for code, glyph in parse_bdf(os.path.join(bdfdir, 'mplus_j10r-iso-W5.bdf')).items():
        if 0xA0 <= code <= 0xFF:
            put(code, False, make_rows(glyph, False), False)

    sjis = []
    for lead in SJIS_LEADS:
        for trail in SJIS_TRAILS:
            code = cp932(bytes([lead, trail]))
            sjis.append(0 if code is None or 0xE000 <= code <= 0xF8FF else code)

    return glyphs, sjis


def write(glyphs, sjis, path):
    pages = sorted({code >> 8 for code in glyphs})
    header_size = 32
    table_off = header_size
    pages_off = table_off + 256 * 2
    sjis_off = pages_off + len(pages) * 256 * RECORD
    sjis_size = len(sjis) * 2
    size = sjis_off + sjis_size

    out = bytearray()
    out += struct.pack('<4sHHHHIIIII', b'LJF1', 1, RECORD, len(pages), 0,
                       table_off, pages_off, sjis_off, sjis_size, size)
    index = {page: i for i, page in enumerate(pages)}
    out += struct.pack('<256H', *[index.get(page, 0xFFFF) for page in range(256)])
    for page in pages:
        for low in range(256):
            entry = glyphs.get(page << 8 | low)
            if entry is None:
                out += bytes(RECORD)
            else:
                wide, rows = entry
                out += struct.pack('<BB10H', 1 | (2 if wide else 0), 0, *rows)
    out += struct.pack('<%dH' % len(sjis), *sjis)
    assert len(out) == size
    open(path, 'wb').write(out)
    return len(pages), size


def main():
    if len(sys.argv) != 3:
        sys.exit('usage: mkfont_jp.py <PixelMplus src> <font_jp.bin>')
    glyphs, sjis = build(sys.argv[1])
    pages, size = write(glyphs, sjis, sys.argv[2])
    print('%d glyphs, %d pages, %d bytes' % (len(glyphs), pages, size))


if __name__ == '__main__':
    main()

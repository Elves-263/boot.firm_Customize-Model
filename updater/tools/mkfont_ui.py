#!/usr/bin/env python3
# 更新アプリの画面の字（data/ui_font.bin）を、Noto Sans CJK JP（SIL Open Font License 1.1）から作る。
#
#   pip install pillow
#   python3 tools/mkfont_ui.py <NotoSansCJK の .ttc があるフォルダ> [出力]
#
# 大きさごとに 5 つの書体を作る。本文（BODY）だけは JIS X 0208 の字をすべて入れ、
# ほかは source/*.c の文字列に出てくる字と英数字だけを入れる（無い字は BODY で描く）。
#
# ファイルの形（数値はすべてリトルエンディアン）:
#   0x00  char[4]  "UIF1"
#   0x04  u16      書体の数
#   0x06  u16      0
#   0x08  u32 x n  書体の位置
#   書体: u16 字の大きさ（点）、u16 行の高さ、u16 ベースライン（行の上から）、u16 字の数、
#         u32 字の表の位置、u32 字形の位置（どちらもファイルの先頭から）
#   字の表: 字ごとに 12 バイト、文字コードの順。
#         u16 文字コード、u8 送り幅、u8 幅、u8 高さ、s8 左の空き、s8 上の位置（ベースラインから）、u8 0、
#         u32 字形の位置（書体の字形の位置から）
#   字形: 1 点 4 ビット（濃さ 0-15）。1 行は (幅 + 1) / 2 バイトで、各バイトの下位 4 ビットが左の点

import os
import re
import struct
import sys

from PIL import Image, ImageDraw, ImageFont

FACES = [
    # 名前,   ファイル,                     大きさ, 全部の字
    ('SMALL', 'NotoSansCJK-Regular.ttc',    12,     False),
    ('BODY',  'NotoSansCJK-Regular.ttc',    14,     True),
    ('LABEL', 'NotoSansCJK-Medium.ttc',     15,     False),
    ('TITLE', 'NotoSansCJK-Bold.ttc',       20,     False),
    ('HUGE',  'NotoSansCJK-Bold.ttc',       30,     False),
]
TTC_INDEX_JP = 0


def jis_chars():
    chars = set(chr(c) for c in range(0x20, 0x7F))
    chars |= set(chr(c) for c in range(0xA0, 0x100))
    for hi in range(0x81, 0xF0):
        if 0xA0 <= hi < 0xE0:
            continue
        for lo in range(0x40, 0xFD):
            if lo == 0x7F:
                continue
            try:
                chars.add(bytes([hi, lo]).decode('cp932'))
            except UnicodeDecodeError:
                pass
    chars |= set('〜—‖−¢£¬')  # cp932 と JIS で違う字
    return chars


def source_chars(srcdir):
    chars = set(chr(c) for c in range(0x20, 0x7F))
    for name in sorted(os.listdir(srcdir)):
        if name.endswith('.c'):
            text = open(os.path.join(srcdir, name), encoding='utf-8').read()
            for lit in re.findall(r'"((?:[^"\\\n]|\\.)*)"', text):
                chars |= set(lit)
    return chars


def render_face(path, size, chars):
    font = ImageFont.truetype(path, size, index=TTC_INDEX_JP)
    ascent, descent = font.getmetrics()
    glyphs = []
    pad = size
    notdef = bytes(font.getmask('\uffff'))
    for ch in sorted(chars):
        code = ord(ch)
        if code > 0xFFFF or ch in '\t\n\r':
            continue
        # 字形が無い字（豆腐になる字）は入れない
        if bytes(font.getmask(ch)) == notdef:
            continue
        img = Image.new('L', (size * 3, size * 3), 0)
        ImageDraw.Draw(img).text((pad, pad + ascent), ch, font=font, fill=255, anchor='ls')
        bbox = img.getbbox()
        advance = int(round(font.getlength(ch)))
        if bbox is None:
            glyphs.append((code, advance, 0, 0, 0, 0, b''))
            continue
        l, t, r, b = bbox
        w, h = r - l, b - t
        crop = img.crop(bbox)
        px = crop.load()
        data = bytearray()
        for y in range(h):
            row = [(px[x, y] * 15 + 127) // 255 for x in range(w)]
            if w % 2:
                row.append(0)
            for x in range(0, len(row), 2):
                data.append(row[x] | row[x + 1] << 4)
        glyphs.append((code, advance, w, h, l - pad, t - (pad + ascent), bytes(data)))
    return ascent, ascent + descent, glyphs


def main():
    if len(sys.argv) < 2:
        sys.exit('使い方: python3 tools/mkfont_ui.py <NotoSansCJK の .ttc があるフォルダ> [出力]')
    fontdir = sys.argv[1]
    here = os.path.dirname(os.path.abspath(__file__))
    out_path = sys.argv[2] if len(sys.argv) > 2 else os.path.join(here, '..', 'data', 'ui_font.bin')

    ui = source_chars(os.path.join(here, '..', 'source'))
    full = jis_chars() | ui

    faces = []
    for name, fname, size, everything in FACES:
        ascent, line, glyphs = render_face(os.path.join(fontdir, fname), size, full if everything else ui)
        faces.append((size, line, ascent, glyphs))
        print('%-5s %2dpx  %5d 字' % (name, size, len(glyphs)))

    head = bytearray(b'UIF1' + struct.pack('<HH', len(faces), 0))
    pos = len(head) + 4 * len(faces)
    blobs = []
    offsets = []
    for size, line, ascent, glyphs in faces:
        table_off = pos + 16
        data_off = table_off + 12 * len(glyphs)
        table = bytearray()
        data = bytearray()
        for code, adv, w, h, xoff, yoff, bits in glyphs:
            table += struct.pack('<HBBBbbBI', code, adv, w, h, xoff, yoff, 0, len(data))
            data += bits
        blob = struct.pack('<HHHHII', size, line, ascent, len(glyphs), table_off, data_off) + table + data
        while len(blob) % 4:
            blob += b'\0'
        offsets.append(pos)
        blobs.append(blob)
        pos += len(blob)
    for off in offsets:
        head += struct.pack('<I', off)

    os.makedirs(os.path.dirname(os.path.abspath(out_path)), exist_ok=True)
    with open(out_path, 'wb') as f:
        f.write(head)
        for blob in blobs:
            f.write(blob)
    print('%s: %d バイト' % (out_path, pos))


if __name__ == '__main__':
    main()

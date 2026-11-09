#!/usr/bin/env python3
# 更新アプリの絵と SMDH（HOME メニューのアイコンと名前）を作る。updater/ で
#
#   python3 tools/mkassets.py
#
# icon.png（48x48）、banner.png（256x128）、banner.wav（無音）、icon.smdh を作る。
# 絵の字は Rosalina と同じ 10 ドットの字（source/font.h と ../font_jp/font_jp.bin）を拡大して描く。
# Pillow が要る。名前を変えたら作り直す。

import os
import re
import struct
import wave

from PIL import Image, ImageDraw

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)

# SMDH の名前。日本語の本体では日本語、それ以外の言語では英語にする
SHORT_TITLE = 'Luma3DS_New 更新'
LONG_TITLE = 'Luma3DS_New の更新アプリ'
SHORT_TITLE_EN = 'Luma3DS_New Updater'
LONG_TITLE_EN = 'Luma3DS_New updater'
PUBLISHER = 'ELVES'

BACK = (20, 34, 60)
ACCENT = (51, 170, 255)
WHITE = (255, 255, 255)


def load_fonts():
    src = open(os.path.join(ROOT, 'source', 'font.h'), encoding='utf-8').read()
    ascii_font = [int(x, 16) for x in re.findall(r'0x([0-9A-Fa-f]{2})', src.split('font[] = {')[1])]
    data = open(os.path.join(ROOT, '..', 'font_jp', 'font_jp.bin'), 'rb').read()
    table, pages = struct.unpack_from('<II', data, 0x0C)
    page_of = struct.unpack_from('<256H', data, table)

    def glyph(ch):
        code = ord(ch)
        if code < 0x80:
            rows = ascii_font[code * 10:code * 10 + 10]
            return 1, [[(r >> (6 - k)) & 1 for k in range(6)] for r in rows]
        page = page_of[code >> 8]
        rec = struct.unpack_from('<11H', data, pages + (page * 256 + (code & 0xFF)) * 22)
        cols = 2 if rec[0] & 2 else 1
        return cols, [[(r >> (11 - k)) & 1 for k in range(cols * 6)] for r in rec[1:]]

    return glyph


def draw_text(img, x, y, text, color, scale, glyph):
    px = img.load()
    for ch in text:
        cols, rows = glyph(ch)
        for ry, row in enumerate(rows):
            for rx, bit in enumerate(row):
                if bit:
                    for sy in range(scale):
                        for sx in range(scale):
                            px[x + (rx - 1) * scale + sx, y + ry * scale + sy] = color
        x += cols * 6 * scale


def text_width(text, scale, glyph):
    return sum(glyph(ch)[0] for ch in text) * 6 * scale


def make_icon(path):
    img = Image.new('RGB', (48, 48), BACK)
    d = ImageDraw.Draw(img)
    d.rounded_rectangle((2, 2, 45, 45), radius=8, fill=(30, 60, 105))
    # 下向きの矢印と受け皿（ダウンロード・更新の印）
    d.rectangle((20, 8, 27, 24), fill=WHITE)
    d.polygon([(12, 22), (35, 22), (23.5, 34)], fill=WHITE)
    d.rectangle((10, 37, 37, 40), fill=ACCENT)
    img.save(path)
    return img


def make_banner(path, glyph):
    img = Image.new('RGB', (256, 128), BACK)
    d = ImageDraw.Draw(img)
    d.rectangle((0, 92, 255, 95), fill=ACCENT)
    title, sub = 'Luma3DS_New', '更新アプリ'
    draw_text(img, (256 - text_width(title, 3, glyph)) // 2, 22, title, WHITE, 3, glyph)
    draw_text(img, (256 - text_width(sub, 2, glyph)) // 2, 62, sub, ACCENT, 2, glyph)
    img.save(path)


def make_wav(path):
    with wave.open(path, 'wb') as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(22050)
        w.writeframes(b'\0\0' * 11025)


# 3DS のテクスチャは 8x8 の塊ごとに、塊の中は Z の順に並ぶ
TILE_ORDER = [
    0, 1, 8, 9, 2, 3, 10, 11, 16, 17, 24, 25, 18, 19, 26, 27,
    4, 5, 12, 13, 6, 7, 14, 15, 20, 21, 28, 29, 22, 23, 30, 31,
    32, 33, 40, 41, 34, 35, 42, 43, 48, 49, 56, 57, 50, 51, 58, 59,
    36, 37, 44, 45, 38, 39, 46, 47, 52, 53, 60, 61, 54, 55, 62, 63,
]


def tiled_rgb565(img):
    w, h = img.size
    px = img.load()
    out = bytearray()
    for ty in range(0, h, 8):
        for tx in range(0, w, 8):
            for k in TILE_ORDER:
                r, g, b = px[tx + (k & 7), ty + (k >> 3)][:3]
                out += struct.pack('<H', (r >> 3) << 11 | (g >> 2) << 5 | (b >> 3))
    return bytes(out)


def utf16_field(text, size):
    raw = text.encode('utf-16-le')
    assert len(raw) < size, text
    return raw + b'\0' * (size - len(raw))


def make_smdh(path, icon):
    # https://www.3dbrew.org/wiki/SMDH
    title = utf16_field(SHORT_TITLE, 0x80) + utf16_field(LONG_TITLE, 0x100) + utf16_field(PUBLISHER, 0x80)
    title_en = utf16_field(SHORT_TITLE_EN, 0x80) + utf16_field(LONG_TITLE_EN, 0x100) + utf16_field(PUBLISHER, 0x80)
    out = bytearray(b'SMDH' + struct.pack('<HH', 0, 0))
    out += title + title_en * 15                        # 16 の言語のうち、最初が日本語
    out += b'\0' * 0x10                                 # 年齢の区分（なし）
    out += struct.pack('<I', 0x7FFFFFFF)                # 地域を問わない
    out += b'\0' * 0x0C                                 # すれちがいの ID
    out += struct.pack('<I', 0x0001 | 0x0004 | 0x0100)  # 表示する、3D 可、遊んだ記録を付ける
    out += struct.pack('<HHfI', 0, 0, 0.0, 0)           # EULA、予約、バナーの絵の初めの枚、CEC ID
    out += b'\0' * 8
    out += tiled_rgb565(icon.resize((24, 24), Image.LANCZOS))
    out += tiled_rgb565(icon)
    assert len(out) == 0x36C0, hex(len(out))
    open(path, 'wb').write(bytes(out))


def main():
    glyph = load_fonts()
    icon = make_icon(os.path.join(ROOT, 'icon.png'))
    make_banner(os.path.join(ROOT, 'banner.png'), glyph)
    make_wav(os.path.join(ROOT, 'banner.wav'))
    make_smdh(os.path.join(ROOT, 'icon.smdh'), icon)
    print('icon.png banner.png banner.wav icon.smdh')


if __name__ == '__main__':
    main()

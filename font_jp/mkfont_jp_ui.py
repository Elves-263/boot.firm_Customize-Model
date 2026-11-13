#!/usr/bin/env python3
# 画面で使う字の字形を font_jp.bin から抜き出し、C の表にして書く。boot.firm に埋め込まれるので、
# フォントが無くても画面は漢字・かなで出る。
#   Rosalina:            common/rosalina/include/font_jp_ui.h（両方の版で共通）
#   起動時の設定画面など: Luma3DS_new/arm9/source/font_jp_ui.h と Luma3DS_v9_1/source/font_jp_ui.h
#
#   python3 mkfont_jp_ui.py
#
# 画面の文字列（ソースの "..." の中の ASCII 以外の字）を変えたら作り直す。
# 字形が font_jp.bin に無い字があるときは、その字を出して止まる。

import os
import re
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
FONT = os.path.join(HERE, 'font_jp.bin')
# (字を探すソースのフォルダ, 書き出すファイル)。同じ組のフォルダの字はまとめて 1 つの表にする
TARGETS = [
    (['Luma3DS_new/sysmodules/rosalina/source', 'Luma3DS_v9_1/sysmodules/rosalina/source', 'common/rosalina/source'],
     ['common/rosalina/include/font_jp_ui.h']),
    (['Luma3DS_new/arm9/source', 'Luma3DS_v9_1/source'],
     ['Luma3DS_new/arm9/source/font_jp_ui.h', 'Luma3DS_v9_1/source/font_jp_ui.h']),
]

LITERAL = re.compile(r'"(?:[^"\\\n]|\\.)*"')
COMMENT = re.compile(r'//[^\n]*|/\*.*?\*/', re.S)


def used_chars(dirs):
    chars = set()
    for d in dirs:
        src = os.path.join(ROOT, d)
        for dirpath, _, files in os.walk(src):
            for name in files:
                if not name.endswith('.c'):
                    continue
                text = open(os.path.join(dirpath, name), encoding='utf-8').read()
                text = COMMENT.sub('', text)
                for lit in LITERAL.findall(text):
                    chars.update(ord(c) for c in lit if ord(c) >= 0x80)
    return chars


def load_font(path):
    data = open(path, 'rb').read()
    magic, version, record, _, _, table, pages = struct.unpack_from('<4sHHHHII', data, 0)
    if magic != b'LJF1' or version != 1 or record != 22:
        sys.exit('font_jp.bin の形が違います')
    pageOf = struct.unpack_from('<256H', data, table)

    def glyph(code):
        page = pageOf[code >> 8]
        if page == 0xFFFF:
            return None
        rec = struct.unpack_from('<11H', data, pages + (page * 256 + (code & 0xFF)) * record)
        return rec if rec[0] & 1 else None

    return glyph


def write_table(glyph, dirs, outs):
    chars = sorted(c for c in used_chars(dirs) if c <= 0xFFFF)
    missing = [chr(c) for c in chars if glyph(c) is None]
    if missing:
        sys.exit('font_jp.bin に字形が無い字: ' + ' '.join(missing))

    lines = [
        '// 改変版の画面で使う字の字形（コード順）。font_jp/mkfont_jp_ui.py で作る。手で直さない',
        '// 字形は M+ BITMAP FONTS（font_jp/LICENSE_M+FONTS.txt）から',
        '',
        '#define FONTJP_UI_COUNT %d' % len(chars),
        '',
        'static const FontJpGlyph g_fontUi[FONTJP_UI_COUNT] =',
        '{',
    ]
    for c in chars:
        rec = glyph(c)
        rows = ', '.join('0x%03X' % r for r in rec[1:])
        lines.append('    { 0x%04X, 0x%02X, { %s } }, // %s' % (c, rec[0] & 0xFF, rows, chr(c)))
    lines.append('};')
    text = '\n'.join(lines) + '\n'

    for out in outs:
        path = os.path.join(ROOT, out)
        open(path, 'w', encoding='utf-8', newline='\n').write(text)
        print('%s: %d 字' % (out, len(chars)))


def main():
    glyph = load_font(FONT)
    for dirs, outs in TARGETS:
        write_table(glyph, dirs, outs)


if __name__ == '__main__':
    main()

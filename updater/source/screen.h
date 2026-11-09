/*
*   This file is part of a modified version of Luma3DS (Luma3DS_New updater)
*   Copyright (C) 2026 ELVES
*
*   This program is free software: you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation, either version 3 of the License, or
*   (at your option) any later version.
*
*   This program is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#pragma once

// 上と下の画面のフレームバッファに、なめらかな図形と字（data/ui_font.bin）を直接描く。
// UI_HOST を付けて PC でビルドすると、画面の代わりにメモリに描き、PPM に書き出せる（見本を作るため）

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define SCREEN_HEIGHT       240
#define SCREEN_TOP_WIDTH    400
#define SCREEN_BOTTOM_WIDTH 320

typedef enum ScreenId
{
    SCREEN_TOP,
    SCREEN_BOTTOM,
} ScreenId;

// data/ui_font.bin の書体の順（tools/mkfont_ui.py の FACES と合わせる）
typedef enum FontId
{
    FONT_SMALL,     // 12 点。添え書き
    FONT_BODY,      // 14 点。本文（JIS X 0208 の字をすべて持つ）
    FONT_LABEL,     // 15 点。ボタン
    FONT_TITLE,     // 20 点（太字）。見出し
    FONT_HUGE,      // 30 点（太字）。大きな数字
    FONT_COUNT,
} FontId;

void Screen_Init(void);
void Screen_Exit(void);

// 描き始めと描き終わり（描き終わりで表示を切り替える）
void Screen_Begin(void);
void Screen_End(void);

int Screen_Width(ScreenId s);

// 色は 0xRRGGBB。alpha は 0（透明）～ 255
void Screen_Gradient(ScreenId s, uint32_t top, uint32_t bottom);
void Screen_FillRect(ScreenId s, int x, int y, int w, int h, uint32_t color, int alpha);
void Screen_RoundRect(ScreenId s, int x, int y, int w, int h, int r, uint32_t color, int alpha);
// 上から下へ色が変わる角の丸い四角
void Screen_RoundRectV(ScreenId s, int x, int y, int w, int h, int r, uint32_t top, uint32_t bottom, int alpha);
void Screen_Circle(ScreenId s, float cx, float cy, float r, uint32_t color, int alpha);
// 太さ width の線（端は丸い）
void Screen_Line(ScreenId s, float x0, float y0, float x1, float y1, float width, uint32_t color, int alpha);

int Screen_LineHeight(FontId f);
// text の len バイトまで（改行で止める）の幅。len が (size_t)-1 なら最後まで
int Screen_TextWidth(FontId f, const char *text, size_t len);
// y は行の上端。描いた幅を返す
int Screen_DrawText(ScreenId s, FontId f, int x, int y, uint32_t color, const char *text, size_t len);
// 幅 width で折り返して描き、次の行の y を返す。lineHeight が 0 なら書体の行の高さ
int Screen_DrawWrapped(ScreenId s, FontId f, int x, int y, int width, int lineHeight, uint32_t color, const char *text);
// s から始まる 1 行を幅 width で切る。*len に行のバイト数（改行を除く）を入れ、次の行の先頭を返す。
// 英数字の単語の途中で切れるときは、その前の空白で切る。s が空なら NULL
const char *Screen_WrapLine(FontId f, const char *s, int width, size_t *len);

#ifdef UI_HOST
bool Screen_LoadFont(const char *path);
bool Screen_SavePpm(const char *path);    // 上と下の画面を縦に並べて書く
#endif

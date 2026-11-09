/*
*   This file is part of Luma3DS
*   Copyright (C) 2016-2021 Aurora Wright, TuxSH
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
*
*   Additional Terms 7.b and 7.c of GPLv3 apply to this file:
*       * Requiring preservation of specified reasonable legal notices or
*         author attributions in that material or in the Appropriate Legal
*         Notices displayed by works containing it.
*       * Prohibiting misrepresentation of the origin of that material,
*         or requiring that modified versions of such material be marked in
*         reasonable ways as different from the original version.
*/

#pragma once

#include <3ds/types.h>

// 日本語の字（M+ BITMAP FONTS の 10 ドット）。改変版の画面で使う字は font_jp_ui.h に
// 埋め込んである。それ以外の字（チート名・ファイル名など）と Shift-JIS を読む表は、
// SD:/boot.firm の FIRM の後ろに付けたフォント（build.bat が付ける）から読む（無ければ ? になる）。
// 形は font_jp/mkfont_jp.py を参照
#define FONTJP_PATH "/boot.firm"
#define FONTJP_ROWS 10

// 開けなかったフォントのファイルを、次に使うときにもう 1 度開いてみる（メニューを開いたときに呼ぶ）
void FontJp_Retry(void);
// code の字形を rows（FONTJP_ROWS 行、bit 11 が左端の点）に入れ、桁数（1 か 2）を返す。
// 字形が無いときは 0。rows は NULL でもよい
u32  FontJp_GetGlyph(u32 code, u16 *rows);

// s の先頭が UTF-8 の 2-4 バイトの字なら、そのバイト数を返し *code に入れる。それ以外は 0
u32  FontJp_DecodeUtf8(const char *s, u32 *code);
// src を、字の途中で切らずに dstSize バイト（終端を含む）まで写す。dst と src は同じでもよい
void FontJp_CopyUtf8(char *dst, u32 dstSize, const char *src);
// UTF-8 か Shift-JIS の src を UTF-8 にして写す（先頭の BOM は除く）。
// Shift-JIS を読む表はフォントにあるため、フォントが無いときはそのまま写す
void FontJp_ToUtf8(char *dst, u32 dstSize, const char *src);

/*
*   This file is part of Luma3DS
*   Copyright (C) 2016-2020 Aurora Wright, TuxSH
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
#include <3ds/gfx.h>
#include "utils.h"

#define GPU_FB_TOP_LEFT_ADDR_1      REG32(0x10400468)
#define GPU_FB_TOP_LEFT_ADDR_2      REG32(0x1040046C)
#define GPU_FB_TOP_FMT              REG32(0x10400470)
#define GPU_FB_TOP_SEL              REG32(0x10400478)
#define GPU_FB_TOP_COL_LUT_INDEX    REG32(0x10400480)
#define GPU_FB_TOP_COL_LUT_ELEM     REG32(0x10400484)
#define GPU_FB_TOP_STRIDE           REG32(0x10400490)
#define GPU_FB_TOP_RIGHT_ADDR_1     REG32(0x10400494)
#define GPU_FB_TOP_RIGHT_ADDR_2     REG32(0x10400498)

#define GPU_FB_BOTTOM_VBLANK        REG32(0x10400530)
#define GPU_FB_BOTTOM_VCOUNT        REG32(0x10400554)
#define GPU_FB_BOTTOM_ADDR_1        REG32(0x10400568)
#define GPU_FB_BOTTOM_ADDR_2        REG32(0x1040056C)
#define GPU_FB_BOTTOM_FMT           REG32(0x10400570)
#define GPU_FB_BOTTOM_SEL           REG32(0x10400578)
#define GPU_FB_BOTTOM_COL_LUT_INDEX REG32(0x10400580)
#define GPU_FB_BOTTOM_COL_LUT_ELEM  REG32(0x10400584)
#define GPU_FB_BOTTOM_STRIDE        REG32(0x10400590)

#define GPU_PSC0_CNT                REG32(0x1040001C)
#define GPU_PSC1_CNT                REG32(0x1040002C)

#define GPU_TRANSFER_CNT            REG32(0x10400C18)
#define GPU_CMDLIST_CNT             REG32(0x104018F0)

#define LCD_TOP_BRIGHTNESS          REG32(0x10202240)
#define LCD_TOP_FILLCOLOR           REG32(0x10202204)
#define LCD_BOT_BRIGHTNESS          REG32(0x10202A40)
#define LCD_BOT_FILLCOLOR           REG32(0x10202A04)

#define LCD_FILLCOLOR_ENABLE        (1u << 24)

#define FB_BOTTOM_VRAM_ADDR         ((void *)0x1F48F000) // cached
#define FB_BOTTOM_VRAM_PA           0x1848F000
#define FB_BOTTOM_SIZE              (320 * 240 * 2)
#define FB_SCREENSHOT_SIZE          (52 + 400 * 240 * 3)


#define SCREEN_BOT_WIDTH  320
#define SCREEN_BOT_HEIGHT 240

#define SPACING_Y 11
#define SPACING_X 6

#define SCROLL_MARK_X (SCREEN_BOT_WIDTH - 16)

#define COLOR_TITLE RGB565(0x00, 0x26, 0x1F)
#define COLOR_WHITE RGB565(0x1F, 0x3F, 0x1F)
#define COLOR_RED   RGB565(0x1F, 0x00, 0x00)
#define COLOR_GREEN RGB565(0x00, 0x1F, 0x00)
#define COLOR_LIME  RGB565(0x00, 0xFF, 0x00)
#define COLOR_YELLOW RGB565(0x1F, 0x3F, 0x00)
#define COLOR_BLACK RGB565(0x00, 0x00, 0x00)

#define DRAW_MAX_FORMATTED_STRING_SIZE  512

void Draw_Init(void);

void Draw_Lock(void);
void Draw_Unlock(void);

void Draw_DrawCharacter(u32 posX, u32 posY, u32 color, char character);
u32 Draw_DrawString(u32 posX, u32 posY, u32 color, const char *string);
// string を posX から 1 行に描く。画面の横の位置が minX 以上 maxX 未満の点だけを描く（posX は画面の外でもよい）
void Draw_DrawStringClipped(s32 posX, u32 posY, u32 color, const char *string, s32 minX, s32 maxX);

__attribute__((format(printf,4,5)))
u32 Draw_DrawFormattedString(u32 posX, u32 posY, u32 color, const char *fmt, ...);
u32 Draw_DrawBilingual(u32 posX, u32 posY, u32 color, const char *kanji, const char *english);
void Draw_DrawScrollMarks(u32 posY, u32 rows, u32 first, u32 total, u32 color);

// 文字列は UTF-8 で、日本語の字（font_jp.c）は全角が 2 桁、半角カナが 1 桁。
// 字形の無い字は ? の 1 桁、UTF-8 として正しくないバイトはそのバイトの字（CP437）の 1 桁
#define DRAW_TEXT_SIZE(cols) ((cols) * 3 + 1)   // cols 桁の文字列に要るバイト数（終端を含む）

// 画面の桁数
u32 Draw_TextColumns(const char *s);
// src を cols 桁・dstSize バイト（終端を含む）に収まるように、字の途中で切らずに写し、使った桁数を
// 返す。描けない字は ? にする（ファイル名など、外から来た文字列を出すときに使う）。dst と src は同じでもよい
u32 Draw_ClipText(char *dst, u32 dstSize, const char *src, u32 cols);
// s を cols 桁に収まるように、字の途中で切らずに短くする（字は置き換えない）
void Draw_CutText(char *s, u32 cols);
// s の末尾のうち、cols 桁に収まる一番長い部分
const char *Draw_TextTail(const char *s, u32 cols);

void Draw_FillFramebuffer(u32 value);
void Draw_ClearFramebuffer(void);
Result Draw_AllocateFramebufferCache(u32 size);
Result Draw_AllocateFramebufferCacheForScreenshot(u32 size);
void Draw_FreeFramebufferCache(void);
void *Draw_GetFramebufferCache(void);
u32 Draw_GetFramebufferCacheSize(void);
u32 Draw_SetupFramebuffer(void);
void Draw_RestoreFramebuffer(void);
void Draw_FlushFramebuffer(void);
u32 Draw_GetCurrentFramebufferAddress(bool top, bool left);
// Width is actually height as the 3ds screen is rotated 90 degrees
void Draw_GetCurrentScreenInfo(u32 *width, bool *is3d, bool top);

void Draw_CreateBitmapHeader(u8 *dst, u32 width, u32 heigth);
void Draw_ConvertFrameBufferLines(u8 *buf, u32 width, u32 startingLine, u32 numLines, u32 scaleFactorY, bool top, bool left);

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

/*
*   Code to print to the screen by mid-kid @CakesFW
*   https://github.com/mid-kid/CakesForeveryWan
*/

#include "draw.h"
#include "memory.h"
#include "screen.h"
#include "utils.h"
#include "fs.h"
#include "fmt.h"
#include "font.h"
#include "config.h"

bool loadSplash(void)
{
    static const char *topSplashFile = "splash.bin",
                      *bottomSplashFile = "splashbottom.bin";

    bool isTopSplashValid = getFileSize(topSplashFile) == SCREEN_TOP_FBSIZE,
         isBottomSplashValid = getFileSize(bottomSplashFile) == SCREEN_BOTTOM_FBSIZE;

    //Don't delay boot nor init the screens if no splash images or invalid splash images are on the SD
    if(!isTopSplashValid && !isBottomSplashValid) return false;

    initScreens();

    if(isTopSplashValid) isTopSplashValid = fileRead(fbs[1].top_left, topSplashFile, SCREEN_TOP_FBSIZE) == SCREEN_TOP_FBSIZE;
    if(isBottomSplashValid) isBottomSplashValid = fileRead(fbs[1].bottom, bottomSplashFile, SCREEN_BOTTOM_FBSIZE) == SCREEN_BOTTOM_FBSIZE;

    if(!isTopSplashValid && !isBottomSplashValid) return false;

    swapFramebuffers(true);

    wait(configData.splashDurationMsec);

    return true;
}

void drawCharacter(bool isTopScreen, u32 posX, u32 posY, u32 color, char character)
{
    u8 *select = isTopScreen ? fbs[0].top_left : fbs[0].bottom;

    for(u32 y = 0; y < 8; y++)
    {
        char charPos = font[character * 8 + y];

        for(u32 x = 0; x < 8; x++)
            if(((charPos >> (7 - x)) & 1) == 1)
            {
                u32 screenPos = (posX * SCREEN_HEIGHT * 3 + (SCREEN_HEIGHT - y - posY - 1) * 3) + x * 3 * SCREEN_HEIGHT;

                select[screenPos] = color >> 16;
                select[screenPos + 1] = color >> 8;
                select[screenPos + 2] = color;
            }
    }
}

// 日本語の字形（font_jp/mkfont_jp_ui.py で作る。M+ BITMAP FONTS の 10 ドット）
typedef struct FontJpGlyph
{
    u16 code;
    u16 flags;
    u16 rows[10];
} FontJpGlyph;

#define FONTJP_GLYPH_WIDE 2     // 12 ドット幅（無ければ 6 ドット幅）

#include "font_jp_ui.h"

static const FontJpGlyph *findGlyph(u32 code)
{
    u32 lo = 0, hi = FONTJP_UI_COUNT;

    while(lo < hi)
    {
        u32 mid = (lo + hi) / 2;
        if(g_fontUi[mid].code == code) return &g_fontUi[mid];
        if(g_fontUi[mid].code < code) lo = mid + 1;
        else hi = mid;
    }

    return NULL;
}

// UTF-8 の 1 字を読む。UTF-8 として正しくないバイト（"Pok\x82mon" のような CP437 の字）は、そのバイトを 1 字とする
static u32 decodeCharacter(const char *s, u32 *code)
{
    const u8 *p = (const u8 *)s;
    u32 n;

    if(p[0] >= 0xE0 && p[0] < 0xF0 && (p[1] & 0xC0) == 0x80 && (p[2] & 0xC0) == 0x80)
    {
        *code = ((p[0] & 0x0F) << 12) | ((p[1] & 0x3F) << 6) | (p[2] & 0x3F);
        n = 3;
    }
    else if(p[0] >= 0xC2 && p[0] < 0xE0 && (p[1] & 0xC0) == 0x80)
    {
        *code = ((p[0] & 0x1F) << 6) | (p[1] & 0x3F);
        n = 2;
    }
    else
    {
        *code = p[0];
        n = 1;
    }

    return n;
}

// 1 字の幅（点）。ASCII と CP437 は SPACING_X、日本語は 12 か 6
static u32 characterWidth(u32 code, u32 n, const FontJpGlyph **glyph)
{
    *glyph = n > 1 ? findGlyph(code) : NULL;
    if(*glyph != NULL) return ((*glyph)->flags & FONTJP_GLYPH_WIDE) ? 12 : 6;
    return SPACING_X;
}

// 日本語の字形は 10 ドットの高さなので、ASCII（8 ドット）より 1 ドット上から描く
static void drawGlyph(bool isTopScreen, u32 posX, u32 posY, u32 color, const FontJpGlyph *glyph, u32 width)
{
    u8 *select = isTopScreen ? fbs[0].top_left : fbs[0].bottom;

    for(u32 y = 0; y < 10; y++)
        for(u32 x = 0; x < width; x++)
            if((glyph->rows[y] >> (11 - x)) & 1)
            {
                u32 screenPos = (posX * SCREEN_HEIGHT * 3 + (SCREEN_HEIGHT - y - (posY - 1) - 1) * 3) + x * 3 * SCREEN_HEIGHT;

                select[screenPos] = color >> 16;
                select[screenPos + 1] = color >> 8;
                select[screenPos + 2] = color;
            }
}

u32 drawStringWidth(const char *string, u32 length)
{
    u32 width = 0;
    const FontJpGlyph *glyph;

    for(u32 i = 0; i < length && string[i] != 0 && string[i] != '\n';)
    {
        u32 code, n = decodeCharacter(string + i, &code);
        width += string[i] == '\t' ? 2 * SPACING_X : characterWidth(code, n, &glyph);
        i += n;
    }

    return width;
}

u32 drawString(bool isTopScreen, u32 posX, u32 posY, u32 color, const char *string)
{
    u32 lineWidth = (isTopScreen ? SCREEN_TOP_WIDTH : SCREEN_BOTTOM_WIDTH) - posX;

    // x は行の頭からの点の数
    for(u32 i = 0, x = 0; string[i] != 0;)
    {
        u32 code, n = decodeCharacter(string + i, &code);
        const FontJpGlyph *glyph;
        u32 width = characterWidth(code, n, &glyph);

        switch(string[i])
        {
            case '\n':
                posY += SPACING_Y;
                x = 0;
                break;

            case '\t':
                x += 2 * SPACING_X;
                break;

            default:
                //Make sure we never get out of the screen
                if(x + width > lineWidth)
                {
                    posY += SPACING_Y;
                    x = SPACING_X; //Little offset so we know the same string continues
                    if(string[i] == ' ') break; //Spaces at the start look weird
                }

                if(glyph != NULL) drawGlyph(isTopScreen, posX + x, posY, color, glyph, width);
                else drawCharacter(isTopScreen, posX + x, posY, color, n > 1 ? '?' : string[i]);

                x += width;
                break;
        }

        i += n;
    }

    return posY;
}

u32 drawFormattedString(bool isTopScreen, u32 posX, u32 posY, u32 color, const char *fmt, ...)
{
    char buf[DRAW_MAX_FORMATTED_STRING_SIZE + 1];
    va_list args;
    va_start(args, fmt);
    vsprintf(buf, fmt, args);
    va_end(args);

    return drawString(isTopScreen, posX, posY, color, buf);
}

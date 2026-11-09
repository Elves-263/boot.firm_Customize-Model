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

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "screen.h"
#include "text.h"

#ifdef UI_HOST
static uint8_t g_hostFb[2][SCREEN_TOP_WIDTH * SCREEN_HEIGHT * 3];
static uint8_t *g_fontData;
static size_t g_fontSize;
#else
#include <3ds.h>
#include "ui_font_bin.h"
#endif

// ---------------------------------------------------------------- 字（data/ui_font.bin。形は tools/mkfont_ui.py を参照）

typedef struct FontFace
{
    int             lineHeight;
    int             ascent;
    int             count;
    const uint8_t   *table;     // 字ごとに 12 バイト
    const uint8_t   *bits;
} FontFace;

typedef struct Glyph
{
    int             advance, w, h, xoff, yoff;
    const uint8_t   *bits;
} Glyph;

static FontFace g_faces[FONT_COUNT];

static uint32_t Screen_Le16(const uint8_t *p) { return p[0] | p[1] << 8; }
static uint32_t Screen_Le32(const uint8_t *p) { return p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24; }

static bool Screen_SetFont(const uint8_t *f, size_t size)
{
    if (size < 8 || memcmp(f, "UIF1", 4) != 0 || Screen_Le16(f + 4) != FONT_COUNT || 8 + 4 * FONT_COUNT > size)
        return false;

    for (int i = 0; i < FONT_COUNT; ++i)
    {
        uint32_t off = Screen_Le32(f + 8 + 4 * i), table, bits, count;
        if ((uint64_t)off + 16 > size)
            return false;
        count = Screen_Le16(f + off + 6);
        table = Screen_Le32(f + off + 8);
        bits = Screen_Le32(f + off + 12);
        if ((uint64_t)table + 12ULL * count > size || bits > size)
            return false;

        g_faces[i].lineHeight = (int)Screen_Le16(f + off + 2);
        g_faces[i].ascent = (int)Screen_Le16(f + off + 4);
        g_faces[i].count = (int)count;
        g_faces[i].table = f + table;
        g_faces[i].bits = f + bits;
    }
    return true;
}

static bool Screen_FindGlyph(const FontFace *face, uint32_t code, Glyph *g)
{
    int lo = 0, hi = face->count - 1;

    if (face->table == NULL || code > 0xFFFF)
        return false;
    while (lo <= hi)
    {
        int mid = (lo + hi) / 2;
        const uint8_t *e = face->table + 12 * mid;
        uint32_t c = Screen_Le16(e);

        if (c < code)
            lo = mid + 1;
        else if (c > code)
            hi = mid - 1;
        else
        {
            g->advance = e[2];
            g->w = e[3];
            g->h = e[4];
            g->xoff = (int8_t)e[5];
            g->yoff = (int8_t)e[6];
            g->bits = face->bits + Screen_Le32(e + 8);
            return true;
        }
    }
    return false;
}

// 書体に無い字は本文の書体で、それにも無い字は ? で描く
static void Screen_GetGlyph(FontId f, uint32_t code, Glyph *g)
{
    // 制御文字（Markdown の飾りの印など）は幅も形も無い
    if (code < 0x20)
    {
        *g = (Glyph){ 0, 0, 0, 0, 0, NULL };
        return;
    }

    if (Screen_FindGlyph(&g_faces[f], code, g) || Screen_FindGlyph(&g_faces[FONT_BODY], code, g) ||
        Screen_FindGlyph(&g_faces[f], '?', g))
        return;
    memset(g, 0, sizeof(*g));
}

int Screen_LineHeight(FontId f)
{
    return g_faces[f].lineHeight;
}

// ---------------------------------------------------------------- 画面

static uint8_t *g_fb[2];
static const int g_width[2] = { SCREEN_TOP_WIDTH, SCREEN_BOTTOM_WIDTH };

void Screen_Init(void)
{
#ifndef UI_HOST
    gfxInitDefault();   // 両方の画面とも BGR8（1 点 3 バイト）、2 枚を切り替えて使う
    Screen_SetFont(ui_font_bin, ui_font_bin_size);
#endif
}

void Screen_Exit(void)
{
#ifndef UI_HOST
    gfxExit();
#endif
}

void Screen_Begin(void)
{
#ifdef UI_HOST
    g_fb[SCREEN_TOP] = g_hostFb[SCREEN_TOP];
    g_fb[SCREEN_BOTTOM] = g_hostFb[SCREEN_BOTTOM];
#else
    g_fb[SCREEN_TOP] = gfxGetFramebuffer(GFX_TOP, GFX_LEFT, NULL, NULL);
    g_fb[SCREEN_BOTTOM] = gfxGetFramebuffer(GFX_BOTTOM, GFX_LEFT, NULL, NULL);
#endif
}

void Screen_End(void)
{
#ifndef UI_HOST
    gfxFlushBuffers();
    gfxSwapBuffers();
    gspWaitForVBlank();
#endif
}

int Screen_Width(ScreenId s)
{
    return g_width[s];
}

// フレームバッファは 90 度回っていて、x ごとに下から上へ並ぶ
static inline uint8_t *Screen_At(ScreenId s, int x, int y)
{
    return g_fb[s] + (x * SCREEN_HEIGHT + (SCREEN_HEIGHT - 1 - y)) * 3;
}

static inline void Screen_Blend(ScreenId s, int x, int y, uint32_t color, int a)
{
    uint8_t *p;

    if ((unsigned)x >= (unsigned)g_width[s] || (unsigned)y >= SCREEN_HEIGHT || a <= 0)
        return;
    p = Screen_At(s, x, y);
    if (a >= 255)
    {
        p[0] = color & 0xFF;
        p[1] = (color >> 8) & 0xFF;
        p[2] = (color >> 16) & 0xFF;
        return;
    }
    p[0] = (uint8_t)((p[0] * (255 - a) + (color & 0xFF) * a + 127) / 255);
    p[1] = (uint8_t)((p[1] * (255 - a) + ((color >> 8) & 0xFF) * a + 127) / 255);
    p[2] = (uint8_t)((p[2] * (255 - a) + ((color >> 16) & 0xFF) * a + 127) / 255);
}

static uint32_t Screen_Mix(uint32_t a, uint32_t b, int t, int n)
{
    uint32_t out = 0;

    for (int shift = 0; shift < 24; shift += 8)
    {
        int ca = (a >> shift) & 0xFF, cb = (b >> shift) & 0xFF;
        out |= (uint32_t)(ca + (cb - ca) * t / n) << shift;
    }
    return out;
}

void Screen_Gradient(ScreenId s, uint32_t top, uint32_t bottom)
{
    uint8_t column[SCREEN_HEIGHT * 3];

    // 1 列分を作って、全部の列に写す
    for (int y = 0; y < SCREEN_HEIGHT; ++y)
    {
        uint32_t c = Screen_Mix(top, bottom, y, SCREEN_HEIGHT - 1);
        uint8_t *p = column + (SCREEN_HEIGHT - 1 - y) * 3;
        p[0] = c & 0xFF;
        p[1] = (c >> 8) & 0xFF;
        p[2] = (c >> 16) & 0xFF;
    }
    for (int x = 0; x < g_width[s]; ++x)
        memcpy(g_fb[s] + x * SCREEN_HEIGHT * 3, column, sizeof(column));
}

void Screen_FillRect(ScreenId s, int x, int y, int w, int h, uint32_t color, int alpha)
{
    int x0 = x < 0 ? 0 : x, y0 = y < 0 ? 0 : y;
    int x1 = x + w > g_width[s] ? g_width[s] : x + w, y1 = y + h > SCREEN_HEIGHT ? SCREEN_HEIGHT : y + h;

    for (int i = x0; i < x1; ++i)
        for (int j = y0; j < y1; ++j)
            Screen_Blend(s, i, j, color, alpha);
}

void Screen_RoundRectV(ScreenId s, int x, int y, int w, int h, int r, uint32_t top, uint32_t bottom, int alpha)
{
    if (r * 2 > h)
        r = h / 2;
    if (r * 2 > w)
        r = w / 2;

    // 1 行ずつ色を決める。角の丸い部分だけ 1 点ずつ覆う割合を求め、残りは四角で塗る
    for (int j = 0; j < h; ++j)
    {
        uint32_t color = h > 1 ? Screen_Mix(top, bottom, j, h - 1) : top;
        int dyRow = j < r ? r - j : j >= h - r ? j - (h - r) + 1 : 0;

        if (dyRow == 0)
        {
            Screen_FillRect(s, x, y + j, w, 1, color, alpha);
            continue;
        }
        Screen_FillRect(s, x + r, y + j, w - 2 * r, 1, color, alpha);
        for (int i = 0; i < r; ++i)
        {
            float dx = r - (i + 0.5f), dy = dyRow - 0.5f;
            float cover = r + 0.5f - sqrtf(dx * dx + dy * dy);
            int a;

            if (cover <= 0.0f)
                continue;
            a = cover >= 1.0f ? alpha : (int)(alpha * cover);
            Screen_Blend(s, x + i, y + j, color, a);
            Screen_Blend(s, x + w - 1 - i, y + j, color, a);
        }
    }
}

void Screen_RoundRect(ScreenId s, int x, int y, int w, int h, int r, uint32_t color, int alpha)
{
    Screen_RoundRectV(s, x, y, w, h, r, color, color, alpha);
}

void Screen_Circle(ScreenId s, float cx, float cy, float r, uint32_t color, int alpha)
{
    int x0 = (int)floorf(cx - r - 1), x1 = (int)ceilf(cx + r + 1);
    int y0 = (int)floorf(cy - r - 1), y1 = (int)ceilf(cy + r + 1);

    for (int y = y0; y <= y1; ++y)
    {
        for (int x = x0; x <= x1; ++x)
        {
            float dx = x + 0.5f - cx, dy = y + 0.5f - cy;
            float cover = r + 0.5f - sqrtf(dx * dx + dy * dy);
            if (cover > 0.0f)
                Screen_Blend(s, x, y, color, cover >= 1.0f ? alpha : (int)(alpha * cover));
        }
    }
}

void Screen_Line(ScreenId s, float x0, float y0, float x1, float y1, float width, uint32_t color, int alpha)
{
    float r = width / 2, vx = x1 - x0, vy = y1 - y0, len2 = vx * vx + vy * vy;
    int bx0 = (int)floorf(fminf(x0, x1) - r - 1), bx1 = (int)ceilf(fmaxf(x0, x1) + r + 1);
    int by0 = (int)floorf(fminf(y0, y1) - r - 1), by1 = (int)ceilf(fmaxf(y0, y1) + r + 1);

    for (int y = by0; y <= by1; ++y)
    {
        for (int x = bx0; x <= bx1; ++x)
        {
            float px = x + 0.5f - x0, py = y + 0.5f - y0;
            float t = len2 > 0.0f ? (px * vx + py * vy) / len2 : 0.0f;
            float dx, dy, cover;

            t = t < 0.0f ? 0.0f : t > 1.0f ? 1.0f : t;
            dx = px - t * vx;
            dy = py - t * vy;
            cover = r + 0.5f - sqrtf(dx * dx + dy * dy);
            if (cover > 0.0f)
                Screen_Blend(s, x, y, color, cover >= 1.0f ? alpha : (int)(alpha * cover));
        }
    }
}

// ---------------------------------------------------------------- 文字

int Screen_TextWidth(FontId f, const char *text, size_t len)
{
    const char *end = len == (size_t)-1 ? text + strlen(text) : text + len;
    int width = 0;

    while (text < end && *text != 0 && *text != '\n')
    {
        uint32_t code;
        Glyph g;

        text += Text_Decode(text, &code);
        Screen_GetGlyph(f, code, &g);
        width += g.advance;
    }
    return width;
}

int Screen_DrawText(ScreenId s, FontId f, int x, int y, uint32_t color, const char *text, size_t len)
{
    const char *end = len == (size_t)-1 ? text + strlen(text) : text + len;
    int pen = x, baseline = y + g_faces[f].ascent;

    while (text < end && *text != 0 && *text != '\n')
    {
        uint32_t code;
        Glyph g;
        int stride;

        text += Text_Decode(text, &code);
        Screen_GetGlyph(f, code, &g);
        stride = (g.w + 1) / 2;

        for (int j = 0; j < g.h; ++j)
        {
            for (int i = 0; i < g.w; ++i)
            {
                int v = (g.bits[j * stride + i / 2] >> ((i & 1) * 4)) & 0xF;
                if (v != 0)
                    Screen_Blend(s, pen + g.xoff + i, baseline + g.yoff + j, color, v * 17);
            }
        }
        pen += g.advance;
    }
    return pen - x;
}

// 行の頭に来ないようにする字（句読点・閉じかっこ・小さい字など）
static bool Screen_NoLineStart(uint32_t c)
{
    static const char chars[] = "、。，．）」』】〉》！？ー…・ぁぃぅぇぉっゃゅょァィゥェォッャュョ";

    if (c == ',' || c == '.' || c == ')' || c == '!' || c == '?')
        return true;
    for (const char *p = chars; *p != 0;)
    {
        uint32_t k;
        p += Text_Decode(p, &k);
        if (k == c)
            return true;
    }
    return false;
}

const char *Screen_WrapLine(FontId f, const char *s, int width, size_t *len)
{
    // 英数字の単語を切らずに折り返せるところ: 空白（空白は次の行に送らない）と、日本語の字の次に英数字が来るところ
    const char *lastSpace = NULL, *lastBoundary = NULL, *p = s;
    bool prevWide = false;
    int used = 0;

    if (*s == 0)
        return NULL;

    while (*p != 0 && *p != '\n')
    {
        uint32_t c;
        size_t n = Text_Decode(p, &c);
        Glyph g;

        Screen_GetGlyph(f, c, &g);
        if (used + g.advance > width && used != 0)
        {
            // 英数字の単語の途中なら、その前の空白か、日本語との境目で切る
            bool midWord = c < 0x80 && c != ' ' && !prevWide && p > s && p[-1] != ' ';
            if (midWord && (lastSpace != NULL || lastBoundary != NULL))
            {
                if (lastBoundary != NULL && (lastSpace == NULL || lastBoundary > lastSpace))
                {
                    *len = (size_t)(lastBoundary - s);
                    return lastBoundary;
                }
                *len = (size_t)(lastSpace - s);
                return lastSpace + 1;
            }
            // 句読点などは前の行の端にはみ出させる（2 字まで）
            for (int i = 0; i < 2 && Screen_NoLineStart(c); ++i)
            {
                p += n;
                n = Text_Decode(p, &c);
            }
            if (*p == 0 || *p == '\n')
            {
                *len = (size_t)(p - s);
                return *p == '\n' ? p + 1 : p;
            }
            *len = (size_t)(p - s);
            return c == ' ' ? p + 1 : p;
        }

        if (c == ' ')
            lastSpace = p;
        else if (c >= 0x80)
            prevWide = true;
        else if (c >= 0x20)
        {
            if (prevWide && p > s)
                lastBoundary = p;
            prevWide = false;
        }
        used += g.advance;
        p += n;
    }

    *len = (size_t)(p - s);
    return *p == '\n' ? p + 1 : p;
}

int Screen_DrawWrapped(ScreenId s, FontId f, int x, int y, int width, int lineHeight, uint32_t color, const char *text)
{
    size_t len;
    const char *next;

    if (lineHeight == 0)
        lineHeight = g_faces[f].lineHeight;
    while ((next = Screen_WrapLine(f, text, width, &len)) != NULL)
    {
        Screen_DrawText(s, f, x, y, color, text, len);
        y += lineHeight;
        text = next;
    }
    return y;
}

// ---------------------------------------------------------------- PC で見本を作る

#ifdef UI_HOST
bool Screen_LoadFont(const char *path)
{
    FILE *f = fopen(path, "rb");
    long size;

    if (f == NULL)
        return false;
    fseek(f, 0, SEEK_END);
    size = ftell(f);
    fseek(f, 0, SEEK_SET);
    g_fontData = (uint8_t *)malloc((size_t)size);
    g_fontSize = fread(g_fontData, 1, (size_t)size, f);
    fclose(f);
    return Screen_SetFont(g_fontData, g_fontSize);
}

bool Screen_SavePpm(const char *path)
{
    FILE *f = fopen(path, "wb");

    if (f == NULL)
        return false;
    fprintf(f, "P6\n%d %d\n255\n", SCREEN_TOP_WIDTH, 2 * SCREEN_HEIGHT);
    for (int s = 0; s < 2; ++s)
    {
        int left = (SCREEN_TOP_WIDTH - g_width[s]) / 2;
        for (int y = 0; y < SCREEN_HEIGHT; ++y)
        {
            for (int x = 0; x < SCREEN_TOP_WIDTH; ++x)
            {
                uint8_t rgb[3] = { 0x20, 0x20, 0x20 };
                if (x >= left && x < left + g_width[s])
                {
                    const uint8_t *p = g_hostFb[s] + ((x - left) * SCREEN_HEIGHT + (SCREEN_HEIGHT - 1 - y)) * 3;
                    rgb[0] = p[2];
                    rgb[1] = p[1];
                    rgb[2] = p[0];
                }
                fwrite(rgb, 1, 3, f);
            }
        }
    }
    fclose(f);
    return true;
}
#endif

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

#include <3ds.h>
#include "font_jp.h"
#include "draw.h"
#include "ifile.h"
#include "memory.h"

// ファイルの形は font_jp/mkfont_jp.py を参照
#define FONTJP_MAGIC        0x31464A4C  // "LJF1"
#define FIRM_MAGIC          0x4D524946  // "FIRM"
#define FONTJP_VERSION      1
#define FONTJP_RECORD_SIZE  22
#define FONTJP_SJIS_LEADS   60          // 0x81-0x9F と 0xE0-0xFC
#define FONTJP_SJIS_TRAILS  189         // 0x40-0xFC

#define FONTJP_GLYPH_EXISTS 0x01
#define FONTJP_GLYPH_WIDE   0x02
#define FONTJP_GLYPH_CACHED 0x80
#define FONTJP_GLYPH_USED   0x100

// 読んだ字は、1 画面分の字が入るように 256 字まで覚えておく。いっぱいのときは、
// しばらく使っていない字から入れ替える（クロック方式）
#define FONTJP_CACHE_SIZE   256
#define FONTJP_SJIS_CACHE   4

typedef struct FontJpHeader
{
    u32 magic;
    u16 version;
    u16 recordSize;
    u16 pageCount;
    u16 reserved;
    u32 pageTableOffset;
    u32 pagesOffset;
    u32 sjisOffset;
    u32 sjisSize;
    u32 fileSize;
} FontJpHeader;

// FIRM のヘッダーのうち、セクションの場所と大きさだけを使う
typedef struct FirmSectionHeader
{
    u32 offset;
    u32 address;
    u32 size;
    u32 copyMethod;
    u8  hash[0x20];
} FirmSectionHeader;

typedef struct FirmHeader
{
    u32 magic;
    u8  reserved[0x3C];
    FirmSectionHeader section[4];
} FirmHeader;

typedef struct FontJpGlyph
{
    u16 code;
    u16 flags;
    u16 rows[FONTJP_ROWS];
} FontJpGlyph;

// 改変版の画面で使う字は埋め込みの字形（g_fontUi）から引く。boot.firm にフォントが無くても出せる
#include "font_jp_ui.h"

static IFile        g_fontFile;
static bool         g_fontOpen;
static bool         g_fontFailed;
static u32          g_fontBase;     // boot.firm の中のフォントの位置
static u32          g_fontPagesOffset;
static u32          g_fontSjisOffset;
static u16          g_fontPages[256];
static FontJpGlyph  g_fontCache[FONTJP_CACHE_SIZE];
static u32          g_fontCacheHand;
static u16          g_sjisRows[FONTJP_SJIS_CACHE][FONTJP_SJIS_TRAILS];
static u8           g_sjisLead[FONTJP_SJIS_CACHE];  // 上位バイトの番号 + 1（0 は空き）
static u32          g_sjisNext;

static void FontJp_Close(void)
{
    if (g_fontOpen)
        IFile_Close(&g_fontFile);
    g_fontOpen = false;
    g_fontFailed = true;
}

static bool FontJp_Read(u32 offset, void *buf, u32 size)
{
    u64 total = 0;

    if (!g_fontOpen)
        return false;

    g_fontFile.pos = g_fontBase + offset;
    if (R_SUCCEEDED(IFile_Read(&g_fontFile, &total, buf, size)) && total == size)
        return true;

    // 途中で読めなくなったら（SD カードを抜いたなど）、それ以降は字形なしとして扱う
    FontJp_Close();
    return false;
}

static bool FontJp_Open(void)
{
    FirmHeader firm;
    FontJpHeader hdr;
    u64 size = 0;

    memset(g_fontCache, 0, sizeof(g_fontCache));
    memset(g_sjisLead, 0, sizeof(g_sjisLead));

    if (R_FAILED(IFile_Open(&g_fontFile, ARCHIVE_SDMC, fsMakePath(PATH_EMPTY, ""),
                            fsMakePath(PATH_ASCII, FONTJP_PATH), FS_OPEN_READ)))
    {
        g_fontFailed = true;
        return false;
    }
    g_fontOpen = true;
    g_fontBase = 0;

    // フォントは、FIRM のいちばん後ろのセクションの終わりから 0x200 バイト境界に付けてある
    if (R_FAILED(IFile_GetSize(&g_fontFile, &size)) || !FontJp_Read(0, &firm, sizeof(firm)) || firm.magic != FIRM_MAGIC)
    {
        FontJp_Close();
        return false;
    }
    for (u32 i = 0; i < 4; ++i)
        if (firm.section[i].size != 0 && firm.section[i].offset + firm.section[i].size > g_fontBase)
            g_fontBase = firm.section[i].offset + firm.section[i].size;
    g_fontBase = (g_fontBase + 0x1FF) & ~0x1FF;
    if (g_fontBase >= size)
    {
        FontJp_Close();
        return false;
    }
    size -= g_fontBase;

    if (!FontJp_Read(0, &hdr, sizeof(hdr)) ||
        hdr.magic != FONTJP_MAGIC || hdr.version != FONTJP_VERSION || hdr.recordSize != FONTJP_RECORD_SIZE ||
        hdr.fileSize > size || hdr.sjisSize != FONTJP_SJIS_LEADS * FONTJP_SJIS_TRAILS * 2 ||
        (u64)hdr.pagesOffset + (u64)hdr.pageCount * 256 * FONTJP_RECORD_SIZE > size ||
        (u64)hdr.sjisOffset + hdr.sjisSize > size ||
        !FontJp_Read(hdr.pageTableOffset, g_fontPages, sizeof(g_fontPages)))
    {
        FontJp_Close();
        return false;
    }

    for (u32 i = 0; i < 256; ++i)
    {
        if (g_fontPages[i] != 0xFFFF && g_fontPages[i] >= hdr.pageCount)
        {
            FontJp_Close();
            return false;
        }
    }

    g_fontPagesOffset = hdr.pagesOffset;
    g_fontSjisOffset = hdr.sjisOffset;
    return true;
}

// boot.firm のフォントを使えるか（初めて呼ばれたときに開く）
static bool FontJp_Available(void)
{
    Draw_Lock();
    bool available = g_fontOpen || (!g_fontFailed && FontJp_Open());
    Draw_Unlock();

    return available;
}

void FontJp_Retry(void)
{
    Draw_Lock();
    if (!g_fontOpen)
        g_fontFailed = false;
    Draw_Unlock();
}

// 埋め込みの字形（コード順）から code の字を探す
static const FontJpGlyph *FontJp_FindUi(u32 code)
{
    u32 lo = 0, hi = FONTJP_UI_COUNT;

    while (lo < hi)
    {
        u32 mid = (lo + hi) / 2;

        if (g_fontUi[mid].code < code)
            lo = mid + 1;
        else
            hi = mid;
    }

    return lo < FONTJP_UI_COUNT && g_fontUi[lo].code == code ? &g_fontUi[lo] : NULL;
}

u32 FontJp_GetGlyph(u32 code, u16 *rows)
{
    u32 cols = 0;

    if (code > 0xFFFF)
        return 0;

    const FontJpGlyph *ui = FontJp_FindUi(code);
    if (ui != NULL)
    {
        if (rows != NULL)
            memcpy(rows, ui->rows, sizeof(ui->rows));
        return (ui->flags & FONTJP_GLYPH_WIDE) ? 2 : 1;
    }

    Draw_Lock();

    FontJpGlyph *glyph = NULL;

    for (u32 i = 0; i < FONTJP_CACHE_SIZE && glyph == NULL; ++i)
        if ((g_fontCache[i].flags & FONTJP_GLYPH_CACHED) && g_fontCache[i].code == code)
            glyph = &g_fontCache[i];

    if (glyph == NULL && FontJp_Available())
    {
        u16 page = g_fontPages[code >> 8];
        u16 record[1 + FONTJP_ROWS];

        if (page == 0xFFFF)
            memset(record, 0, sizeof(record));
        else if (!FontJp_Read(g_fontPagesOffset + ((u32)page * 256 + (code & 0xFF)) * FONTJP_RECORD_SIZE,
                              record, sizeof(record)))
            record[0] = 0;

        if (g_fontOpen)
        {
            while (g_fontCache[g_fontCacheHand].flags & FONTJP_GLYPH_USED)
            {
                g_fontCache[g_fontCacheHand].flags &= ~FONTJP_GLYPH_USED;
                g_fontCacheHand = (g_fontCacheHand + 1) % FONTJP_CACHE_SIZE;
            }
            glyph = &g_fontCache[g_fontCacheHand];
            g_fontCacheHand = (g_fontCacheHand + 1) % FONTJP_CACHE_SIZE;
            glyph->code = code;
            glyph->flags = (record[0] & 0xFF) | FONTJP_GLYPH_CACHED;
            memcpy(glyph->rows, record + 1, sizeof(glyph->rows));
        }
    }

    if (glyph != NULL)
        glyph->flags |= FONTJP_GLYPH_USED;

    if (glyph != NULL && (glyph->flags & FONTJP_GLYPH_EXISTS))
    {
        cols = (glyph->flags & FONTJP_GLYPH_WIDE) ? 2 : 1;
        if (rows != NULL)
            memcpy(rows, glyph->rows, sizeof(glyph->rows));
    }

    Draw_Unlock();
    return cols;
}

u32 FontJp_DecodeUtf8(const char *s, u32 *code)
{
    const u8 *p = (const u8 *)s;
    u32 len, c, min;

    if (p[0] >= 0xC2 && p[0] <= 0xDF)
    {
        len = 2;
        c = p[0] & 0x1F;
        min = 0x80;
    }
    else if (p[0] >= 0xE0 && p[0] <= 0xEF)
    {
        len = 3;
        c = p[0] & 0x0F;
        min = 0x800;
    }
    else if (p[0] >= 0xF0 && p[0] <= 0xF4)
    {
        len = 4;
        c = p[0] & 0x07;
        min = 0x10000;
    }
    else
        return 0;

    for (u32 i = 1; i < len; ++i)
    {
        if ((p[i] & 0xC0) != 0x80)
            return 0;
        c = (c << 6) | (p[i] & 0x3F);
    }

    // 冗長な表現、サロゲート、U+10FFFF より上は UTF-8 として正しくない
    if (c < min || (c >= 0xD800 && c <= 0xDFFF) || c > 0x10FFFF)
        return 0;

    *code = c;
    return len;
}

static bool FontJp_IsUtf8(const char *s)
{
    u32 code;

    while (*s != 0)
    {
        if ((u8)*s < 0x80)
            ++s;
        else
        {
            u32 len = FontJp_DecodeUtf8(s, &code);
            if (len == 0)
                return false;
            s += len;
        }
    }

    return true;
}

void FontJp_CopyUtf8(char *dst, u32 dstSize, const char *src)
{
    u32 n = 0, code;

    if (dstSize == 0)
        return;

    while (*src != 0)
    {
        u32 len = FontJp_DecodeUtf8(src, &code);
        if (len == 0)
            len = 1;
        if (n + len >= dstSize)
            break;
        for (u32 i = 0; i < len; ++i)
            dst[n++] = *src++;
    }

    dst[n] = 0;
}

static u32 FontJp_EncodeUtf8(char *dst, u32 code)
{
    if (code < 0x80)
    {
        dst[0] = (char)code;
        return 1;
    }
    if (code < 0x800)
    {
        dst[0] = (char)(0xC0 | (code >> 6));
        dst[1] = (char)(0x80 | (code & 0x3F));
        return 2;
    }

    dst[0] = (char)(0xE0 | (code >> 12));
    dst[1] = (char)(0x80 | ((code >> 6) & 0x3F));
    dst[2] = (char)(0x80 | (code & 0x3F));
    return 3;
}

// Shift-JIS（cp932）の 2 バイトの字を Unicode にする。字が無いときは 0
static u32 FontJp_SjisCode(u8 lead, u8 trail)
{
    u32 l = lead <= 0x9F ? lead - 0x81 : lead - 0xE0 + 0x1F;
    u32 slot;

    for (slot = 0; slot < FONTJP_SJIS_CACHE; ++slot)
        if (g_sjisLead[slot] == l + 1)
            break;

    if (slot == FONTJP_SJIS_CACHE)
    {
        slot = g_sjisNext;
        g_sjisNext = (g_sjisNext + 1) % FONTJP_SJIS_CACHE;
        g_sjisLead[slot] = 0;
        if (!FontJp_Read(g_fontSjisOffset + l * FONTJP_SJIS_TRAILS * 2, g_sjisRows[slot], sizeof(g_sjisRows[slot])))
            return 0;
        g_sjisLead[slot] = l + 1;
    }

    return g_sjisRows[slot][trail - 0x40];
}

void FontJp_ToUtf8(char *dst, u32 dstSize, const char *src)
{
    u32 n = 0;

    if ((u8)src[0] == 0xEF && (u8)src[1] == 0xBB && (u8)src[2] == 0xBF)
        src += 3;

    Draw_Lock();

    if (FontJp_IsUtf8(src) || !FontJp_Available())
    {
        FontJp_CopyUtf8(dst, dstSize, src);
        Draw_Unlock();
        return;
    }

    while (*src != 0 && dstSize > 0)
    {
        u8 c = (u8)*src++;
        u32 code = '?';
        char buf[3];

        if (c < 0x80)
            code = c;
        else if (c >= 0xA1 && c <= 0xDF)
            code = 0xFF61 + c - 0xA1;   // 半角カナ
        // 2 バイト目として使えないバイトが続くときは ? にし、そのバイトは次の字として読み直す
        else if (((c >= 0x81 && c <= 0x9F) || (c >= 0xE0 && c <= 0xFC)) && (u8)*src >= 0x40 && (u8)*src <= 0xFC)
        {
            code = FontJp_SjisCode(c, (u8)*src++);
            if (code == 0)
                code = '?';
        }

        u32 len = FontJp_EncodeUtf8(buf, code);
        if (n + len >= dstSize)
            break;
        memcpy(dst + n, buf, len);
        n += len;
    }

    if (dstSize > 0)
        dst[n] = 0;

    Draw_Unlock();
}

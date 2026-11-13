/*
*   This file is part of a modified version of Luma3DS (boot.firm_Customize-Model updater)
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "md.h"

// ---------------------------------------------------------------- 版の節

static bool Md_IsDigit(char c)
{
    return c >= '0' && c <= '9';
}

// 行の終わり（\n か文字列の終わり）と、\r を除いた行の終わり
static const char *Md_LineEnd(const char *s, const char *end, const char **next)
{
    const char *eol = memchr(s, '\n', (size_t)(end - s));
    if (eol == NULL)
        eol = end;
    *next = eol < end ? eol + 1 : end;
    return eol > s && eol[-1] == '\r' ? eol - 1 : eol;
}

// 「## v1.5 (2026-11-20)」なら true にし、版と日付を入れる
static bool Md_SectionHeading(const char *s, const char *e, MdSection *sec)
{
    if (e - s < 4 || s[0] != '#' || s[1] != '#' || s[2] != ' ')
        return false;
    s += 3;
    while (s < e && *s == ' ')
        ++s;

    const char *v = s;
    if (v < e && (*v == 'v' || *v == 'V'))
        ++v;
    if (v >= e || !Md_IsDigit(*v))
        return false;

    const char *t = v;
    while (t < e && (Md_IsDigit(*t) || *t == '.' || *t == '_' || (*t >= 'a' && *t <= 'z') || *t == '-'))
        ++t;
    snprintf(sec->tag, sizeof(sec->tag), "v%.*s", (int)(t - v), v);

    // 版の後ろの区切り（空白・- ・( など）を除いたものを日付にする
    while (t < e && (*t == ' ' || *t == '-' || *t == '(' || *t == ':' || *t == '/'))
        ++t;
    while (e > t && (e[-1] == ' ' || e[-1] == ')'))
        --e;
    snprintf(sec->date, sizeof(sec->date), "%.*s", (int)(e - t), t);
    return true;
}

int Md_Sections(const char *md, MdSection *out, int max)
{
    const char *end = md + strlen(md), *s = md, *next;
    int count = 0;
    MdSection cur;

    while (s < end && count < max)
    {
        const char *e = Md_LineEnd(s, end, &next);
        if (Md_SectionHeading(s, e, &cur))
        {
            if (count > 0)
                out[count - 1].len = (size_t)(s - out[count - 1].body);
            cur.body = next;
            cur.len = (size_t)(end - next);
            out[count++] = cur;
        }
        s = next;
    }
    if (count > 0 && count == max)
    {
        // max 個目の節は、次の版の見出しの前までにする
        for (s = out[count - 1].body; s < end; s = next)
        {
            const char *e = Md_LineEnd(s, end, &next);
            if (Md_SectionHeading(s, e, &cur))
            {
                out[count - 1].len = (size_t)(s - out[count - 1].body);
                break;
            }
        }
    }
    return count;
}

// ---------------------------------------------------------------- 文の中の飾り

// [s, e) の中で c が次に出てくる場所（無ければ NULL）
static const char *Md_Find(const char *s, const char *e, char c)
{
    return (const char *)memchr(s, c, (size_t)(e - s));
}

static const char *Md_FindStr(const char *s, const char *e, const char *pat)
{
    size_t n = strlen(pat);
    for (; s + n <= e; ++s)
        if (memcmp(s, pat, n) == 0)
            return s;
    return NULL;
}

static char *Md_Inline(char *out, const char *s, const char *e)
{
    while (s < e)
    {
        char c = *s;

        if (c == '\\' && s + 1 < e && strchr("\\`*_[]()#+-.!<>", s[1]) != NULL)
        {
            *out++ = s[1];
            s += 2;
        }
        else if (c == '`')
        {
            const char *close = Md_Find(s + 1, e, '`');
            if (close == NULL)
                *out++ = *s++;
            else
            {
                *out++ = MD_CODE;
                memcpy(out, s + 1, (size_t)(close - s - 1));
                out += close - s - 1;
                *out++ = MD_CODE;
                s = close + 1;
            }
        }
        else if ((c == '*' || c == '_') && s + 1 < e && s[1] == c)
        {
            // 閉じる ** があるときだけ太字にする
            const char pat[3] = { c, c, 0 };
            const char *close = Md_FindStr(s + 2, e, pat);
            if (close == NULL)
            {
                *out++ = *s++;
                *out++ = *s++;
            }
            else
            {
                *out++ = MD_BOLD;
                out = Md_Inline(out, s + 2, close);
                *out++ = MD_BOLD;
                s = close + 2;
            }
        }
        else if (c == '*' && s + 1 < e && s[1] != ' ')
        {
            // *斜体* は飾りを外すだけ（斜めの字は無い）。閉じる * が無ければそのまま。
            // _斜体_ は boot.firm_Customize-Model のような名前を壊すので扱わない
            const char *close = Md_Find(s + 1, e, c);
            if (close == NULL || close[-1] == ' ')
                *out++ = *s++;
            else
            {
                out = Md_Inline(out, s + 1, close);
                s = close + 1;
            }
        }
        else if (c == '[' || (c == '!' && s + 1 < e && s[1] == '['))
        {
            // [文字](URL) と ![説明](画像) は文字だけを出す
            const char *open = c == '!' ? s + 1 : s;
            const char *close = Md_Find(open, e, ']');
            const char *paren = close != NULL && close + 1 < e && close[1] == '(' ? Md_Find(close, e, ')') : NULL;
            if (paren == NULL)
                *out++ = *s++;
            else
            {
                *out++ = MD_LINK;
                out = Md_Inline(out, open + 1, close);
                *out++ = MD_LINK;
                s = paren + 1;
            }
        }
        else if (c == '<')
        {
            // <https://...> はそのまま URL を出し、<br> などのタグは消す
            const char *close = Md_Find(s, e, '>');
            if (close == NULL)
                *out++ = *s++;
            else if (Md_FindStr(s, close, "://") != NULL)
            {
                *out++ = MD_LINK;
                memcpy(out, s + 1, (size_t)(close - s - 1));
                out += close - s - 1;
                *out++ = MD_LINK;
                s = close + 1;
            }
            else if (close - s > 1 && (s[1] == '/' || (s[1] >= 'a' && s[1] <= 'z')))
                s = close + 1;
            else
                *out++ = *s++;
        }
        else
            *out++ = *s++;
    }
    return out;
}

// ---------------------------------------------------------------- 段落

typedef struct MdBuilder
{
    MdDoc   *doc;
    int     capacity;
    char    *out;           // pool の次に書くところ
    int     open;           // 行を続けて足せる段落（無ければ -1）
} MdBuilder;

static MdBlock *Md_Add(MdBuilder *b, MdKind kind, int depth, const char *marker)
{
    if (b->doc->count == b->capacity)
    {
        int cap = b->capacity == 0 ? 64 : b->capacity * 2;
        MdBlock *blocks = (MdBlock *)realloc(b->doc->blocks, (size_t)cap * sizeof(MdBlock));
        if (blocks == NULL)
            return NULL;
        b->doc->blocks = blocks;
        b->capacity = cap;
    }

    MdBlock *blk = &b->doc->blocks[b->doc->count++];
    blk->kind = kind;
    blk->depth = depth;
    snprintf(blk->marker, sizeof(blk->marker), "%s", marker != NULL ? marker : "");
    blk->text = b->out;
    *b->out = 0;
    return blk;
}

// 開いている段落の終わりに文を足す。英数字どうしの間だけ空白を入れる（日本語の文は空白なしでつなぐ）
static void Md_Append(MdBuilder *b, const char *s, const char *e)
{
    char *start = b->out;

    while (s < e && *s == ' ')
        ++s;
    while (e > s && e[-1] == ' ')
        --e;
    if (s == e)
        return;

    if (start > b->doc->blocks[b->open].text && (unsigned char)start[-1] < 0x80 && start[-1] != ' ' &&
        (unsigned char)*s < 0x80)
        *b->out++ = ' ';
    b->out = Md_Inline(b->out, s, e);
    *b->out = 0;
}

// 段落を閉じる（次の行は新しい段落になる）
static void Md_Close(MdBuilder *b)
{
    if (b->open >= 0)
        ++b->out;   // 終わりの 0 の次から書く
    b->open = -1;
}

static void Md_Start(MdBuilder *b, MdKind kind, int depth, const char *marker, const char *s, const char *e)
{
    Md_Close(b);
    if (Md_Add(b, kind, depth, marker) == NULL)
        return;
    b->open = b->doc->count - 1;
    Md_Append(b, s, e);
}

static bool Md_IsRule(const char *s, const char *e)
{
    char c = *s;
    int n = 0;

    if (c != '-' && c != '*' && c != '_')
        return false;
    for (; s < e; ++s)
    {
        if (*s == c)
            ++n;
        else if (*s != ' ')
            return false;
    }
    return n >= 3;
}

bool Md_Parse(const char *src, size_t len, MdDoc *doc)
{
    const char *end = src + len, *s = src, *next;
    MdBuilder b;
    bool fence = false;

    memset(doc, 0, sizeof(*doc));
    // 飾りの字は元の記号より短いので、元の長さと行の数（つなぐ空白と終わりの 0）で足りる
    doc->pool = (char *)malloc(len * 2 + 16);
    if (doc->pool == NULL)
        return false;
    b = (MdBuilder){ doc, 0, doc->pool, -1 };

    for (; s < end; s = next)
    {
        const char *e = Md_LineEnd(s, end, &next), *t = s;
        int indent = 0;

        while (t < e && (*t == ' ' || *t == '\t'))
        {
            indent += *t == '\t' ? 4 : 1;
            ++t;
        }

        if (e - t >= 3 && memcmp(t, "```", 3) == 0)
        {
            Md_Close(&b);
            fence = !fence;
            continue;
        }
        if (fence)
        {
            // コードはそのまま 1 行ずつ
            Md_Close(&b);
            MdBlock *blk = Md_Add(&b, MD_CODEBLOCK, 0, NULL);
            if (blk == NULL)
                break;
            memcpy(b.out, s, (size_t)(e - s));
            b.out += e - s;
            *b.out++ = 0;
            continue;
        }

        if (t == e)
        {
            Md_Close(&b);
            if (doc->count > 0 && doc->blocks[doc->count - 1].kind != MD_BLANK && Md_Add(&b, MD_BLANK, 0, NULL) != NULL)
                ++b.out;
            continue;
        }

        if (*t == '#')
        {
            const char *h = t;
            while (h < e && *h == '#')
                ++h;
            if (h - t <= 6 && (h == e || *h == ' '))
            {
                const char *te = e;
                while (te > h && (te[-1] == '#' || te[-1] == ' '))
                    --te;
                MdKind kind = h - t == 1 ? MD_H1 : h - t == 2 ? MD_H2 : MD_H3;
                Md_Start(&b, kind, 0, NULL, h, te);
                Md_Close(&b);
                continue;
            }
        }

        if (Md_IsRule(t, e))
        {
            Md_Close(&b);
            if (Md_Add(&b, MD_RULE, 0, NULL) != NULL)
                ++b.out;
            continue;
        }

        if (*t == '>')
        {
            int depth = 0;
            while (t < e && (*t == '>' || *t == ' '))
                depth += *t++ == '>';
            if (b.open >= 0 && doc->blocks[b.open].kind == MD_QUOTE && doc->blocks[b.open].depth == depth - 1)
                Md_Append(&b, t, e);
            else
                Md_Start(&b, MD_QUOTE, depth - 1, NULL, t, e);
            continue;
        }

        if (e - t >= 2 && (*t == '-' || *t == '*' || *t == '+') && t[1] == ' ')
        {
            Md_Start(&b, MD_BULLET, indent / 2, "・", t + 2, e);
            continue;
        }

        const char *d = t;
        while (d < e && Md_IsDigit(*d))
            ++d;
        if (d > t && d - t <= 3 && d + 1 < e && (*d == '.' || *d == ')') && d[1] == ' ')
        {
            char marker[8];
            snprintf(marker, sizeof(marker), "%.*s.", (int)(d - t), t);
            Md_Start(&b, MD_NUMBER, indent / 3, marker, d + 2, e);
            continue;
        }

        // 続きの行は、開いている段落・箇条書き・引用に足す
        if (b.open >= 0)
            Md_Append(&b, t, e);
        else
            Md_Start(&b, MD_PARA, 0, NULL, t, e);
    }

    Md_Close(&b);
    // 最後の空行は要らない
    while (doc->count > 0 && doc->blocks[doc->count - 1].kind == MD_BLANK)
        --doc->count;
    while (doc->count > 0 && doc->blocks[0].kind == MD_BLANK)
    {
        memmove(doc->blocks, doc->blocks + 1, (size_t)(doc->count - 1) * sizeof(MdBlock));
        --doc->count;
    }
    return true;
}

void Md_Free(MdDoc *doc)
{
    free(doc->blocks);
    free(doc->pool);
    memset(doc, 0, sizeof(*doc));
}

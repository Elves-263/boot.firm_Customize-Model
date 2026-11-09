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

#include <stdlib.h>
#include <string.h>
#include "json.h"

#define JSON_BLOCK_SIZE 0x10000
#define JSON_MAX_DEPTH  64

typedef struct JsonBlock
{
    struct JsonBlock    *next;
    size_t              used, size;
    unsigned char       data[];
} JsonBlock;

typedef struct JsonParser
{
    const char  *p, *end;
    JsonDoc     *doc;
    int         depth;
} JsonParser;

static void *Json_Alloc(JsonDoc *doc, size_t size)
{
    JsonBlock *b = (JsonBlock *)doc->blocks;

    size = (size + 7) & ~(size_t)7;
    if (b == NULL || b->size - b->used < size)
    {
        size_t blockSize = size > JSON_BLOCK_SIZE ? size : JSON_BLOCK_SIZE;
        JsonBlock *nb = (JsonBlock *)malloc(sizeof(JsonBlock) + blockSize);
        if (nb == NULL)
            return NULL;
        nb->next = b;
        nb->used = 0;
        nb->size = blockSize;
        doc->blocks = b = nb;
    }

    void *ptr = b->data + b->used;
    b->used += size;
    return ptr;
}

static void Json_SkipSpace(JsonParser *ps)
{
    while (ps->p < ps->end && (*ps->p == ' ' || *ps->p == '\t' || *ps->p == '\n' || *ps->p == '\r'))
        ++ps->p;
}

static bool Json_Literal(JsonParser *ps, const char *word)
{
    size_t n = strlen(word);

    if ((size_t)(ps->end - ps->p) < n || memcmp(ps->p, word, n) != 0)
        return false;
    ps->p += n;
    return true;
}

static int Json_Hex4(const char *p)
{
    int v = 0;

    for (int i = 0; i < 4; ++i)
    {
        char c = p[i];
        v <<= 4;
        if (c >= '0' && c <= '9')       v |= c - '0';
        else if (c >= 'a' && c <= 'f')  v |= c - 'a' + 10;
        else if (c >= 'A' && c <= 'F')  v |= c - 'A' + 10;
        else                            return -1;
    }
    return v;
}

static char *Json_PutUtf8(char *out, unsigned code)
{
    if (code < 0x80)
        *out++ = (char)code;
    else if (code < 0x800)
    {
        *out++ = (char)(0xC0 | (code >> 6));
        *out++ = (char)(0x80 | (code & 0x3F));
    }
    else if (code < 0x10000)
    {
        *out++ = (char)(0xE0 | (code >> 12));
        *out++ = (char)(0x80 | ((code >> 6) & 0x3F));
        *out++ = (char)(0x80 | (code & 0x3F));
    }
    else
    {
        *out++ = (char)(0xF0 | (code >> 18));
        *out++ = (char)(0x80 | ((code >> 12) & 0x3F));
        *out++ = (char)(0x80 | ((code >> 6) & 0x3F));
        *out++ = (char)(0x80 | (code & 0x3F));
    }
    return out;
}

// "..." を読み、ほどいた文字列を返す（ps->p は先頭の " を指していること）
static const char *Json_ParseString(JsonParser *ps)
{
    const char *start = ++ps->p, *q;
    char *out, *o;

    // 終わりの " を探す（ほどいた後は元より長くならないので、その長さで確保する）
    for (q = start; q < ps->end && *q != '"'; ++q)
    {
        if ((unsigned char)*q < 0x20)
            return NULL;
        if (*q == '\\' && ++q >= ps->end)
            return NULL;
    }
    if (q >= ps->end)
        return NULL;

    out = o = (char *)Json_Alloc(ps->doc, (size_t)(q - start) + 1);
    if (out == NULL)
        return NULL;

    for (const char *p = start; p < q; ++p)
    {
        if (*p != '\\')
        {
            *o++ = *p;
            continue;
        }

        switch (*++p)
        {
            case 'b': *o++ = '\b'; break;
            case 'f': *o++ = '\f'; break;
            case 'n': *o++ = '\n'; break;
            case 'r': *o++ = '\r'; break;
            case 't': *o++ = '\t'; break;
            case 'u':
            {
                int code = q - p >= 5 ? Json_Hex4(p + 1) : -1;
                if (code < 0)
                    return NULL;
                p += 4;

                // サロゲートの組は 1 字にまとめる。組になっていなければ ? にする
                if (code >= 0xD800 && code <= 0xDBFF)
                {
                    int low = q - p >= 7 && p[1] == '\\' && p[2] == 'u' ? Json_Hex4(p + 3) : -1;
                    if (low >= 0xDC00 && low <= 0xDFFF)
                    {
                        code = 0x10000 + ((code - 0xD800) << 10) + (low - 0xDC00);
                        p += 6;
                    }
                    else
                        code = '?';
                }
                else if ((code >= 0xDC00 && code <= 0xDFFF) || code == 0)
                    code = '?';

                o = Json_PutUtf8(o, (unsigned)code);
                break;
            }
            default: *o++ = *p; break;  // \" \\ \/
        }
    }

    *o = 0;
    ps->p = q + 1;
    return out;
}

static JsonValue *Json_ParseValue(JsonParser *ps);

static JsonValue *Json_ParseList(JsonParser *ps, JsonValue *v, bool object)
{
    JsonValue **tail = &v->u.child;
    char close = object ? '}' : ']';

    ++ps->p;
    Json_SkipSpace(ps);
    if (ps->p < ps->end && *ps->p == close)
    {
        ++ps->p;
        return v;
    }

    for (;;)
    {
        const char *key = NULL;
        JsonValue *item;

        if (object)
        {
            Json_SkipSpace(ps);
            if (ps->p >= ps->end || *ps->p != '"' || (key = Json_ParseString(ps)) == NULL)
                return NULL;
            Json_SkipSpace(ps);
            if (ps->p >= ps->end || *ps->p++ != ':')
                return NULL;
        }

        item = Json_ParseValue(ps);
        if (item == NULL)
            return NULL;
        item->key = key;
        *tail = item;
        tail = &item->next;

        Json_SkipSpace(ps);
        if (ps->p >= ps->end)
            return NULL;
        if (*ps->p == close)
        {
            ++ps->p;
            return v;
        }
        if (*ps->p++ != ',')
            return NULL;
    }
}

static JsonValue *Json_ParseValue(JsonParser *ps)
{
    JsonValue *v;

    Json_SkipSpace(ps);
    if (ps->p >= ps->end || ps->depth >= JSON_MAX_DEPTH)
        return NULL;

    v = (JsonValue *)Json_Alloc(ps->doc, sizeof(JsonValue));
    if (v == NULL)
        return NULL;
    memset(v, 0, sizeof(*v));

    switch (*ps->p)
    {
        case '{':
        case '[':
        {
            JsonValue *res;
            bool object = *ps->p == '{';

            v->type = object ? JSON_OBJECT : JSON_ARRAY;
            ++ps->depth;
            res = Json_ParseList(ps, v, object);
            --ps->depth;
            return res;
        }
        case '"':
            v->type = JSON_STRING;
            v->u.string = Json_ParseString(ps);
            return v->u.string != NULL ? v : NULL;
        case 't':
            v->type = JSON_BOOL;
            v->u.boolean = true;
            return Json_Literal(ps, "true") ? v : NULL;
        case 'f':
            v->type = JSON_BOOL;
            return Json_Literal(ps, "false") ? v : NULL;
        case 'n':
            v->type = JSON_NULL;
            return Json_Literal(ps, "null") ? v : NULL;
        default:
        {
            char num[40];
            size_t n = 0;
            char *end;

            while (ps->p < ps->end && n + 1 < sizeof(num) && strchr("+-0123456789.eE", *ps->p) != NULL)
                num[n++] = *ps->p++;
            num[n] = 0;
            v->type = JSON_NUMBER;
            v->u.number = strtod(num, &end);
            return n != 0 && *end == 0 ? v : NULL;
        }
    }
}

bool Json_Parse(JsonDoc *doc, const char *text, size_t len)
{
    JsonParser ps = { text, text + len, doc, 0 };

    doc->root = NULL;
    doc->blocks = NULL;

    doc->root = Json_ParseValue(&ps);
    Json_SkipSpace(&ps);
    if (doc->root == NULL || ps.p != ps.end)
    {
        Json_Free(doc);
        return false;
    }
    return true;
}

void Json_Free(JsonDoc *doc)
{
    JsonBlock *b = (JsonBlock *)doc->blocks;

    while (b != NULL)
    {
        JsonBlock *next = b->next;
        free(b);
        b = next;
    }
    doc->root = NULL;
    doc->blocks = NULL;
}

const JsonValue *Json_Get(const JsonValue *obj, const char *key)
{
    if (obj == NULL || obj->type != JSON_OBJECT)
        return NULL;
    for (const JsonValue *v = obj->u.child; v != NULL; v = v->next)
        if (strcmp(v->key, key) == 0)
            return v;
    return NULL;
}

const char *Json_String(const JsonValue *v, const char *def)
{
    return v != NULL && v->type == JSON_STRING ? v->u.string : def;
}

double Json_Number(const JsonValue *v, double def)
{
    return v != NULL && v->type == JSON_NUMBER ? v->u.number : def;
}

bool Json_Bool(const JsonValue *v, bool def)
{
    return v != NULL && v->type == JSON_BOOL ? v->u.boolean : def;
}

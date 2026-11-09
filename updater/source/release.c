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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "release.h"
#include "json.h"

#ifndef RELEASE_API_BASE
#define RELEASE_API_BASE    "https://api.github.com"    // PC での試験では手元のサーバーにする
#endif
#define RELEASE_JSON_MAX    (4 << 20)   // 一覧の応答の上限

static void Release_Copy(char *dst, size_t size, const char *src)
{
    size_t n = strlen(src);

    if (n >= size)
        n = size - 1;
    memcpy(dst, src, n);
    dst[n] = 0;
}

static bool Release_Read(Release *r, const JsonValue *v)
{
    const char *tag = Json_String(Json_Get(v, "tag_name"), "");
    const char *title = Json_String(Json_Get(v, "name"), NULL);
    const char *date = Json_String(Json_Get(v, "published_at"), "");
    const char *notes = Json_String(Json_Get(v, "body"), "");
    const JsonValue *assets = Json_Get(v, "assets");

    memset(r, 0, sizeof(*r));
    if (tag[0] == 0 || strlen(tag) >= sizeof(r->tag) || Json_Bool(Json_Get(v, "draft"), false))
        return false;

    strcpy(r->tag, tag);
    Release_Copy(r->title, sizeof(r->title), title != NULL && title[0] != 0 ? title : tag);
    Release_Copy(r->date, sizeof(r->date), date);
    Release_Copy(r->published, sizeof(r->published), date);
    r->prerelease = Json_Bool(Json_Get(v, "prerelease"), false);

    r->notes = strdup(notes);
    if (r->notes == NULL)
        return false;

    if (assets != NULL && assets->type == JSON_ARRAY)
    {
        for (const JsonValue *a = assets->u.child; a != NULL && r->assetCount < RELEASE_ASSET_MAX; a = a->next)
        {
            const char *name = Json_String(Json_Get(a, "name"), "");
            const char *url = Json_String(Json_Get(a, "browser_download_url"), "");
            ReleaseAsset *dst = &r->assets[r->assetCount];

            if (name[0] == 0 || strlen(name) >= sizeof(dst->name) || strlen(url) >= sizeof(dst->url))
                continue;
            strcpy(dst->name, name);
            strcpy(dst->url, url);
            dst->size = (uint64_t)Json_Number(Json_Get(a, "size"), 0);
            ++r->assetCount;
        }
    }
    return true;
}

static bool Release_IsOlder(const Release *a, const Release *b)
{
    int c = Version_Compare(a->tag, b->tag);
    return c < 0 || (c == 0 && strcmp(a->published, b->published) < 0);
}

HttpsResult Release_FetchList(ReleaseList *list, const char *repo, int *detail)
{
    char        url[256];
    HttpsSink   sink;
    HttpsBuffer buf;
    JsonDoc     doc;
    HttpsResult res;

    memset(list, 0, sizeof(*list));
    list->latest = -1;

    snprintf(url, sizeof(url), RELEASE_API_BASE "/repos/%s/releases?per_page=%d", repo, RELEASE_MAX);
    Https_BufferSink(&sink, &buf, RELEASE_JSON_MAX);
    res = Https_Get(url, "application/vnd.github+json", &sink, detail);
    if (res != HTTPS_OK)
    {
        Https_BufferFree(&buf);
        return res;
    }

    if (!Json_Parse(&doc, buf.data, buf.size) || doc.root->type != JSON_ARRAY)
    {
        Https_BufferFree(&buf);
        return HTTPS_ERR_PROTOCOL;
    }
    Https_BufferFree(&buf);

    for (const JsonValue *v = doc.root->u.child; v != NULL && list->count < RELEASE_MAX; v = v->next)
    {
        Release *r = &list->items[list->count];
        if (!Release_Read(r, v))
        {
            free(r->notes);
            r->notes = NULL;
            continue;
        }
        ++list->count;
    }
    Json_Free(&doc);

    // GitHub の一覧は版の順とは限らない（古い版のリリースを後から作ると上に来る）ので、版の新しい順に並べ直す。
    // 数字が同じ版（1.0 と 1.0_fix など）は、後に公開したほうを新しいとする
    for (int i = 1; i < list->count; ++i)
    {
        static Release tmp;
        int j = i;

        tmp = list->items[i];
        for (; j > 0 && Release_IsOlder(&list->items[j - 1], &tmp); --j)
            list->items[j] = list->items[j - 1];
        list->items[j] = tmp;
    }
    for (int i = 0; i < list->count && list->latest < 0; ++i)
        if (!list->items[i].prerelease)
            list->latest = i;

    return HTTPS_OK;
}

void Release_FreeList(ReleaseList *list)
{
    for (int i = 0; i < list->count; ++i)
        free(list->items[i].notes);
    memset(list, 0, sizeof(*list));
    list->latest = -1;
}

const ReleaseAsset *Release_FindAsset(const Release *r, const char *name)
{
    for (int i = 0; i < r->assetCount; ++i)
        if (strcmp(r->assets[i].name, name) == 0)
            return &r->assets[i];
    return NULL;
}

// ---------------------------------------------------------------- 版の比較

// v1.4 → { 1, 4, 0, 0 }。数字で始まらなければ false
static bool Version_Parse(const char *s, unsigned parts[4])
{
    memset(parts, 0, 4 * sizeof(unsigned));

    if (*s == 'v' || *s == 'V')
        ++s;
    if (*s < '0' || *s > '9')
        return false;

    for (unsigned i = 0; i < 4; ++i)
    {
        while (*s >= '0' && *s <= '9')
            parts[i] = parts[i] * 10 + (unsigned)(*s++ - '0');
        if (*s != '.' || s[1] < '0' || s[1] > '9')
            break;
        ++s;
    }
    return true;
}

int Version_Compare(const char *a, const char *b)
{
    unsigned pa[4], pb[4];
    bool oka = Version_Parse(a, pa), okb = Version_Parse(b, pb);

    if (!oka || !okb)
        return (int)oka - (int)okb;

    for (unsigned i = 0; i < 4; ++i)
        if (pa[i] != pb[i])
            return pa[i] > pb[i] ? 1 : -1;
    return 0;
}

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

// GitHub の Releases から、版の一覧・更新内容・添付ファイルを取る

#include <stdbool.h>
#include <stdint.h>
#include "https.h"

// リリースに添付するファイルの名前（build.bat の release フォルダと合わせる）。
// boot.firm はこのアプリの中に入っているので、アプリを入れ直せば両方の版がそろう
#define ASSET_CIA       "Luma3DS_New_Updater.cia"

#define RELEASE_MAX         10      // 一覧に出す版の数（新しい順）
#define RELEASE_ASSET_MAX   16
#define RELEASE_TAG_SIZE    32

typedef struct ReleaseAsset
{
    char        name[64];
    char        url[384];
    uint64_t    size;
} ReleaseAsset;

typedef struct Release
{
    char            tag[RELEASE_TAG_SIZE];
    char            title[128];
    char            date[11];       // 公開日（2026-11-02、UTC）
    char            published[21];  // 公開日時（2026-11-02T03:10:00Z）。同じ版どうしを並べるのに使う
    bool            prerelease;
    char            *notes;         // 更新内容（malloc。無ければ空の文字列）
    ReleaseAsset    assets[RELEASE_ASSET_MAX];
    int             assetCount;
} Release;

typedef struct ReleaseList
{
    Release items[RELEASE_MAX];
    int     count;
    int     latest;                 // プレリリースを除いた一番新しい版（無ければ -1）
} ReleaseList;

// repo（所有者/名前）のリリースを取り、版の新しい順に並べる。応答が読めなければ HTTPS_ERR_PROTOCOL
HttpsResult Release_FetchList(ReleaseList *list, const char *repo, int *detail);
void Release_FreeList(ReleaseList *list);

const ReleaseAsset *Release_FindAsset(const Release *r, const char *name);

// 版の名前（v1.4、1.10.2 など）を、数字の並びとして比べる。a が新しければ正、同じなら 0、古ければ負。
// 数字で始まらない名前は、どの版よりも古いものとして扱う（Rosalina の update_tls.c と同じ）
int Version_Compare(const char *a, const char *b);

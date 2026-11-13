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

#pragma once

// SD カードの上の boot.firm_Customize-Model のファイルと、Rosalina の起動時の確認（/luma/update/）のオン・オフ

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifndef SD_ROOT
#define SD_ROOT "sdmc:"     // PC での試験では別のフォルダにする
#endif
#ifndef BUNDLE_ROOT
#define BUNDLE_ROOT "romfs:"    // このアプリに入れた boot.firm（build.bat が romfs に入れる）
#endif

// Rosalina の update_check.h と合わせる
#define UPDATE_DIR          SD_ROOT "/luma/update"
#define UPDATE_OFF_PATH     UPDATE_DIR "/off.flag"
#define UPDATE_RUNNING_PATH UPDATE_DIR "/running.txt"
// 更新アプリだけが使う
#define UPDATE_TMP_DIR      UPDATE_DIR "/tmp"       // ダウンロードした .cia（入れたら消す）
#define UPDATE_BACKUP_DIR   UPDATE_DIR "/backup"    // 差し替える前のファイル
#define UPDATE_LIGHT_PATH   UPDATE_DIR "/light.flag"    // 有れば、画面の色を白×金にする
#define UPDATE_PENDING_PATH UPDATE_DIR "/pending.txt"   // アプリを入れ直したあと、続けて boot.firm を入れる版

typedef enum Part
{
    PART_ROOT_FIRM,     // SD:/boot.firm（いま起動に使う版の写し）
    PART_FIRM_NEW,      // SD:/3ds/luma/new/boot.firm（新しい方の Luma。上流の版が変わってもこの名前）
    PART_FIRM_91,       // SD:/3ds/luma/9.1/boot.firm
    PART_COUNT,
} Part;

typedef struct LocalState
{
    bool        same[PART_COUNT];   // このアプリに入れた boot.firm と同じ中身か

    // SD:/boot.firm をどちらの版にするか（"new"・"9.1"）。公式の Luma など、どちらの版の写しか
    // 分からないときは "new"
    const char  *rootLuma;

    // Rosalina が書く running.txt（動いている改変版の版と Luma の版。無ければ空）
    char        runningTag[32];
    char        runningLuma[8];

    // 起動時の確認（off.flag が無ければオン）
    bool        autoCheck;
    // 画面の色（light.flag が有れば白×金、無ければ黒×金）
    bool        lightTheme;
} LocalState;

// ファイルを読んでこのアプリに入れた boot.firm と比べ、状態をまとめる
void Local_Read(LocalState *st);
// このアプリに入れた boot.firm と違うファイル（無いものも含む）があるか
bool Local_NeedsInstall(const LocalState *st);

const char *Part_Path(Part part);
// そのファイルに写す、このアプリに入れた boot.firm
const char *Part_Bundle(Part part, const LocalState *st);

// 起動時の確認のオン・オフ（off.flag を消す・作る）
bool Local_SetAutoCheck(bool on);
// 画面の色を覚える（light.flag を作る・消す）
bool Local_SetLightTheme(bool on);

// アプリを tag の版に入れ直す前に書いておく。入れ直したアプリが起動したとき、自分の版と同じなら
// 続けて boot.firm を入れる。Local_TakePending は読んで消す（無ければ空）
bool Local_SetPending(const char *tag);
void Local_TakePending(char *tag, size_t size);

// 2 つのファイルの中身が同じか。どちらかが読めなければ false
bool Local_SameFile(const char *a, const char *b);

// 前の版が使っていたもの（v1.4 より前の SD:/3ds/luma/13.4/ の boot.firm とフォルダ、v1.4 の /luma/lang.cfg）を消す
void Local_RemoveLegacy(void);
// path の親のフォルダを順に作る
bool Local_MakeParentDirs(const char *path);

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

// このアプリに入れた boot.firm を SD カードのファイルと差し替える。新しい版の .cia をダウンロードして入れる

#include <stdbool.h>
#include <stdint.h>
#include "release.h"

typedef enum InstallResult
{
    INSTALL_OK = 0,
    INSTALL_ERR_NET,        // ダウンロードの失敗（detail は HttpsResult、detail2 はその詳細）
    INSTALL_ERR_FORMAT,     // 大きさがリリースの添付ファイルと違う
    INSTALL_ERR_WRITE,      // SD カードに書けない
    INSTALL_ERR_CIA,        // .cia を入れられない（detail は Result）
    INSTALL_ERR_CANCELLED,
} InstallResult;

typedef struct InstallProgress
{
    // done / total バイトまで進んだ（total が 0 なら大きさ不明）
    void (*update)(void *ctx, uint64_t done, uint64_t total);
    // 中止するなら true（ダウンロードの間だけ見る）
    bool (*cancelled)(void *ctx);
    void *ctx;
} InstallProgress;

// asset を path にダウンロードし、大きさがリリースの添付ファイルと同じか確かめる。違えば消す
InstallResult Install_Download(const ReleaseAsset *asset, const char *path,
                               const InstallProgress *progress, int *detail, int *detail2);

// staged を target に写す。target があれば UPDATE_BACKUP_DIR/backupName に残してから差し替え、
// 写したものが staged と同じ中身であることを確かめる
InstallResult Install_ReplaceFile(const char *staged, const char *target, const char *backupName);

// .cia を入れる（3DS だけ）
InstallResult Install_Cia(const char *path, const InstallProgress *progress, int *detail);

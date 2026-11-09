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

#pragma once

#include <3ds/types.h>
#include "MyThread.h"

// 起動時の新しい版の確認。Wi-Fi に繋がったあと 1 度だけ GitHub の最新のリリースの名前を取り、
// 動いている版より新しければ、Rosalina メニューを開いたときとお知らせリストで知らせる。
// 取得先のリポジトリは UPDATE_REPO（build.bat で設定）。空ならこの機能は動かない。
//
// SD カードの /luma/update/ に置くファイル（更新アプリ updater/ と共通）
//   off.flag       有ると起動時に確かめない（更新アプリで切り替える）
//   running.txt    動いている版（1 行目が改変版の版、2 行目が Luma の版）。更新アプリが読む
//   check.txt      最後に確かめた結果（time= ミリ秒（1900 年から）、result=、code=、latest=）
//   notified.txt   お知らせリストに送った版（同じ版は 2 度送らない）
#define UPDATE_DIR          "/luma/update"
#define UPDATER_TITLE_ID    0x000400000FF15A00ULL   // 更新アプリ（updater/updater.rsf の UniqueId と合わせる）

MyThread *updateCheckCreateThread(void);
// 新しい版が見つかっていれば知らせる。起動ごとに 1 度だけ（Rosalina メニューを開いたときに呼ぶ）
void UpdateCheck_ShowNotice(void);

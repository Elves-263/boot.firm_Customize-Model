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

// 改変版（boot.firm_Customize-Model）で、新しい方と v9.1 の Rosalina に共通のもの。
// 版ごとに違う名前（FORK_LUMA_ID など）は、それぞれの版の include/menu.h にある

#include <3ds/types.h>

// 改変版の版。更新アプリの版と GitHub のリリースのタグもこれに合わせる（build.bat がここから読む）
#define FORK_PACKAGE_TAG   "v1.5"

// menus/fork_menus.c
extern bool g_safeMode;
void LogLine(const char *msg);
void SDSpaceWarnIfLow(void);
void ChainloadCheckFailed(void);
void ChainloadWarnIfFailed(void);
void RosalinaMenu_FileBrowser(void);
void RosalinaMenu_ChainloadVersionToggle(void);

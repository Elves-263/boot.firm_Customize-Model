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

// common/rosalina（新しい方と v9.1 で共通の、改変版のコード）は新しい方の名前で書いてあるので、
// v9.1 の名前に読み替える。ボタンは libctru の KEY_*（menu.h の BUTTON_* と同じ値）をそのまま使う

#include <3ds.h>
#include "menu.h"
#include "minisoc.h"
#include "ifile.h"

#define menuShouldExit          terminationRequest
#define preTerminationRequested terminationRequest
#define preTerminationEvent     terminationRequestEvent

#define socSend                 soc_send
#define socRecv                 soc_recv
#define socGethostid            gethostid

#define CTR_ALIGN               ALIGN

// v9.1 のメニューが見るのは十字ボタンだけ（menu.h の BUTTON_UP など）。libctru の KEY_UP などは
// スライドパッドも含むので、十字ボタンだけのほうに読み替える
#define KEY_UP                  KEY_DUP
#define KEY_DOWN                KEY_DDOWN
#define KEY_LEFT                KEY_DLEFT
#define KEY_RIGHT               KEY_DRIGHT

// v9.1 では、メニューの外（起動のあと）から呼ばれる関数が自分で表示の準備をする
#define ForkCompat_MenuEnter()  menuEnter()
#define ForkCompat_MenuLeave()  menuLeave()

static inline bool isServiceUsable(const char *name)
{
    bool r;
    return R_SUCCEEDED(srvIsServiceRegistered(&r, name)) && r;
}

static inline Result IFile_SetSize(IFile *file, u64 size)
{
    return FSFILE_SetSize(file->handle, size);
}

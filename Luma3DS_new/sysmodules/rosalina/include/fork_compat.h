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

// common/rosalina（新しい方と v9.1 で共通の、改変版のコード）は、この版の名前（KEY_B、menuShouldExit、
// socSend など）で書いてある。v9.1 では、同じ名前のヘッダーが v9.1 の名前に読み替える

// メニューから呼ばれる関数は、メニューの側が表示の準備を済ませている
#define ForkCompat_MenuEnter()  ((void)0)
#define ForkCompat_MenuLeave()  ((void)0)

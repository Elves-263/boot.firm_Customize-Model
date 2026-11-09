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

// 画面の言葉の言語。Rosalina の「言語 / Language」と同じ /luma/lang.cfg（0: 英語、1: 日本語）を読み、
// 無ければ本体の言語が日本語なら日本語、それ以外は英語にして lang.cfg に書く。アプリを起動している間は変えない

#include <stdbool.h>

extern bool g_langEn;

// 日本語の文と英語の文のどちらかを選ぶ
#define TR(ja, en) (g_langEn ? (en) : (ja))

void Lang_Init(void);

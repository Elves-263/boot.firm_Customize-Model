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

// UTF-8 の文字列の読み取り（Markdown は md.c）

#include <stddef.h>
#include <stdint.h>

// s の先頭の 1 字を *code に入れ、バイト数を返す（UTF-8 として正しくなければ 1 バイトを 1 字とする）
size_t Text_Decode(const char *s, uint32_t *code);

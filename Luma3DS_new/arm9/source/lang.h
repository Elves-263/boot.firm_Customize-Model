/*
*   This file is part of a modified version of Luma3DS (Luma3DS_New)
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

// 起動時の設定画面などの言葉の言語。Rosalina の「言語 / Language」と同じ /luma/lang.cfg
// （0: 英語、1: 日本語）を読む。この画面は本体の設定を読む前に出るので、lang.cfg が無いときは日本語にする
// （Rosalina と更新アプリが、初めて本体の言語を読んだときに lang.cfg を作る）。エラーの画面は英語のまま

#include "types.h"

extern bool langEnglish;

// 日本語の文と英語の文のどちらかを選ぶ
#define TR(ja, en) (langEnglish ? (en) : (ja))

void initLanguage(void);

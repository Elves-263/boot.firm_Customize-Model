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

// GitHub の「更新.md」を読む。「## v1.5」のような見出しごとに 1 つの版の節に分け、
// 節の中の Markdown を、画面に出す段落（見出し・箇条書き・引用など）に直す。
// 3DS の機能は使わないので PC でも動く（tools/preview.c）

#include <stdbool.h>
#include <stddef.h>

// 文の中の飾り。同じ字がもう一度出てくるまでがその飾り（画面の字の幅は 0）
#define MD_BOLD     '\x02'      // **太字**・__太字__
#define MD_CODE     '\x03'      // `コード`
#define MD_LINK     '\x04'      // [文字](URL) の文字（URL は出さない）

typedef enum MdKind
{
    MD_PARA,        // 段落
    MD_H1,          // # 見出し
    MD_H2,          // ## 見出し
    MD_H3,          // ### 以下の見出し
    MD_BULLET,      // - * + の箇条書き
    MD_NUMBER,      // 1. の番号付き
    MD_QUOTE,       // > 引用
    MD_CODEBLOCK,   // ``` で囲んだコード（1 行ずつ）
    MD_RULE,        // --- の区切り線
    MD_BLANK,       // 空行（段落の間）
} MdKind;

typedef struct MdBlock
{
    MdKind  kind;
    int     depth;          // 箇条書き・引用の入れ子の深さ（0 から）
    char    marker[8];      // 箇条書きの「・」、番号付きの「1.」
    char    *text;          // 飾りを MD_BOLD などに直した文（MdDoc の pool の中）
} MdBlock;

typedef struct MdDoc
{
    MdBlock *blocks;
    int     count;
    char    *pool;
} MdDoc;

// 1 つの版の節
typedef struct MdSection
{
    char        tag[24];    // 「v1.5」
    char        date[24];   // 見出しの版の後ろ（「2026-11-20」など。無ければ空）
    const char  *body;      // 見出しの次の行から、次の版の見出しの前まで
    size_t      len;
} MdSection;

// 「## v1.5」「## 1.5」の見出しで分ける。最初の版の見出しより前（ファイルの題など）は使わない。
// 見つけた節の数を返す（max まで）
int Md_Sections(const char *md, MdSection *out, int max);

bool Md_Parse(const char *src, size_t len, MdDoc *doc);
void Md_Free(MdDoc *doc);

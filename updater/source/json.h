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

// GitHub の API の応答を読むための小さな JSON の読み取り。値は 1 つの塊（JsonDoc）にまとめて確保し、
// Json_Free でまとめて放す。文字列は UTF-8 にほどいて終端を付ける（\u0000 は ? にする）

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum JsonType
{
    JSON_NULL,
    JSON_BOOL,
    JSON_NUMBER,
    JSON_STRING,
    JSON_ARRAY,
    JSON_OBJECT,
} JsonType;

typedef struct JsonValue JsonValue;

struct JsonValue
{
    JsonType    type;
    const char  *key;       // オブジェクトの中の値のときの名前
    JsonValue   *next;      // 配列・オブジェクトの中の次の値
    union
    {
        bool        boolean;
        double      number;
        const char  *string;
        JsonValue   *child;  // 配列・オブジェクトの最初の値
    } u;
};

typedef struct JsonDoc
{
    JsonValue   *root;
    void        *blocks;    // 確保した塊の並び（Json_Free で放す）
} JsonDoc;

// text（len バイト）を読む。失敗は false（doc は空）
bool Json_Parse(JsonDoc *doc, const char *text, size_t len);
void Json_Free(JsonDoc *doc);

// オブジェクトの key の値（無い、オブジェクトでなければ NULL）
const JsonValue *Json_Get(const JsonValue *obj, const char *key);
// 文字列の値（文字列でなければ def）
const char *Json_String(const JsonValue *v, const char *def);
// 数の値（数でなければ def）
double Json_Number(const JsonValue *v, double def);
bool Json_Bool(const JsonValue *v, bool def);

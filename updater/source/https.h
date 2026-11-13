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

// HTTPS の GET。3DS 本体の HTTPS（http:C）は TLS 1.1 までで GitHub に繋がらないため、
// ソケットの上で BearSSL（TLS 1.2）を使う。証明書は trust_anchors.c のルート証明書で確かめる。
// 転送（3xx）は 5 回までたどり、chunked にも対応する。PC でも動く（試験用）

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct HttpsSink
{
    // 本文の長さが分かったときに呼ぶ（分からなければ -1）。false で中止
    bool (*begin)(void *ctx, int64_t length);
    // 本文を少しずつ渡す。false で中止
    bool (*write)(void *ctx, const void *data, size_t len);
    void *ctx;
} HttpsSink;

typedef enum HttpsResult
{
    HTTPS_OK = 0,
    HTTPS_ERR_URL,          // URL が https:// で始まらない、長すぎる
    HTTPS_ERR_DNS,          // 名前を引けない
    HTTPS_ERR_CONNECT,      // 繋がらない
    HTTPS_ERR_TLS,          // TLS の失敗（detail は BearSSL のエラー番号）
    HTTPS_ERR_HTTP,         // 2xx でない（detail は状態番号）
    HTTPS_ERR_PROTOCOL,     // 応答が読めない、途中で切れた
    HTTPS_ERR_REDIRECTS,    // 転送が多すぎる
    HTTPS_ERR_ABORTED,      // HttpsSink が中止した
    HTTPS_ERR_MEMORY,
} HttpsResult;

// ソケットを使う前後に呼ぶ（3DS では soc:U を開く）
bool Https_Init(void);
void Https_Exit(void);

// url を GET して本文を sink に渡す。accept は Accept ヘッダー（NULL なら */*）
HttpsResult Https_Get(const char *url, const char *accept, const HttpsSink *sink, int *detail);

// 本文をメモリに受ける HttpsSink（*data は malloc。maxSize を超えたら中止）
typedef struct HttpsBuffer
{
    char    *data;      // 終端の 0 を付ける
    size_t  size;
    size_t  capacity;
    size_t  maxSize;
} HttpsBuffer;

void Https_BufferSink(HttpsSink *sink, HttpsBuffer *buf, size_t maxSize);
void Https_BufferFree(HttpsBuffer *buf);

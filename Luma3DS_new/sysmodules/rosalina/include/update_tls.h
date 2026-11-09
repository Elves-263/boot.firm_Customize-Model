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

// GitHub に出ている最新の版の名前を、TLS 1.2（BearSSL）で取りにいく部分。
// 3DS 本体の HTTPS（http:C）は TLS 1.1 までで GitHub に繋がらないため、ソケットの上で自分で TLS を話す。
// ソケットの読み書きは呼ぶ側が渡すので、この部分は 3DS に依らない（PC でも試せる）

#include <stddef.h>

#define UPDATE_TAG_SIZE     24      // 版の名前（v1.5 など）に使えるバイト数（終端を含む）
#define UPDATE_TLS_MEM_SIZE 0x5800  // 確認に使うメモリ（TLS の状態と受信の場所）。呼ぶ側が使うときだけ確保する

typedef struct UpdateTlsIo
{
    // 最大 len バイトを読む（書く）。読み書きしたバイト数を返し、失敗や時間切れは -1
    int  (*read)(void *ctx, unsigned char *buf, size_t len);
    int  (*write)(void *ctx, const unsigned char *buf, size_t len);
    void *ctx;
} UpdateTlsIo;

enum
{
    UPDATE_TLS_OK = 0,
    UPDATE_TLS_ERR_TLS,     // TLS か通信の失敗。*detail は BearSSL のエラー番号
    UPDATE_TLS_ERR_HTTP,    // 転送（3xx と Location）の応答でない。*detail は HTTP の状態番号
    UPDATE_TLS_ERR_NOTAG,   // 転送先に版の名前が無い（リリースが 1 つも無いなど）
};

// https://<host>/<repo>/releases/latest を HEAD で開き、転送先の .../releases/tag/<版> から
// 版の名前を tag（UPDATE_TAG_SIZE バイト）に入れる。mem は UPDATE_TLS_MEM_SIZE バイト（8 バイト境界）、
// seed は乱数の種（32 バイト以上）。サーバーの証明書は確かめない（確認で得るのは版の名前だけで、
// ダウンロードは更新アプリが証明書を確かめたうえで行い、SHA-256 も照らし合わせるため）
int UpdateTls_GetLatestTag(void *mem, const UpdateTlsIo *io, const char *host, const char *repo,
                           const char *userAgent, const void *seed, size_t seedLen, char *tag, int *detail);

// 版の名前（v1.4、1.10.2 など）を、数字の並びとして比べる。a が新しければ正、同じなら 0、古ければ負。
// 数字で始まらない名前は、どの版よりも古いものとして扱う
int UpdateVersion_Compare(const char *a, const char *b);

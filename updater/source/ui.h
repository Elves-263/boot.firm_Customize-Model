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

// 画面の見た目。Rosalina と同じく地の色に文字だけを並べ、選んでいる行に > を付ける。色は黒×金と白×金の 2 つ。
// UiState の中身どおりに両方の画面を描き、タッチされたものを返す。
// 3DS の機能は使わないので、PC でも見本を作れる（tools/preview.c）

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "md.h"

#define UI_ITEM_MAX     8
#define UI_VERSION_MAX  32
#define UI_QR_MAX       512     // qrcodegen の版 10 まで（qrcodegen_BUFFER_LEN_FOR_VERSION(10) = 406）

typedef enum UiView
{
    UI_VIEW_MAIN,
    UI_VIEW_NOTES,      // 1 つの版の更新内容（上の画面）
    UI_VIEW_HISTORY,    // 版の一覧（下の画面）と、選んでいる版の更新内容（上の画面）
    UI_VIEW_SOURCE,     // ソースコードの場所と QR コード
} UiView;

// 上の画面の状態の行（色が変わる）
typedef enum UiStatus
{
    UI_STATUS_BUSY,     // 確かめている
    UI_STATUS_LATEST,   // 最新
    UI_STATUS_UPDATE,   // 新しいバージョンがある
    UI_STATUS_DONE,     // 入れた・更新した
    UI_STATUS_ERROR,    // 確かめられなかった
} UiStatus;

// 下の画面に重ねるもの
typedef enum UiOverlay
{
    UI_OVERLAY_NONE,
    UI_OVERLAY_PROGRESS,    // 進み具合
    UI_OVERLAY_CONFIRM,     // はい・いいえ
    UI_OVERLAY_MESSAGE,     // OK
} UiOverlay;

typedef struct UiItem
{
    int         id;
    const char  *label;
    const char  *value;     // 行の右に出す今の設定（ON など）。無ければ NULL
    const char  *help;      // 選んでいるとき、上の画面の下に出す説明
    bool        attention;  // 名前を目立つ色にする（「更新する」）
} UiItem;

// 更新内容を画面の幅で折り返した 1 行
typedef struct UiNoteLine
{
    const char  *text;
    size_t      len;
    uint8_t     kind;       // MdKind
    uint8_t     style;      // 行の始めで効いている飾り（UI_STYLE_*）
    int16_t     x;          // 字下げ（点）
    const char  *marker;    // 箇条書きの「・」・番号（段落の最初の行だけ。無ければ NULL）
} UiNoteLine;

#define UI_STYLE_BOLD   1
#define UI_STYLE_CODE   2
#define UI_STYLE_LINK   4

typedef struct UiVersion
{
    const char  *tag;
    const char  *date;
    bool        latest;     // 「最新」の印
    bool        current;    // 「使用中」の印
} UiVersion;

typedef struct UiState
{
    UiView      view;
    bool        light;      // 画面の色（false: 黒×金、true: 白×金）
    uint32_t    frame;      // 大きさの分からない進み具合の帯を動かす
    const char  *appVersion;

    // 最初の画面
    UiStatus    status;
    char        statusText[64];
    char        statusDetail[320];
    char        current[40];        // いまのバージョン（分からなければ「不明」など）
    char        latest[40];         // 最新のバージョン（分からなければ「-」など）
    UiItem      items[UI_ITEM_MAX];
    int         itemCount;
    int         selected;

    // 更新内容（UI_VIEW_NOTES と UI_VIEW_HISTORY の上の画面）
    char        notesTitle[64];
    const UiNoteLine *notes;
    int         noteCount;
    int         noteScroll;
    const char  *notesEmpty;        // 更新内容が無いときに出す文

    // 版の一覧
    UiVersion   versions[UI_VERSION_MAX];
    int         versionCount;
    int         versionSelected;
    int         versionScroll;

    // ソースコード
    const char  *sourceUrl;         // 無ければ NULL
    const char  *upstreamUrl;
    const uint8_t *qr;              // qrcodegen の QR コード（無ければ NULL）

    // 重ねるもの
    UiOverlay   overlay;
    char        overlayText[320];
    const char  *yesLabel;          // CONFIRM の「はい」と MESSAGE のボタン
    const char  *noLabel;           // CONFIRM の「いいえ」
    char        progressLabel[64];
    uint64_t    progressDone, progressTotal;
    bool        cancellable;
} UiState;

void Ui_Draw(const UiState *ui);

// Markdown の段落を、上の画面の更新内容の幅で折り返す。lines に入れた行の数を返す（max まで）
int Ui_LayoutNotes(const MdDoc *doc, UiNoteLine *lines, int max);
// 更新内容をいちばん下まで送ったときの noteScroll と、1 画面に出る行のおよその数
int Ui_NotesMaxScroll(const UiState *ui);
int Ui_NotesPage(void);
int Ui_VersionRows(void);

// 下の画面のタッチ。当たらなければ -1
int Ui_HitItem(const UiState *ui, int x, int y);
int Ui_HitVersion(const UiState *ui, int x, int y);
// 下の端の「A: …」（1）・「B: …」（0）・「X: …」か「Y: …」（2）。当たらなければ -1。重ねるものがあればその A・B
int Ui_HitButtons(const UiState *ui, int x, int y);

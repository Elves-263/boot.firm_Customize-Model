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

// boot.firm_Customize-Model の更新アプリ。GitHub の Releases で新しいバージョンを確かめ、リポジトリの「更新.md」で
// 更新内容と更新履歴を見せる。起動しただけでは何も入れず、「最新バージョンをインストール」を選んだときに、
// 新しいバージョンがあればこのアプリ自身を入れ直し（入れ直したアプリが、中に入っている boot.firm を入れる）、
// 無ければこのアプリに入れた boot.firm（新しい方と v9.1）を SD カードに入れる（公式の Luma3DS の boot.firm も
// 置き換える）。更新内容・更新履歴からは、選んだ版（前の版も）のアプリに入れ直せる。
// 画面は Rosalina と同じく文字だけ（ui.c）。言葉は短く、ファイルの名前や場所などの細かいことは出さない

#include <3ds.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "screen.h"
#include "ui.h"
#include "md.h"
#include "https.h"
#include "release.h"
#include "local.h"
#include "install.h"
#include "lang.h"
#include "qrcodegen/qrcodegen.h"

#ifndef UPDATE_REPO
#define UPDATE_REPO ""
#endif
#ifndef APP_VERSION
#define APP_VERSION "v0"
#endif
#ifndef LUMA_NEW_VERSION
#define LUMA_NEW_VERSION "?"
#endif

// 更新内容と更新履歴（リポジトリのいちばん上の「更新.md」。既定のブランチのもの）
#define CHANGELOG_URL   "https://raw.githubusercontent.com/" UPDATE_REPO "/HEAD/%E6%9B%B4%E6%96%B0.md"
#define CHANGELOG_MAX   (256 << 10)
#define SOURCE_URL      "https://github.com/" UPDATE_REPO
#define UPSTREAM_URL    "https://github.com/LumaTeam/Luma3DS"
#define NOTE_LINES_MAX  1024

#define MSG_RETRY       TR("時間をおいて、もう一度お試しください。", "Please try again later.")
#define MSG_UPDATING    TR("ソフトを入れ替えています。\n電源を切らないでください。", "Replacing the software.\nDo not turn off the power.")
#define MSG_INSTALLING  TR("boot.firm_Customize-Model を\n入れています。\n電源を切らないでください。", \
                           "Installing boot.firm_Customize-Model.\nDo not turn off the power.")

// 最初の画面の行
enum
{
    ITEM_INSTALL,
    ITEM_NOTES,
    ITEM_HISTORY,
    ITEM_SOURCE,
    ITEM_AUTO,
};

typedef struct App
{
    LocalState  local;
    ReleaseList releases;
    bool        fetched;        // 版の一覧を取れた
    UiState     ui;

    // 更新.md
    char        *changelog;
    MdSection   sections[UI_VERSION_MAX];
    int         sectionCount;
    MdDoc       doc;            // いま出している節
    UiNoteLine  lines[NOTE_LINES_MAX];

    uint8_t     qr[UI_QR_MAX];
} App;

static App g_app;

// ---------------------------------------------------------------- 状態

static const Release *App_Latest(const App *a)
{
    return a->fetched && a->releases.latest >= 0 ? &a->releases.items[a->releases.latest] : NULL;
}

// 「1.4」も「v1.4」も「v1.4」と出す
static void App_VersionLabel(char *out, size_t size, const char *tag)
{
    snprintf(out, size, "%s%s", tag[0] >= '0' && tag[0] <= '9' ? "v" : "", tag);
}

// 先頭の v を除いて同じ名前か（タグの「1.4」と Rosalina の「v1.4」を同じにする）
static bool App_SameTag(const char *a, const char *b)
{
    if (*a == 'v' || *a == 'V')
        ++a;
    if (*b == 'v' || *b == 'V')
        ++b;
    return *a != 0 && strcmp(a, b) == 0;
}

// 新しいバージョンのアプリがあるか
static bool App_HasUpdate(const App *a)
{
    const Release *r = App_Latest(a);
    return r != NULL && Version_Compare(r->tag, APP_VERSION) > 0 && Release_FindAsset(r, ASSET_CIA) != NULL;
}

// SD カードの boot.firm_Customize-Model がこのアプリより新しい（アプリを入れ直す前の古いアプリ）か
static bool App_IsOutdated(const App *a)
{
    return a->local.runningTag[0] != 0 && Version_Compare(a->local.runningTag, APP_VERSION) > 0;
}

// このアプリに入れた boot.firm を、まだ SD カードに入れていないか（SD カードの方が新しいときは除く）
static bool App_NeedsInstall(const App *a)
{
    return Local_NeedsInstall(&a->local) && !App_IsOutdated(a);
}

static void App_SetStatus(App *a, UiStatus status, const char *text, const char *detail)
{
    a->ui.status = status;
    snprintf(a->ui.statusText, sizeof(a->ui.statusText), "%s", text);
    snprintf(a->ui.statusDetail, sizeof(a->ui.statusDetail), "%s", detail);
}

// 通信の失敗を、どうすればよいか分かる文にする
static void App_NetErrorText(char *out, size_t size, HttpsResult res, int detail)
{
    switch (res)
    {
        case HTTPS_ERR_DNS:
        case HTTPS_ERR_CONNECT:
            snprintf(out, size, "%s", TR("インターネットに接続できませんでした。\nWi-Fi の設定を確認してください。",
                                         "Could not connect to the Internet.\nCheck your Wi-Fi settings."));
            break;
        case HTTPS_ERR_TLS:
            if (detail >= 54 && detail <= 62)   // 証明書の期限切れ・まだ有効でない など
                snprintf(out, size, "%s", TR("本体の日付と時刻が正しいか、確認してください。",
                                             "Check that the system date and time are correct."));
            else
                snprintf(out, size, "%s\n%s", TR("通信エラーが発生しました。", "A communication error occurred."), MSG_RETRY);
            break;
        case HTTPS_ERR_HTTP:
            if (detail == 403 || detail == 429)
                snprintf(out, size, "%s\n%s", TR("サーバーが混み合っています。", "The server is busy."), MSG_RETRY);
            else
                snprintf(out, size, "%s\n%s", TR("通信エラーが発生しました。", "A communication error occurred."), MSG_RETRY);
            break;
        case HTTPS_ERR_MEMORY:
            snprintf(out, size, "%s", TR("メモリが不足しています。\n本体を再起動してください。",
                                         "Not enough memory.\nPlease restart the system."));
            break;
        default:
            snprintf(out, size, "%s\n%s", TR("通信エラーが発生しました。", "A communication error occurred."), MSG_RETRY);
            break;
    }
}

// 上の画面の「現在のバージョン」「最新のバージョン」と、下の画面の行を今の状態にする
static void App_Refresh(App *a)
{
    const Release *latest = App_Latest(a);
    UiState *ui = &a->ui;
    int n = 0;

    if (!Local_NeedsInstall(&a->local))
        App_VersionLabel(ui->current, sizeof(ui->current), APP_VERSION);    // 中身がこのアプリと同じ
    else if (a->local.runningTag[0] != 0)
        App_VersionLabel(ui->current, sizeof(ui->current), a->local.runningTag);
    else
        snprintf(ui->current, sizeof(ui->current), "%s", TR("不明", "Unknown"));

    if (latest != NULL)
        App_VersionLabel(ui->latest, sizeof(ui->latest), latest->tag);
    else
        snprintf(ui->latest, sizeof(ui->latest), "-");

    // 入れるものがあれば「最新バージョンをインストール」、もう入っていれば「入れ直す」
    bool install = App_HasUpdate(a) || App_NeedsInstall(a);
    ui->items[n++] = (UiItem){ ITEM_INSTALL,
                               install ? TR("最新バージョンをインストール", "Install the latest version") : TR("入れ直す", "Reinstall"), NULL,
                               App_HasUpdate(a) ? TR("最新のバージョンをダウンロードして入れます。",
                                                     "Downloads the latest version and installs it to the SD card.") :
                               install ? TR("このソフトに入っている boot.firm_Customize-Model を、SD カードに入れます。",
                                            "Installs the boot.firm_Customize-Model included in this software to the SD card.") :
                                         TR("このソフトに入っている boot.firm_Customize-Model を、SD カードに入れ直します。",
                                            "Reinstalls the boot.firm_Customize-Model included in this software to the SD card."), install };
    ui->items[n++] = (UiItem){ ITEM_NOTES, TR("更新内容", "What's new"), NULL,
                               TR("新しいバージョンで変わったところを見ます。",
                                  "Shows what changed in the newest version."), false };
    ui->items[n++] = (UiItem){ ITEM_HISTORY, TR("更新履歴", "Version history"), NULL,
                               TR("これまでのバージョンの変更点を見ます。",
                                  "Shows the changes in earlier versions."), false };
    ui->items[n++] = (UiItem){ ITEM_SOURCE, TR("ソースコード", "Source code"), NULL,
                               TR("ソースコードの場所を、URL と QR コードで出します。",
                                  "Shows where to get the source code, as a URL and a QR code."), false };
    ui->items[n++] = (UiItem){ ITEM_AUTO, TR("更新のお知らせ", "Update notifications"), a->local.autoCheck ? "ON" : "OFF",
                               TR("ON にすると、本体を起動したときに新しいバージョンを確認し、あればお知らせします。",
                                  "When ON, checks for a new version at startup and lets you know if there is one."), false };
    ui->itemCount = n;
    if (ui->selected >= n)
        ui->selected = n - 1;
}

// ---------------------------------------------------------------- 描く・答えを待つ

static void App_Draw(App *a)
{
    ++a->ui.frame;
    Ui_Draw(&a->ui);
}

// タッチを始めた点（離したときではなく、触ったときに決める）
static bool App_Touched(int *x, int *y)
{
    touchPosition t;

    if (!(hidKeysDown() & KEY_TOUCH))
        return false;
    hidTouchRead(&t);
    *x = t.px;
    *y = t.py;
    return true;
}

static void App_SetOverlay(App *a, UiOverlay overlay, const char *text)
{
    a->ui.overlay = overlay;
    snprintf(a->ui.overlayText, sizeof(a->ui.overlayText), "%s", text);
}

// A（タッチなら「A: …」）なら true、B（「B: …」）なら false
static bool App_WaitAnswer(App *a)
{
    bool yes = false;

    while (aptMainLoop())
    {
        int x, y, hit = -1;

        hidScanInput();
        u32 down = hidKeysDown();
        if (App_Touched(&x, &y))
            hit = Ui_HitButtons(&a->ui, x, y);
        if ((down & (KEY_A | KEY_B)) || hit >= 0)
        {
            yes = hit >= 0 ? hit == 1 : (down & KEY_A) != 0;
            break;
        }
        App_Draw(a);
    }
    a->ui.overlay = UI_OVERLAY_NONE;
    return yes;
}

static bool App_Confirm(App *a, const char *text, const char *yes, const char *no)
{
    App_SetOverlay(a, UI_OVERLAY_CONFIRM, text);
    a->ui.yesLabel = yes;
    a->ui.noLabel = no;
    return App_WaitAnswer(a);
}

static void App_Message(App *a, const char *text)
{
    App_SetOverlay(a, UI_OVERLAY_MESSAGE, text);
    a->ui.yesLabel = TR("OK", "OK");
    App_WaitAnswer(a);
}

static void App_ProgressUpdate(void *ctx, uint64_t done, uint64_t total)
{
    App *a = (App *)ctx;

    a->ui.progressDone = done;
    a->ui.progressTotal = total;
    App_Draw(a);
}

static bool App_ProgressCancelled(void *ctx)
{
    App *a = (App *)ctx;
    int x, y;

    hidScanInput();
    if (!a->ui.cancellable)
        return false;
    return (hidKeysDown() & KEY_B) || (App_Touched(&x, &y) && Ui_HitButtons(&a->ui, x, y) == 0);
}

// ---------------------------------------------------------------- 確かめる

// 更新.md を取って版ごとに分ける。取れなくても版の確認は続ける
static void App_FetchChangelog(App *a)
{
    HttpsBuffer buf;
    HttpsSink sink;
    int detail;

    free(a->changelog);
    a->changelog = NULL;
    a->sectionCount = 0;

    Https_BufferSink(&sink, &buf, CHANGELOG_MAX);
    if (Https_Get(CHANGELOG_URL, "text/plain", &sink, &detail) != HTTPS_OK || buf.data == NULL)
    {
        Https_BufferFree(&buf);
        return;
    }
    a->changelog = buf.data;    // Https_BufferFree はしない（節が中を指す）
    a->sectionCount = Md_Sections(a->changelog, a->sections, UI_VERSION_MAX);
}

static void App_Fetch(App *a)
{
    const Release *latest;
    char text[256];
    int detail;
    HttpsResult res;

    Release_ClearList(&a->releases);
    a->fetched = false;
    App_SetStatus(a, UI_STATUS_BUSY, TR("確認しています", "Checking"), "");
    App_Refresh(a);
    App_Draw(a);

    if (UPDATE_REPO[0] == 0)
    {
        App_SetStatus(a, UI_STATUS_ERROR, TR("確認できませんでした", "Could not check"),
                      TR("確認先が設定されていません。", "No update source is set."));
        App_Refresh(a);
        return;
    }

    res = Release_FetchList(&a->releases, UPDATE_REPO, &detail);
    a->fetched = res == HTTPS_OK;
    latest = App_Latest(a);
    if (a->fetched)
        App_FetchChangelog(a);

    if (!a->fetched)
    {
        App_NetErrorText(text, sizeof(text), res, detail);
        App_SetStatus(a, UI_STATUS_ERROR, TR("確認できませんでした", "Could not check"), text);
    }
    else if (latest == NULL)
        App_SetStatus(a, UI_STATUS_ERROR, TR("確認できませんでした", "Could not check"),
                      TR("配信中のバージョンがありません。", "No version is available."));
    else if (App_HasUpdate(a))
    {
        char label[40];
        App_VersionLabel(label, sizeof(label), latest->tag);
        snprintf(text, sizeof(text), TR("「最新バージョンをインストール」で %s に更新します。",
                                        "Select \"Install the latest version\" to update to %s."), label);
        App_SetStatus(a, UI_STATUS_UPDATE, TR("新しいバージョンがあります", "A new version is available"), text);
        a->ui.selected = 0;     // 「最新バージョンをインストール」を選んでおく
    }
    else if (App_NeedsInstall(a))
    {
        char label[40];
        App_VersionLabel(label, sizeof(label), APP_VERSION);
        snprintf(text, sizeof(text), TR("「最新バージョンをインストール」で %s を入れます。",
                                        "Select \"Install the latest version\" to install %s to the SD card."), label);
        App_SetStatus(a, UI_STATUS_UPDATE, TR("まだインストールしていません", "Not installed yet"), text);
        a->ui.selected = 0;
    }
    else
        App_SetStatus(a, UI_STATUS_LATEST, TR("最新の状態です", "Up to date"), "");

    App_Refresh(a);
}

// ---------------------------------------------------------------- 更新内容・更新履歴

// index 番目の版の節を、上の画面に出せる行にする
static void App_LoadSection(App *a, int index)
{
    UiState *ui = &a->ui;
    const MdSection *sec = &a->sections[index];

    Md_Free(&a->doc);
    ui->notes = a->lines;
    ui->noteCount = 0;
    ui->noteScroll = 0;
    if (sec->date[0] != 0)
        snprintf(ui->notesTitle, sizeof(ui->notesTitle), TR("%s の更新内容（%s）", "What's new in %s (%s)"), sec->tag, sec->date);
    else
        snprintf(ui->notesTitle, sizeof(ui->notesTitle), TR("%s の更新内容", "What's new in %s"), sec->tag);

    if (Md_Parse(sec->body, sec->len, &a->doc))
        ui->noteCount = Ui_LayoutNotes(&a->doc, a->lines, NOTE_LINES_MAX);
}

static bool App_HasChangelog(App *a)
{
    if (a->sectionCount > 0)
        return true;
    App_Message(a, TR("更新内容を読み込めませんでした。\nインターネットに接続して、ソフトを起動し直してください。",
                      "Could not load the release notes.\nConnect to the Internet and restart the software."));
    return false;
}

// 上下・左右・L・R で更新内容を送る
static void App_ScrollNotes(App *a, u32 down)
{
    UiState *ui = &a->ui;
    int page = Ui_NotesPage(), max = Ui_NotesMaxScroll(ui);

    if (down & KEY_DOWN)
        ui->noteScroll = ui->noteScroll < max ? ui->noteScroll + 1 : max;
    if (down & KEY_UP)
        ui->noteScroll = ui->noteScroll > 0 ? ui->noteScroll - 1 : 0;
    if (down & (KEY_RIGHT | KEY_R))
        ui->noteScroll = ui->noteScroll + page < max ? ui->noteScroll + page : max;
    if (down & (KEY_LEFT | KEY_L))
        ui->noteScroll = ui->noteScroll > page ? ui->noteScroll - page : 0;
}

static bool App_InstallVersion(App *a, int index);

// 1 つの版の更新内容を読む。B で戻り、X でその版を入れる。アプリを起動し直すなら true
static bool App_NotesLoop(App *a, int index)
{
    UiView back = a->ui.view;
    bool quit = false;

    App_LoadSection(a, index);
    a->ui.view = UI_VIEW_NOTES;

    while (!quit && aptMainLoop())
    {
        int x, y, hit;

        hidScanInput();
        u32 down = hidKeysDown() | (hidKeysDownRepeat() & (KEY_UP | KEY_DOWN | KEY_LEFT | KEY_RIGHT));
        hit = App_Touched(&x, &y) ? Ui_HitButtons(&a->ui, x, y) : -1;
        if ((down & KEY_B) || hit == 0)
            break;
        if ((down & KEY_X) || hit == 2)
            quit = App_InstallVersion(a, index);
        App_ScrollNotes(a, down);
        App_Draw(a);
    }
    a->ui.view = back;
    return quit;
}

// 新しいバージョンがあればその版、無ければ 更新.md のいちばん上の版
static bool App_ShowNotes(App *a)
{
    const Release *latest = App_Latest(a);
    int index = 0;
    bool quit;

    if (!App_HasChangelog(a))
        return false;
    for (int i = 0; latest != NULL && i < a->sectionCount; ++i)
        if (App_SameTag(a->sections[i].tag, latest->tag))
            index = i;
    quit = App_NotesLoop(a, index);
    a->ui.view = UI_VIEW_MAIN;
    return quit;
}

static void App_SelectVersion(App *a, int index)
{
    UiState *ui = &a->ui;
    int rows = Ui_VersionRows();

    ui->versionSelected = index;
    if (index < ui->versionScroll)
        ui->versionScroll = index;
    else if (index >= ui->versionScroll + rows)
        ui->versionScroll = index - rows + 1;
    App_LoadSection(a, index);
}

// アプリを起動し直すなら true
static bool App_HistoryLoop(App *a)
{
    UiState *ui = &a->ui;
    const Release *latest = App_Latest(a);
    bool quit = false;

    if (!App_HasChangelog(a))
        return false;

    ui->versionCount = a->sectionCount;
    for (int i = 0; i < ui->versionCount; ++i)
    {
        const MdSection *s = &a->sections[i];
        bool isLatest = latest != NULL ? App_SameTag(s->tag, latest->tag) : i == 0;
        ui->versions[i] = (UiVersion){ s->tag, s->date, isLatest, App_SameTag(s->tag, APP_VERSION) };
    }
    ui->versionScroll = 0;
    ui->view = UI_VIEW_HISTORY;
    App_SelectVersion(a, 0);

    while (!quit && aptMainLoop())
    {
        int x, y, hit;

        hidScanInput();
        u32 down = hidKeysDown() | (hidKeysDownRepeat() & (KEY_UP | KEY_DOWN));

        if (App_Touched(&x, &y))
        {
            if ((hit = Ui_HitVersion(ui, x, y)) >= 0)
            {
                if (hit == ui->versionSelected)
                    quit = App_NotesLoop(a, hit);
                else
                    App_SelectVersion(a, hit);
            }
            else if ((hit = Ui_HitButtons(ui, x, y)) == 0)
                break;
            else if (hit == 1)
                quit = App_NotesLoop(a, ui->versionSelected);
            else if (hit == 2)
                quit = App_InstallVersion(a, ui->versionSelected);
        }
        if (down & KEY_B)
            break;
        if ((down & KEY_DOWN) && ui->versionSelected + 1 < ui->versionCount)
            App_SelectVersion(a, ui->versionSelected + 1);
        if ((down & KEY_UP) && ui->versionSelected > 0)
            App_SelectVersion(a, ui->versionSelected - 1);
        if (down & KEY_A)
            quit = App_NotesLoop(a, ui->versionSelected);
        else if (down & KEY_X)
            quit = App_InstallVersion(a, ui->versionSelected);
        App_ScrollNotes(a, down & (KEY_L | KEY_R | KEY_LEFT | KEY_RIGHT));

        App_Draw(a);
    }
    ui->view = UI_VIEW_MAIN;
    return quit;
}

// ---------------------------------------------------------------- ソースコード

static void App_SourceLoop(App *a)
{
    a->ui.view = UI_VIEW_SOURCE;
    while (aptMainLoop())
    {
        int x, y;

        hidScanInput();
        if ((hidKeysDown() & KEY_B) || (App_Touched(&x, &y) && Ui_HitButtons(&a->ui, x, y) == 0))
            break;
        App_Draw(a);
    }
    a->ui.view = UI_VIEW_MAIN;
}

static void App_InitSource(App *a)
{
    static uint8_t temp[UI_QR_MAX];

    a->ui.upstreamUrl = UPSTREAM_URL;
    if (UPDATE_REPO[0] == 0)
        return;
    a->ui.sourceUrl = SOURCE_URL;
    if (qrcodegen_encodeText(SOURCE_URL, temp, a->qr, qrcodegen_Ecc_MEDIUM, qrcodegen_VERSION_MIN, 10,
                             qrcodegen_Mask_AUTO, true))
        a->ui.qr = a->qr;
}

// ---------------------------------------------------------------- 入れる・更新する

static void App_InstallErrorText(char *out, size_t size, InstallResult res, int detail, int detail2)
{
    switch (res)
    {
        case INSTALL_ERR_NET:
            App_NetErrorText(out, size, (HttpsResult)detail, detail2);
            break;
        case INSTALL_ERR_FORMAT:
            snprintf(out, size, "%s\n%s", TR("ダウンロードしたデータが壊れていました。", "The downloaded data was corrupted."),
                     MSG_RETRY);
            break;
        case INSTALL_ERR_WRITE:
            snprintf(out, size, "%s", TR("SDカードに書き込めませんでした。\n空き容量を確認してください。",
                                         "Could not write to the SD card.\nCheck the free space."));
            break;
        case INSTALL_ERR_CIA:
            snprintf(out, size, "%s\n%s", TR("ソフトを更新できませんでした。", "Could not update the software."), MSG_RETRY);
            break;
        default:
            snprintf(out, size, "%s", MSG_RETRY);
            break;
    }
}

// 途中で HOME メニューに戻ったりスリープしたりしないようにする
static void App_LockSystem(bool lock)
{
    aptSetHomeAllowed(!lock);
    aptSetSleepAllowed(!lock);
}

// このアプリに入れた boot.firm を SD カードに入れる。アプリを入れ直した続きでは違うものだけを入れ、
// メニューから選んだとき（all）はすべて入れ直す
static void App_InstallBundled(App *a, bool all)
{
    // SD:/boot.firm は最後に差し替える（途中で失敗しても起動できる版を残すため）
    static const Part order[PART_COUNT] = { PART_FIRM_NEW, PART_FIRM_91, PART_ROOT_FIRM };
    static const char *const backupNames[PART_COUNT] = { "boot.firm", "new_boot.firm", "9.1_boot.firm" };
    InstallResult res = INSTALL_OK;
    char text[320], label[40];
    int steps = 0, step = 0;

    if (!all && !Local_NeedsInstall(&a->local))
        return;

    for (int p = 0; p < PART_COUNT; ++p)
        steps += all || !a->local.same[p];

    App_LockSystem(true);
    a->ui.cancellable = false;
    a->ui.progressDone = 0;
    a->ui.progressTotal = (uint64_t)steps;
    App_SetStatus(a, UI_STATUS_BUSY, TR("boot.firm_Customize-Model を入れています", "Installing boot.firm_Customize-Model"),
                  TR("電源を切らないでください。", "Do not turn off the power."));
    App_Refresh(a);
    App_SetOverlay(a, UI_OVERLAY_PROGRESS, MSG_INSTALLING);

    for (int i = 0; i < PART_COUNT && res == INSTALL_OK; ++i)
    {
        Part p = order[i];
        if (!all && a->local.same[p])
            continue;
        snprintf(a->ui.progressLabel, sizeof(a->ui.progressLabel), TR("入れています %d/%d", "Installing %d/%d"), ++step, steps);
        App_Draw(a);
        res = Install_ReplaceFile(Part_Bundle(p, &a->local), Part_Path(p), backupNames[p]);
        a->ui.progressDone = (uint64_t)step;
    }
    if (res == INSTALL_OK)
        Local_RemoveLegacy();

    a->ui.overlay = UI_OVERLAY_NONE;
    App_LockSystem(false);
    Local_Read(&a->local);
    App_VersionLabel(label, sizeof(label), APP_VERSION);

    if (res != INSTALL_OK)
    {
        char reason[200];
        App_InstallErrorText(reason, sizeof(reason), res, 0, 0);
        snprintf(text, sizeof(text), "%s\n%s", TR("入れられませんでした。", "Could not install."), reason);
        App_SetStatus(a, UI_STATUS_ERROR, TR("入れられませんでした", "Could not install"), reason);
        App_Refresh(a);
        App_Message(a, text);
        return;
    }

    snprintf(text, sizeof(text), TR("本体を再起動すると、boot.firm_Customize-Model %s になります。",
                                    "boot.firm_Customize-Model %s will be used after a restart."), label);
    App_SetStatus(a, UI_STATUS_DONE, TR("入れ終わりました", "Installed"), text);
    App_Refresh(a);
    snprintf(text, sizeof(text), TR("boot.firm_Customize-Model %s を\n入れました。\n本体を再起動すると使えるようになります。\n今すぐ再起動しますか？",
                                    "boot.firm_Customize-Model %s has been installed.\nRestart the system to use it.\nRestart now?"), label);
    if (App_Confirm(a, text, TR("再起動", "Restart"), TR("あとで", "Later")))
        APT_HardwareResetAsync();
}

static void App_Reinstall(App *a)
{
    char text[320], label[40];

    App_VersionLabel(label, sizeof(label), APP_VERSION);
    snprintf(text, sizeof(text), TR("このソフトに入っている\nboot.firm_Customize-Model %s\n（v" LUMA_NEW_VERSION "・v9.1）を、"
                                    "SD カードに%s。\nよろしいですか？",
                                    "boot.firm_Customize-Model %s (v" LUMA_NEW_VERSION " and v9.1) included in this software "
                                    "will be %s to the SD card.\nContinue?"), label,
             App_NeedsInstall(a) ? TR("入れます", "installed") : TR("入れ直します", "reinstalled"));
    if (App_IsOutdated(a))
        snprintf(text + strlen(text), sizeof(text) - strlen(text), "%s",
                 TR("\n（SD カードの方が新しいバージョンです）", "\n(The SD card has a newer version.)"));
    if (App_Confirm(a, text, TR("入れる", "Install"), TR("やめる", "Cancel")))
        App_InstallBundled(a, true);
}

// r の版のアプリをダウンロードして入れ直す（新しい版への更新も、前の版に戻すのも同じ）。
// 入れ直したら、そのアプリを起動し直すので true
static bool App_InstallRelease(App *a, const Release *r)
{
    const ReleaseAsset *asset = r != NULL ? Release_FindAsset(r, ASSET_CIA) : NULL;
    char text[320], label[40], ciaPath[160];
    int detail = 0, detail2 = 0;
    InstallResult res;

    // v1.3 までは、Releases に更新アプリが無い
    if (asset == NULL)
    {
        App_Message(a, TR("このバージョンは、このソフトからは入れられません。",
                          "This version cannot be installed from this software."));
        return false;
    }

    bool older = Version_Compare(r->tag, APP_VERSION) < 0;
    App_VersionLabel(label, sizeof(label), r->tag);
    snprintf(text, sizeof(text), older ? TR("boot.firm_Customize-Model を\n%s に戻します。\nよろしいですか？",
                                            "boot.firm_Customize-Model will be downgraded to %s.\nContinue?") :
                                         TR("boot.firm_Customize-Model を\n%s に更新します。\nよろしいですか？",
                                            "boot.firm_Customize-Model will be updated to %s.\nContinue?"), label);
    if (!App_Confirm(a, text, older ? TR("戻す", "Downgrade") : TR("更新する", "Update"), TR("やめる", "Cancel")))
        return false;

    snprintf(ciaPath, sizeof(ciaPath), UPDATE_TMP_DIR "/%s", ASSET_CIA);
    App_LockSystem(true);

    InstallProgress progress = { App_ProgressUpdate, App_ProgressCancelled, a };
    a->ui.cancellable = true;
    a->ui.progressDone = a->ui.progressTotal = 0;
    snprintf(a->ui.progressLabel, sizeof(a->ui.progressLabel), "%s", TR("ダウンロード中", "Downloading"));
    App_SetOverlay(a, UI_OVERLAY_PROGRESS, MSG_UPDATING);
    res = Install_Download(asset, ciaPath, &progress, &detail, &detail2);
    a->ui.cancellable = false;

    if (res == INSTALL_OK)
    {
        progress.cancelled = NULL;
        a->ui.progressDone = a->ui.progressTotal = 0;
        snprintf(a->ui.progressLabel, sizeof(a->ui.progressLabel), "%s", TR("ソフトを入れ替え中", "Replacing the software"));
        App_SetOverlay(a, UI_OVERLAY_PROGRESS, MSG_UPDATING);
        res = Install_Cia(ciaPath, &progress, &detail);
    }

    remove(ciaPath);
    a->ui.overlay = UI_OVERLAY_NONE;
    App_LockSystem(false);

    if (res != INSTALL_OK)
    {
        if (res == INSTALL_ERR_CANCELLED)
            App_Message(a, TR("中止しました。", "Cancelled."));
        else
        {
            char reason[200];
            App_InstallErrorText(reason, sizeof(reason), res, detail, detail2);
            snprintf(text, sizeof(text), "%s\n%s", TR("入れられませんでした。", "Could not install."), reason);
            App_Message(a, text);
        }
        // 更新内容を出している間は取り直さない（出している文が 更新.md の中を指している）
        if (a->ui.view == UI_VIEW_MAIN)
            App_Fetch(a);
        return false;
    }

    // 入れ替えたアプリが、中に入っている boot.firm を入れる（しるしを見て、ボタンを待たずに続ける）
    Local_SetPending(r->tag);
    App_Message(a, TR("ソフトを入れ替えました。\n入れ替えたソフトを起動して、\nboot.firm_Customize-Model を入れます。",
                      "The software has been replaced.\nIt will now start and install boot.firm_Customize-Model."));
    u64 programId = 0;
    if (R_SUCCEEDED(APT_GetProgramID(&programId)))
        aptSetChainloader(programId, MEDIATYPE_SD);
    return true;
}

// 更新.md の index 番目の版を入れる。このアプリと同じ版なら、中に入っている boot.firm を入れ直す
static bool App_InstallVersion(App *a, int index)
{
    const char *tag = a->sections[index].tag;

    if (App_SameTag(tag, APP_VERSION))
    {
        App_Reinstall(a);
        return false;
    }
    for (int i = 0; a->fetched && i < a->releases.count; ++i)
        if (App_SameTag(a->releases.items[i].tag, tag))
            return App_InstallRelease(a, &a->releases.items[i]);
    return App_InstallRelease(a, NULL);
}

// 画面の色を切り替えて、SD カードに覚える（書けなくても、この回は切り替えた色で出す）
static void App_ToggleTheme(App *a)
{
    a->ui.light = !a->ui.light;
    Local_SetLightTheme(a->ui.light);
}

// ---------------------------------------------------------------- 本体

// 行を選んだときの動き。アプリを終えるなら true
static bool App_Activate(App *a, int index)
{
    switch (a->ui.items[index].id)
    {
        case ITEM_INSTALL:
            if (App_HasUpdate(a))
                return App_InstallRelease(a, App_Latest(a));
            App_Reinstall(a);
            break;
        case ITEM_NOTES:
            return App_ShowNotes(a);
        case ITEM_HISTORY:
            return App_HistoryLoop(a);
        case ITEM_SOURCE:
            App_SourceLoop(a);
            break;
        case ITEM_AUTO:
            if (!Local_SetAutoCheck(!a->local.autoCheck))
                App_Message(a, TR("SDカードに書き込めませんでした。", "Could not write to the SD card."));
            Local_Read(&a->local);
            App_Refresh(a);
            break;
        default:
            break;
    }
    return false;
}

int main(void)
{
    App *a = &g_app;
    bool quit = false;
    char pending[RELEASE_TAG_SIZE];

    romfsInit();
    Screen_Init();
    memset(a, 0, sizeof(*a));
    a->releases.latest = -1;
    a->ui.appVersion = APP_VERSION;
    a->ui.notesEmpty = TR("（更新内容はありません）", "(No release notes)");
    App_InitSource(a);
    Local_Read(&a->local);
    a->ui.light = a->local.lightTheme;

    // 起動しただけでは入れない。このアプリに入れ直した続きのときだけ、ボタンを待たずに入れる
    Local_TakePending(pending, sizeof(pending));
    if (App_SameTag(pending, APP_VERSION))
        App_InstallBundled(a, false);

    if (Https_Init())
        App_Fetch(a);
    else
    {
        App_SetStatus(a, UI_STATUS_ERROR, TR("確認できませんでした", "Could not check"),
                      TR("メモリが不足しています。\n本体を再起動してください。", "Not enough memory.\nPlease restart the system."));
        App_Refresh(a);
    }

    while (!quit && aptMainLoop())
    {
        int x, y, hit;

        hidScanInput();
        u32 down = hidKeysDown() | (hidKeysDownRepeat() & (KEY_UP | KEY_DOWN));

        if (down & (KEY_START | KEY_B))
            break;
        if (down & KEY_DOWN)
            a->ui.selected = (a->ui.selected + 1) % a->ui.itemCount;
        if (down & KEY_UP)
            a->ui.selected = (a->ui.selected + a->ui.itemCount - 1) % a->ui.itemCount;
        if (down & KEY_Y)
            App_ToggleTheme(a);

        if (App_Touched(&x, &y))
        {
            if ((hit = Ui_HitItem(&a->ui, x, y)) >= 0)
            {
                a->ui.selected = hit;
                quit = App_Activate(a, hit);
            }
            else if ((hit = Ui_HitButtons(&a->ui, x, y)) == 0)
                break;
            else if (hit == 1)
                quit = App_Activate(a, a->ui.selected);
            else if (hit == 2)
                App_ToggleTheme(a);
        }
        else if (down & KEY_A)
            quit = App_Activate(a, a->ui.selected);

        App_Draw(a);
    }

    Md_Free(&a->doc);
    free(a->changelog);
    Https_Exit();
    Screen_Exit();
    romfsExit();
    return 0;
}

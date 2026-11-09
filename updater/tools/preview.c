// 更新アプリの画面の見本を PC で作る（3DS の上の画面と下の画面を縦に並べた PPM）。
//
//   cc -std=gnu11 -DUI_HOST -Isource -o preview tools/preview.c source/ui.c source/screen.c source/text.c
//      source/md.c source/lang.c source/qrcodegen/qrcodegen.c -lm（1 行で）
//   ./preview data/ui_font.bin 出力のフォルダ ../更新.md [en]
//
// 4 つ目に en を付けると英語の画面にする

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ui.h"
#include "screen.h"
#include "md.h"
#include "lang.h"
#include "qrcodegen/qrcodegen.h"

static UiNoteLine g_lines[1024];

static void Preview_Items(UiState *ui, bool update)
{
    int n = 0;
    if (update)
        ui->items[n++] = (UiItem){ 0, TR("更新する", "Update"), NULL,
                                   TR("Luma3DS_New を新しいバージョンに更新します。", "Updates Luma3DS_New to the new version."), true };
    ui->items[n++] = (UiItem){ 1, TR("更新内容", "What's new"), NULL,
                               TR("新しいバージョンで変わったところを見ます。", "Shows what changed in the newest version."), false };
    ui->items[n++] = (UiItem){ 2, TR("更新履歴", "Version history"), NULL,
                               TR("これまでのバージョンの変更点を見ます。", "Shows the changes in earlier versions."), false };
    ui->items[n++] = (UiItem){ 3, TR("インストール", "Install"), NULL,
                               TR("このソフトに入っている Luma3DS_New を、SD カードに入れ直します。",
                                  "Reinstalls the Luma3DS_New included in this software to the SD card."), false };
    ui->items[n++] = (UiItem){ 4, TR("ソースコード", "Source code"), NULL,
                               TR("ソースコードの場所を、URL と QR コードで出します。",
                                  "Shows where to get the source code, as a URL and a QR code."), false };
    ui->items[n++] = (UiItem){ 5, TR("更新のお知らせ", "Update notifications"), "ON",
                               TR("ON にすると、本体を起動したときに新しいバージョンを確認し、あればお知らせします。",
                                  "When ON, checks for a new version at startup and lets you know if there is one."), false };
    ui->itemCount = n;
}

static void Preview_Save(const UiState *ui, const char *dir, const char *name)
{
    char path[512];
    Ui_Draw(ui);
    snprintf(path, sizeof(path), "%s/%s.ppm", dir, name);
    Screen_SavePpm(path);
}

static char *Preview_ReadFile(const char *path)
{
    FILE *f = fopen(path, "rb");
    char *md;
    size_t n;

    if (f == NULL)
        return NULL;
    md = calloc(1, 1 << 20);
    n = fread(md, 1, (1 << 20) - 1, f);
    md[n] = 0;
    fclose(f);
    return md;
}

int main(int argc, char **argv)
{
    static UiState ui;
    static MdSection sections[UI_VERSION_MAX];
    static uint8_t qr[UI_QR_MAX], temp[UI_QR_MAX];
    static MdDoc doc;
    char *md;
    int count;

    if (argc < 4 || !Screen_LoadFont(argv[1]) || (md = Preview_ReadFile(argv[3])) == NULL)
    {
        fprintf(stderr, "usage: preview ui_font.bin outdir 更新.md [en]\n");
        return 1;
    }
    g_langEn = argc > 4 && strcmp(argv[4], "en") == 0;
    count = Md_Sections(md, sections, UI_VERSION_MAX);

    memset(&ui, 0, sizeof(ui));
    ui.appVersion = "v1.4";
    ui.notesEmpty = TR("（更新内容はありません）", "(No release notes)");

    // 確かめている
    ui.status = UI_STATUS_BUSY;
    strcpy(ui.statusText, TR("確認しています", "Checking"));
    strcpy(ui.current, "v1.4");
    strcpy(ui.latest, "-");
    Preview_Items(&ui, false);
    ui.frame = 40;
    Preview_Save(&ui, argv[2], "1_checking");

    // 新しいバージョンがある
    ui.status = UI_STATUS_UPDATE;
    strcpy(ui.statusText, TR("新しいバージョンがあります", "A new version is available"));
    strcpy(ui.statusDetail, TR("「更新する」を選ぶと、v1.5 に更新します。", "Select \"Update\" to update to v1.5."));
    strcpy(ui.latest, "v1.5");
    Preview_Items(&ui, true);
    ui.selected = 0;
    Preview_Save(&ui, argv[2], "2_update");

    // 最新
    ui.status = UI_STATUS_LATEST;
    strcpy(ui.statusText, TR("最新の状態です", "Up to date"));
    ui.statusDetail[0] = 0;
    strcpy(ui.latest, "v1.4");
    Preview_Items(&ui, false);
    ui.selected = 4;
    Preview_Save(&ui, argv[2], "3_latest");

    // 失敗
    ui.status = UI_STATUS_ERROR;
    strcpy(ui.statusText, TR("確認できませんでした", "Could not check"));
    strcpy(ui.statusDetail, TR("インターネットに接続できませんでした。\nWi-Fi の設定を確認してください。",
                               "Could not connect to the Internet.\nCheck your Wi-Fi settings."));
    strcpy(ui.latest, "-");
    ui.selected = 1;
    Preview_Save(&ui, argv[2], "4_error");

    // 更新内容（いちばん上の版）
    if (count > 0)
    {
        Md_Parse(sections[0].body, sections[0].len, &doc);
        ui.notes = g_lines;
        ui.noteCount = Ui_LayoutNotes(&doc, g_lines, 1024);
        snprintf(ui.notesTitle, sizeof(ui.notesTitle), TR("%s の更新内容", "What's new in %s"), sections[0].tag);
        ui.view = UI_VIEW_NOTES;
        Preview_Save(&ui, argv[2], "5_notes");
        ui.noteScroll = Ui_NotesPage();
        Preview_Save(&ui, argv[2], "5_notes_2");
        ui.noteScroll = 0;

        // 更新履歴
        ui.view = UI_VIEW_HISTORY;
        ui.versionCount = count;
        for (int i = 0; i < count; ++i)
            ui.versions[i] = (UiVersion){ sections[i].tag, sections[i].date, i == 0, i == 0 };
        ui.versionSelected = 1;
        Md_Free(&doc);
        Md_Parse(sections[1].body, sections[1].len, &doc);
        ui.noteCount = Ui_LayoutNotes(&doc, g_lines, 1024);
        snprintf(ui.notesTitle, sizeof(ui.notesTitle), TR("%s の更新内容", "What's new in %s"), sections[1].tag);
        Preview_Save(&ui, argv[2], "6_history");
        ui.view = UI_VIEW_MAIN;
    }

    // ソースコード
    ui.view = UI_VIEW_SOURCE;
    ui.sourceUrl = "https://github.com/Elves-263/boot.firm_Customize-Model";
    ui.upstreamUrl = "https://github.com/LumaTeam/Luma3DS";
    if (qrcodegen_encodeText(ui.sourceUrl, temp, qr, qrcodegen_Ecc_MEDIUM, qrcodegen_VERSION_MIN, 10, qrcodegen_Mask_AUTO, true))
        ui.qr = qr;
    Preview_Save(&ui, argv[2], "7_source");
    ui.view = UI_VIEW_MAIN;

    // 確かめる
    ui.status = UI_STATUS_UPDATE;
    strcpy(ui.statusText, TR("新しいバージョンがあります", "A new version is available"));
    strcpy(ui.statusDetail, TR("「更新する」を選ぶと、v1.5 に更新します。", "Select \"Update\" to update to v1.5."));
    strcpy(ui.latest, "v1.5");
    Preview_Items(&ui, true);
    ui.selected = 0;
    ui.overlay = UI_OVERLAY_CONFIRM;
    strcpy(ui.overlayText, TR("Luma3DS_New を v1.5 に更新します。\nよろしいですか？", "Luma3DS_New will be updated to v1.5.\nContinue?"));
    ui.yesLabel = TR("更新する", "Update");
    ui.noLabel = TR("やめる", "Cancel");
    Preview_Save(&ui, argv[2], "8_confirm");

    // 進み具合
    ui.overlay = UI_OVERLAY_PROGRESS;
    strcpy(ui.overlayText, TR("更新しています。\n電源を切らないでください。", "Updating.\nDo not turn off the power."));
    strcpy(ui.progressLabel, TR("ダウンロード中", "Downloading"));
    ui.progressDone = 180000;
    ui.progressTotal = 400000;
    ui.cancellable = true;
    Preview_Save(&ui, argv[2], "9_progress");

    // 入れ終わった
    ui.status = UI_STATUS_DONE;
    strcpy(ui.statusText, TR("入れ終わりました", "Installed"));
    strcpy(ui.statusDetail, TR("本体を再起動すると、Luma3DS_New v1.4 になります。", "Luma3DS_New v1.4 will be used after a restart."));
    strcpy(ui.latest, "v1.4");
    Preview_Items(&ui, false);
    ui.overlay = UI_OVERLAY_CONFIRM;
    strcpy(ui.overlayText, TR("Luma3DS_New v1.4 を入れました。\n本体を再起動すると使えるようになります。\n今すぐ再起動しますか？",
                              "Luma3DS_New v1.4 has been installed.\nRestart the system to use it.\nRestart now?"));
    ui.yesLabel = TR("再起動", "Restart");
    ui.noLabel = TR("あとで", "Later");
    Preview_Save(&ui, argv[2], "10_installed");

    Md_Free(&doc);
    free(md);
    return 0;
}

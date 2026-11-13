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

// Rosalina と同じく、地の色に文字だけを並べる。見出しは金色、選んでいる行には > を付け、
// 下の端にボタンの説明（A: 決定 など）を出す。枠・影・角の丸い四角は使わない

#include <stdio.h>
#include <string.h>
#include "ui.h"
#include "screen.h"
#include "lang.h"
#include "qrcodegen/qrcodegen.h"

// ---------------------------------------------------------------- 色（黒×金と白×金。UiState の light で選ぶ）

typedef struct UiColors
{
    uint32_t bg, text, dim;
    uint32_t title;     // 題・見出し・選んでいる行の印・進み具合の帯
    uint32_t good;      // 目立たせる行、ON、「最新」、うまくいった知らせ
    uint32_t bad;
    uint32_t mark;      // 太字、「使用中」、進んでいる途中の知らせ
    uint32_t h3, code, codeBg, rule;
} UiColors;

static const UiColors g_colors[2] =
{
    { 0x000000, 0xF4F1E8, 0x968F7D, 0xD6B23E, 0xF4D678, 0xEB7864, 0xF4D678, 0xC8AC5A, 0xE8D9A8, 0x2A261C, 0x5C4D1E },
    { 0xFAF7F0, 0x1E1B15, 0x7C7463, 0x967012, 0x7A5800, 0xBE3C28, 0x8A6A10, 0xA07E1E, 0x5A4A14, 0xF0E8D2, 0xD6BE78 },
};
static const UiColors *g_c = &g_colors[0];

#define C_BG        (g_c->bg)
#define C_TEXT      (g_c->text)
#define C_DIM       (g_c->dim)
#define C_TITLE     (g_c->title)
#define C_GOOD      (g_c->good)
#define C_BAD       (g_c->bad)
#define C_YELLOW    (g_c->mark)
#define C_H3        (g_c->h3)
#define C_CODE      (g_c->code)
#define C_CODE_BG   (g_c->codeBg)
#define C_RULE      (g_c->rule)

// ---------------------------------------------------------------- 並び（タッチの判定と共通）

#define MARGIN      10
#define LINE_H      19          // 文の行の間隔
#define TITLE_Y     6
#define BODY_Y      32          // 上の画面の、題の下
#define ITEM_Y      10          // 下の画面の最初の行
#define ITEM_H      22
#define CURSOR_X    MARGIN
#define LABEL_X     (MARGIN + 16)
#define HELP_Y      (SCREEN_HEIGHT - 6 - LINE_H)    // 下の端のボタンの説明
#define BUTTON_GAP  24

#define NOTES_X     MARGIN
#define NOTES_W     (SCREEN_TOP_WIDTH - 2 * MARGIN)
#define NOTES_ROWS  ((SCREEN_HEIGHT - BODY_Y - 4) / LINE_H)

static int Ui_Text(ScreenId s, int x, int y, uint32_t color, const char *text)
{
    return Screen_DrawText(s, x, y, color, text, (size_t)-1);
}

static int Ui_Width(const char *text)
{
    return Screen_TextWidth(text, (size_t)-1);
}

// 右寄せ
static void Ui_TextRight(ScreenId s, int right, int y, uint32_t color, const char *text)
{
    Ui_Text(s, right - Ui_Width(text), y, color, text);
}

// 幅 width で折り返して描き、次の行の y を返す（maxY を超える行は描かない）
static int Ui_Wrapped(ScreenId s, int x, int y, int width, int maxY, uint32_t color, const char *text)
{
    size_t len;
    const char *next;

    while ((next = Screen_WrapLine(text, width, &len)) != NULL && y + LINE_H <= maxY)
    {
        Screen_DrawText(s, x, y, color, text, len);
        y += LINE_H;
        text = next;
    }
    return y;
}

// 上の画面の題。右に小さく版を出す
static void Ui_TopTitle(const char *title, const char *right)
{
    Ui_Text(SCREEN_TOP, MARGIN, TITLE_Y, C_TITLE, title);
    if (right != NULL)
        Ui_TextRight(SCREEN_TOP, SCREEN_TOP_WIDTH - MARGIN, TITLE_Y, C_DIM, right);
}

// 下の端のボタンの説明は 3 つまで。順に「A: …」「B: …」と、「X: …」か「Y: …」（無ければ NULL）
#define BUTTON_MAX  3

// 説明の左端と幅。タッチの判定にも使う
static void Ui_ButtonRects(const char *const labels[BUTTON_MAX], int x[BUTTON_MAX], int w[BUTTON_MAX])
{
    int next = MARGIN;

    for (int i = 0; i < BUTTON_MAX; ++i)
    {
        x[i] = next;
        w[i] = labels[i] != NULL ? Ui_Width(labels[i]) : 0;
        if (w[i] > 0)
            next += w[i] + BUTTON_GAP;
    }
}

// いまの画面のボタンの説明。a と b は言葉を組み立てる場所
static void Ui_ButtonLabels(const UiState *ui, char a[48], char b[48], const char *labels[BUTTON_MAX])
{
    labels[0] = labels[1] = labels[2] = NULL;

    switch (ui->overlay)
    {
        case UI_OVERLAY_CONFIRM:
            snprintf(a, 48, "A: %s", ui->yesLabel);
            snprintf(b, 48, "B: %s", ui->noLabel);
            labels[0] = a;
            labels[1] = b;
            break;
        case UI_OVERLAY_MESSAGE:
            snprintf(a, 48, "A: %s", ui->yesLabel);
            labels[0] = a;
            break;
        case UI_OVERLAY_PROGRESS:
            labels[1] = ui->cancellable ? TR("B: 中止", "B: Cancel") : NULL;
            break;
        default:
            if (ui->view == UI_VIEW_MAIN)
            {
                labels[0] = TR("A: 決定", "A: Select");
                labels[1] = TR("B: 終了", "B: Exit");
                labels[2] = TR("Y: 色の切り替え", "Y: Colors");
            }
            else
            {
                if (ui->view == UI_VIEW_HISTORY)
                    labels[0] = TR("A: 読む", "A: Read");
                labels[1] = TR("B: 戻る", "B: Back");
                if (ui->view != UI_VIEW_SOURCE)
                    labels[2] = TR("X: この版を入れる", "X: Install this");
            }
            break;
    }
}

static void Ui_DrawButtons(const UiState *ui)
{
    char a[48], b[48];
    const char *labels[BUTTON_MAX];
    int x[BUTTON_MAX], w[BUTTON_MAX];

    Ui_ButtonLabels(ui, a, b, labels);
    Ui_ButtonRects(labels, x, w);
    for (int i = 0; i < BUTTON_MAX; ++i)
        if (labels[i] != NULL)
            Ui_Text(SCREEN_BOTTOM, x[i], HELP_Y, C_TEXT, labels[i]);
}

// ---------------------------------------------------------------- 更新内容

static uint8_t Ui_StyleBit(char c)
{
    return c == MD_BOLD ? UI_STYLE_BOLD : c == MD_CODE ? UI_STYLE_CODE : c == MD_LINK ? UI_STYLE_LINK : 0;
}

static uint32_t Ui_NoteColor(uint8_t kind)
{
    switch (kind)
    {
        case MD_H1:
        case MD_H2:         return C_TITLE;
        case MD_H3:         return C_H3;
        case MD_QUOTE:      return C_DIM;
        case MD_CODEBLOCK:  return C_CODE;
        default:            return C_TEXT;
    }
}

// 飾りの字（MD_BOLD など）で区切りながら 1 行を描く
static void Ui_DrawRich(int x, int y, uint32_t base, const char *text, size_t len, uint8_t style)
{
    const char *end = text + len;

    while (text < end)
    {
        const char *seg = text;
        while (text < end && Ui_StyleBit(*text) == 0)
            ++text;

        size_t n = (size_t)(text - seg);
        int w = Screen_TextWidth(seg, n);
        uint32_t color = base;
        if (style & UI_STYLE_CODE)
        {
            Screen_FillRect(SCREEN_TOP, x - 1, y + 1, w + 2, LINE_H - 1, C_CODE_BG, 255);
            color = C_CODE;
        }
        else if (style & UI_STYLE_LINK)
            color = C_TITLE;
        else if (style & UI_STYLE_BOLD)
            color = C_YELLOW;

        Screen_DrawText(SCREEN_TOP, x, y, color, seg, n);
        if ((style & UI_STYLE_LINK) && n > 0)
            Screen_FillRect(SCREEN_TOP, x, y + LINE_H - 2, w, 1, C_TITLE, 255);
        x += w;

        if (text < end)
            style ^= Ui_StyleBit(*text++);
    }
}

int Ui_LayoutNotes(const MdDoc *doc, UiNoteLine *lines, int max)
{
    int count = 0;

    for (int b = 0; b < doc->count && count < max; ++b)
    {
        const MdBlock *blk = &doc->blocks[b];
        int x = 0;

        switch (blk->kind)
        {
            case MD_BULLET:
            case MD_NUMBER:
                x = blk->depth * 16 + Screen_TextWidth(blk->marker, (size_t)-1) + 4;
                break;
            case MD_QUOTE:
                x = 10 + blk->depth * 10;
                break;
            case MD_CODEBLOCK:
                x = 8;
                break;
            default:
                break;
        }

        if (blk->kind == MD_RULE || blk->kind == MD_BLANK || blk->text[0] == 0)
        {
            lines[count++] = (UiNoteLine){ "", 0, (uint8_t)blk->kind, 0, (int16_t)x, NULL };
            continue;
        }

        const char *p = blk->text, *next;
        uint8_t style = blk->kind == MD_CODEBLOCK ? UI_STYLE_CODE : 0;
        bool first = true;
        size_t len;

        while (count < max && (next = Screen_WrapLine(p, NOTES_W - x, &len)) != NULL)
        {
            bool marked = first && (blk->kind == MD_BULLET || blk->kind == MD_NUMBER);
            lines[count++] = (UiNoteLine){ p, len, (uint8_t)blk->kind, style, (int16_t)x, marked ? blk->marker : NULL };
            for (size_t i = 0; i < len; ++i)
                style ^= Ui_StyleBit(p[i]);
            first = false;
            p = next;
        }
    }
    return count;
}

int Ui_NotesPage(void)
{
    return NOTES_ROWS;
}

int Ui_NotesMaxScroll(const UiState *ui)
{
    return ui->noteCount > NOTES_ROWS ? ui->noteCount - NOTES_ROWS : 0;
}

static void Ui_DrawNoteLine(const UiNoteLine *l, int y)
{
    int x = NOTES_X + l->x;

    switch (l->kind)
    {
        case MD_RULE:
            Screen_FillRect(SCREEN_TOP, NOTES_X, y + LINE_H / 2, NOTES_W, 1, C_RULE, 255);
            return;
        case MD_QUOTE:
            Screen_FillRect(SCREEN_TOP, x - 8, y + 2, 2, LINE_H - 2, C_RULE, 255);
            break;
        case MD_BULLET:
        case MD_NUMBER:
            if (l->marker != NULL)
                Screen_DrawText(SCREEN_TOP, x - Screen_TextWidth(l->marker, (size_t)-1) - 4, y,
                                C_DIM, l->marker, (size_t)-1);
            break;
        default:
            break;
    }

    Ui_DrawRich(x, y, Ui_NoteColor(l->kind), l->text, l->len, l->style);
    if (l->kind == MD_H1 && l->len > 0)
        Screen_FillRect(SCREEN_TOP, NOTES_X, y + LINE_H - 1, NOTES_W, 1, C_TITLE, 255);
}

// 上の画面に更新内容を描く（題と、行の場所）
static void Ui_DrawNotes(const UiState *ui)
{
    char pos[40];

    pos[0] = 0;
    if (ui->noteCount > NOTES_ROWS)
    {
        int last = ui->noteScroll + NOTES_ROWS;
        snprintf(pos, sizeof(pos), "%d-%d / %d", ui->noteScroll + 1, last < ui->noteCount ? last : ui->noteCount,
                 ui->noteCount);
    }
    Ui_TopTitle(ui->notesTitle, pos[0] != 0 ? pos : NULL);

    if (ui->noteCount == 0)
    {
        Ui_Text(SCREEN_TOP, NOTES_X, BODY_Y, C_DIM, ui->notesEmpty != NULL ? ui->notesEmpty : "");
        return;
    }

    for (int i = 0; i < NOTES_ROWS && ui->noteScroll + i < ui->noteCount; ++i)
        Ui_DrawNoteLine(&ui->notes[ui->noteScroll + i], BODY_Y + i * LINE_H);
}

// ---------------------------------------------------------------- 最初の画面

static uint32_t Ui_StatusColor(UiStatus s)
{
    switch (s)
    {
        case UI_STATUS_UPDATE:  return C_GOOD;
        case UI_STATUS_DONE:    return C_GOOD;
        case UI_STATUS_ERROR:   return C_BAD;
        case UI_STATUS_BUSY:    return C_YELLOW;
        default:                return C_TEXT;
    }
}

static void Ui_DrawMainTop(const UiState *ui)
{
    const char *labels[2] = { TR("現在のバージョン", "Current version"), TR("最新のバージョン", "Latest version") };
    const char *values[2] = { ui->current, ui->latest };
    int valueX = MARGIN + Ui_Width(labels[0]) > MARGIN + Ui_Width(labels[1]) ?
                 MARGIN + Ui_Width(labels[0]) + 20 : MARGIN + Ui_Width(labels[1]) + 20;
    int y = BODY_Y;
    char status[80];

    Ui_TopTitle(TR("boot.firm_Customize-Model 更新", "boot.firm_Customize-Model Updater"), ui->appVersion);

    for (int i = 0; i < 2; ++i, y += LINE_H)
    {
        Ui_Text(SCREEN_TOP, MARGIN, y, C_DIM, labels[i]);
        Ui_Text(SCREEN_TOP, valueX, y, C_TEXT, values[i]);
    }

    // 確かめている間は、文の後ろの点を動かす
    snprintf(status, sizeof(status), "%s%s", ui->statusText,
             ui->status == UI_STATUS_BUSY ? &"..."[2 - (ui->frame / 20) % 3] : "");
    y += LINE_H / 2;
    Ui_Text(SCREEN_TOP, MARGIN, y, Ui_StatusColor(ui->status), status);
    y = Ui_Wrapped(SCREEN_TOP, MARGIN, y + LINE_H, NOTES_W, SCREEN_HEIGHT - 3 * LINE_H, C_TEXT, ui->statusDetail);

    // 選んでいる行の説明（下の 2 行）
    if (ui->selected >= 0 && ui->selected < ui->itemCount && ui->items[ui->selected].help != NULL)
        Ui_Wrapped(SCREEN_TOP, MARGIN, SCREEN_HEIGHT - 6 - 2 * LINE_H, NOTES_W, SCREEN_HEIGHT, C_DIM,
                   ui->items[ui->selected].help);
}

static void Ui_DrawMainBottom(const UiState *ui)
{
    for (int i = 0; i < ui->itemCount; ++i)
    {
        const UiItem *it = &ui->items[i];
        int y = ITEM_Y + i * ITEM_H;

        if (i == ui->selected)
            Ui_Text(SCREEN_BOTTOM, CURSOR_X, y, C_TITLE, ">");
        Ui_Text(SCREEN_BOTTOM, LABEL_X, y, it->attention ? C_GOOD : C_TEXT, it->label);
        if (it->value != NULL)
            Ui_TextRight(SCREEN_BOTTOM, SCREEN_BOTTOM_WIDTH - MARGIN, y,
                         strcmp(it->value, "ON") == 0 ? C_GOOD : C_DIM, it->value);
    }
}

// ---------------------------------------------------------------- 版の一覧

int Ui_VersionRows(void)
{
    return (HELP_Y - ITEM_Y) / ITEM_H;
}

static void Ui_DrawHistoryBottom(const UiState *ui)
{
    int rows = Ui_VersionRows();

    for (int i = 0; i < rows && ui->versionScroll + i < ui->versionCount; ++i)
    {
        int idx = ui->versionScroll + i, y = ITEM_Y + i * ITEM_H, x;
        const UiVersion *v = &ui->versions[idx];

        if (idx == ui->versionSelected)
            Ui_Text(SCREEN_BOTTOM, CURSOR_X, y, C_TITLE, ">");
        x = LABEL_X + Ui_Text(SCREEN_BOTTOM, LABEL_X, y, C_TEXT, v->tag) + 12;
        if (v->date != NULL && v->date[0] != 0)
            x += Ui_Text(SCREEN_BOTTOM, x, y, C_DIM, v->date) + 12;
        if (v->latest)
            x += Ui_Text(SCREEN_BOTTOM, x, y, C_GOOD, TR("最新", "latest")) + 8;
        if (v->current)
            Ui_Text(SCREEN_BOTTOM, x, y, C_YELLOW, TR("使用中", "in use"));
    }
    if (ui->versionScroll > 0)
        Ui_TextRight(SCREEN_BOTTOM, SCREEN_BOTTOM_WIDTH - MARGIN, ITEM_Y, C_TITLE, "^");
    if (ui->versionScroll + rows < ui->versionCount)
        Ui_TextRight(SCREEN_BOTTOM, SCREEN_BOTTOM_WIDTH - MARGIN, ITEM_Y + (rows - 1) * ITEM_H, C_TITLE, "v");
    if (ui->versionCount == 0)
        Ui_Text(SCREEN_BOTTOM, LABEL_X, ITEM_Y, C_DIM, ui->notesEmpty != NULL ? ui->notesEmpty : "");
}

static void Ui_DrawNotesBottom(void)
{
    Ui_Wrapped(SCREEN_BOTTOM, MARGIN, ITEM_Y, SCREEN_BOTTOM_WIDTH - 2 * MARGIN, HELP_Y, C_DIM,
               TR("上下: 1 行ずつ送る\n左右・L・R: 1 画面ずつ送る", "Up/Down: scroll by line\nLeft/Right, L/R: scroll by page"));
}

// ---------------------------------------------------------------- ソースコード

static void Ui_DrawSourceTop(const UiState *ui)
{
    int y = BODY_Y;

    Ui_TopTitle(TR("ソースコード", "Source code"), NULL);
    y = Ui_Wrapped(SCREEN_TOP, MARGIN, y, NOTES_W, SCREEN_HEIGHT, C_TEXT,
                   TR("boot.firm_Customize-Model は GNU GPL v3 のもとで公開しています。ソースコードは次の場所で入手できます。",
                      "boot.firm_Customize-Model is released under the GNU GPL v3. The source code is available at:"));
    y += LINE_H / 2;
    if (ui->sourceUrl != NULL)
    {
        y = Ui_Wrapped(SCREEN_TOP, MARGIN, y, NOTES_W, SCREEN_HEIGHT, C_TITLE, ui->sourceUrl);
        y += LINE_H / 2;
        y = Ui_Wrapped(SCREEN_TOP, MARGIN, y, NOTES_W, SCREEN_HEIGHT, C_DIM,
                       TR("下の画面の QR コードを読み取っても開けます。", "You can also scan the QR code on the lower screen."));
    }
    else
        y = Ui_Wrapped(SCREEN_TOP, MARGIN, y, NOTES_W, SCREEN_HEIGHT, C_BAD,
                       TR("ソースコードの場所が設定されていません。", "The source code location is not set."));

    y += LINE_H;
    Ui_Text(SCREEN_TOP, MARGIN, y, C_DIM, TR("元にした Luma3DS:", "Based on Luma3DS:"));
    Ui_Wrapped(SCREEN_TOP, MARGIN, y + LINE_H, NOTES_W, SCREEN_HEIGHT, C_TITLE, ui->upstreamUrl);
}

static void Ui_DrawSourceBottom(const UiState *ui)
{
    if (ui->qr != NULL)
    {
        // QR コードの周りには、明るい余白（4 マス）が要る
        int size = qrcodegen_getSize(ui->qr), cells = size + 8;
        int scale = (HELP_Y - 8) / cells;
        if (scale > (SCREEN_BOTTOM_WIDTH - 2 * MARGIN) / cells)
            scale = (SCREEN_BOTTOM_WIDTH - 2 * MARGIN) / cells;
        int ox = (SCREEN_BOTTOM_WIDTH - cells * scale) / 2, oy = (HELP_Y - 4 - cells * scale) / 2;

        Screen_FillRect(SCREEN_BOTTOM, ox, oy, cells * scale, cells * scale, 0xFFFFFF, 255);
        for (int y = 0; y < size; ++y)
            for (int x = 0; x < size; ++x)
                if (qrcodegen_getModule(ui->qr, x, y))
                    Screen_FillRect(SCREEN_BOTTOM, ox + (x + 4) * scale, oy + (y + 4) * scale, scale, scale, 0x000000, 255);
    }
}

// ---------------------------------------------------------------- 重ねるもの（下の画面を置き換える）

static void Ui_DrawOverlay(const UiState *ui)
{
    int y = ITEM_Y, w = SCREEN_BOTTOM_WIDTH - 2 * MARGIN;

    Screen_FillRect(SCREEN_BOTTOM, 0, 0, SCREEN_BOTTOM_WIDTH, SCREEN_HEIGHT, C_BG, 255);
    y = Ui_Wrapped(SCREEN_BOTTOM, MARGIN, y, w, HELP_Y, C_TEXT, ui->overlayText);

    if (ui->overlay != UI_OVERLAY_PROGRESS)
        return;

    char line[96];
    int barY = y + 4 + LINE_H + 4, barW = w;    // 「ダウンロード中 45%」の下

    if (ui->progressTotal > 0)
    {
        uint64_t done = ui->progressDone < ui->progressTotal ? ui->progressDone : ui->progressTotal;
        snprintf(line, sizeof(line), "%s  %d%%", ui->progressLabel, (int)(done * 100 / ui->progressTotal));
        Ui_Text(SCREEN_BOTTOM, MARGIN, y + 4, C_YELLOW, line);
        Screen_FillRect(SCREEN_BOTTOM, MARGIN, barY, (int)((uint64_t)barW * done / ui->progressTotal), 6, C_TITLE, 255);
    }
    else
    {
        // 大きさが分からないときは、短い帯を左から右へ動かす
        int seg = barW / 5, pos = (int)(ui->frame * 4 % (uint32_t)(barW + seg)) - seg;
        int x0 = pos < 0 ? 0 : pos, x1 = pos + seg > barW ? barW : pos + seg;
        Ui_Text(SCREEN_BOTTOM, MARGIN, y + 4, C_YELLOW, ui->progressLabel);
        if (x1 > x0)
            Screen_FillRect(SCREEN_BOTTOM, MARGIN + x0, barY, x1 - x0, 6, C_TITLE, 255);
    }
    Screen_FillRect(SCREEN_BOTTOM, MARGIN, barY + 7, barW, 1, C_RULE, 255);
}

// ---------------------------------------------------------------- 全体

void Ui_Draw(const UiState *ui)
{
    g_c = &g_colors[ui->light];
    Screen_Begin();
    Screen_FillRect(SCREEN_TOP, 0, 0, SCREEN_TOP_WIDTH, SCREEN_HEIGHT, C_BG, 255);
    Screen_FillRect(SCREEN_BOTTOM, 0, 0, SCREEN_BOTTOM_WIDTH, SCREEN_HEIGHT, C_BG, 255);

    switch (ui->view)
    {
        case UI_VIEW_MAIN:
            Ui_DrawMainTop(ui);
            Ui_DrawMainBottom(ui);
            break;
        case UI_VIEW_NOTES:
            Ui_DrawNotes(ui);
            Ui_DrawNotesBottom();
            break;
        case UI_VIEW_HISTORY:
            Ui_DrawNotes(ui);
            Ui_DrawHistoryBottom(ui);
            break;
        case UI_VIEW_SOURCE:
            Ui_DrawSourceTop(ui);
            Ui_DrawSourceBottom(ui);
            break;
    }

    if (ui->overlay != UI_OVERLAY_NONE)
        Ui_DrawOverlay(ui);
    Ui_DrawButtons(ui);
    Screen_End();
}

// ---------------------------------------------------------------- タッチ

int Ui_HitItem(const UiState *ui, int x, int y)
{
    if (ui->overlay != UI_OVERLAY_NONE || ui->view != UI_VIEW_MAIN || y < ITEM_Y)
        return -1;
    int i = (y - ITEM_Y) / ITEM_H;
    return x >= 0 && x < SCREEN_BOTTOM_WIDTH && i < ui->itemCount ? i : -1;
}

int Ui_HitVersion(const UiState *ui, int x, int y)
{
    if (ui->overlay != UI_OVERLAY_NONE || ui->view != UI_VIEW_HISTORY || y < ITEM_Y || y >= HELP_Y)
        return -1;
    int i = ui->versionScroll + (y - ITEM_Y) / ITEM_H;
    return x >= 0 && x < SCREEN_BOTTOM_WIDTH && i < ui->versionCount ? i : -1;
}

int Ui_HitButtons(const UiState *ui, int x, int y)
{
    static const int ids[BUTTON_MAX] = { 1, 0, 2 };
    char a[48], b[48];
    const char *labels[BUTTON_MAX];
    int bx[BUTTON_MAX], bw[BUTTON_MAX];

    Ui_ButtonLabels(ui, a, b, labels);
    Ui_ButtonRects(labels, bx, bw);
    if (y < HELP_Y - 4 || y >= SCREEN_HEIGHT)
        return -1;
    for (int i = 0; i < BUTTON_MAX; ++i)
        if (labels[i] != NULL && x >= bx[i] - 4 && x < bx[i] + bw[i] + 4)
            return ids[i];
    return -1;
}

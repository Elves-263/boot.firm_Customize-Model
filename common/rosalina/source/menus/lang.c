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

#include "menus/lang.h"

static const char *const g_strings[STR_COUNT] =
{
    [STR_OPERATION_FAILED_0X] = TR("失敗しました（0x%08lx）。", "Operation failed (0x%08lx)."),
    [STR_POWER_OFF_REBOOT] = TR("電源オフ／再起動", "Power off / reboot"),
    [STR_PRESS_A_TO_POWER_OFF_PRESS_Y_TO_REBOOT_P] = TR("A: 電源オフ\nY: 再起動\nB: 戻る", "A: power off\nY: reboot\nB: back"),
    [STR_VERSION_SWITCH_CHAINLOAD] = TR("バージョンの切り替え", "Version switch"),
    [STR_CURRENT_V_V] = TR("現在 v%s   ->   v%s", "Current v%s   ->   v%s"),
    [STR_A_LAUNCH_B_GO_BACK] = TR("A: 切り替え   B: 戻る", "A: switch   B: back"),
    [STR_ROSALINA_SYSTEM_INFO] = TR("Rosalina -- システム情報", "Rosalina -- System info"),
    [STR_TOP_SCREEN_TYPE] = TR("上画面の種類:       %s\n", "Top screen type:    %s\n"),
    [STR_BOTTOM_SCREEN_TYPE] = TR("下画面の種類:       %s\n\n", "Bottom screen type: %s\n\n"),
    [STR_KERNEL_VERSION] = TR("カーネル:           %lu.%lu-%lu\n\n", "Kernel version:     %lu.%lu-%lu\n\n"),
    [STR_MCU_FW_VERSION] = TR("MCU ファーム:       %lu.%lu\n", "MCU FW version:     %lu.%lu\n"),
    [STR_PMIC_VENDOR] = TR("PMIC メーカー:      %hhu\n", "PMIC vendor:        %hhu\n"),
    [STR_BATTERY_VENDOR] = TR("電池メーカー:       %hhu\n\n", "Battery vendor:     %hhu\n\n"),
    [STR_ROSALINA_DEBUG_INFO] = TR("Rosalina -- デバッグ情報", "Rosalina -- Debug info"),
    [STR_RETURN_TO_HOME_MENU] = TR("HOME メニューに戻る", "Return to HOME Menu"),
    [STR_PRESS_A_TO_CONFIRM_PRESS_B_TO_GO_BACK] = TR("A: 決定\nB: 戻る", "A: confirm\nB: back"),
    [STR_TIME_SPENT_CONVERTING_MS] = TR("変換の時間:               %5lums\n", "Time spent converting:    %5lums\n"),
    [STR_TIME_SPENT_WRITING_FILES_MS] = TR("書き込みの時間:           %5lums\n", "Time spent writing files: %5lums\n"),
    [STR_FREE_MB] = TR("空き: %u MB", "Free: %u MB"),
    [STR_PRESS_ANY_BUTTON] = TR("どれかボタンを押してください。", "Press any button."),
    [STR_A_OPEN_X_COPY_Y_CUT_R_PASTE_LR_SORT] = TR("A:開く X:コピー Y:切り取り R:貼り付け <>:並び", "A:open X:copy Y:cut R:paste <>:sort"),
    [STR_SORT_ADDED] = TR("登録順", "Added"),
    [STR_SORT_NAME] = TR("名前順", "Name"),
    [STR_SORT_SIZE] = TR("サイズ順", "Size"),
    [STR_EMPTY_FILE] = TR("（空のファイル）", "(empty file)"),
    [STR_UP_DOWN_LINE_LEFT_RIGHT_PAGE] = TR("上下: 1 行   左右: 1 ページ", "Up/Down: line   Left/Right: page"),
    [STR_X_TEXT_HEX_B_BACK] = TR("X: テキスト／16 進数   B: 戻る", "X: text/hex   B: back"),
    [STR_SELECT_DELETE_START_RENAME_L_NEW_DIR] = TR("SELECT:削除 START:名前変更 L:新フォルダ", "SELECT:delete START:rename L:new folder"),
    [STR_ANY_USERNAME_AND_PASSWORD] = TR("ユーザー名とパスワードは何でも可。\n", "Any username and password.\n"),
    [STR_PASSIVE_MODE_ONLY] = TR("パッシブモードのみ。\n\n", "Passive mode only.\n\n"),
    [STR_KEEPS_RUNNING_WITH_THE_MENU_CLOSED] = TR("メニューを閉じても動き続けます。\n", "Keeps running with the menu closed.\n"),
    [STR_A_STOP_B_BACK] = TR("A: 止める     B: 戻る", "A: stop      B: back"),
    [STR_SERVES_THE_SD_CARD_OVER_FTP_SO_YOU] = TR("SD カードを FTP で公開し、\n", "Serves the SD card over FTP so you\n"),
    [STR_CAN_MANAGE_FILES_FROM_A_PC] = TR("PC からファイルを扱えます。\n\n", "can manage files from a PC.\n\n"),
    [STR_WI_FI_MUST_BE_CONNECTED_FIRST] = TR("先に Wi-Fi に接続してください。\n", "Wi-Fi must be connected first.\n"),
    [STR_NOT_ENOUGH_MEMORY] = TR("メモリが足りません。\n", "Not enough memory.\n"),
    [STR_A_START_B_BACK] = TR("A: 始める     B: 戻る", "A: start     B: back"),
    [STR_A_USE_X_PIN_UNPIN_B_NONE] = TR("A: 使う   X: 固定／解除   B: 使わない", "A: use   X: pin/unpin   B: none"),
    [STR_X_BACK_TO_THE_WARNING] = TR("X: 警告に戻る", "X: back to the warning"),
    [STR_MATCHED_LOCATIONS] = TR("見つかった場所", "Matched locations"),
    [STR_A_TYPE_X_BACKSPACE_Y_CASE] = TR("A: 入力   X: 1 字消す   Y: 大文字／小文字", "A: type   X: backspace   Y: case"),
    [STR_START_CONFIRM_B_CANCEL] = TR("START: 決定   B: やめる", "START: confirm   B: cancel"),
    [STR_A_CONFIRM_B_CANCEL] = TR("A: 決定         B: やめる", "A: confirm      B: cancel"),
    [STR_COPYING] = TR("コピーしています...", "Copying..."),
    [STR_CLIPBOARD_IS_EMPTY] = TR("クリップボードは空です。", "The clipboard is empty."),
    [STR_PATH_IS_TOO_LONG] = TR("パスが長すぎます。", "The path is too long."),
    [STR_SOURCE_AND_DESTINATION_ARE_THE_SAME] = TR("コピー元とコピー先が同じです。", "Source and destination are the same."),
    [STR_CANNOT_PASTE_A_FOLDER_INTO_ITSELF] = TR("フォルダをそのフォルダの中には貼れません。", "Cannot paste a folder into itself."),
    [STR_ALREADY_EXISTS_OVERWRITE] = TR("すでにあります。上書きしますか？", "Already exists. Overwrite?"),
    [STR_MOVED] = TR("移動しました。", "Moved."),
    [STR_FAILED] = TR("失敗しました。", "Failed."),
    [STR_COPIED_BUT_FAILED_TO_DELETE_THE_SOURCE] = TR("コピーしましたが、元を消せませんでした。", "Copied, but failed to delete the source."),
    [STR_COPIED] = TR("コピーしました。", "Copied."),
    [STR_CLIP_EMPTY] = TR("（空）", "(empty)"),
    [STR_CLIP_CUT] = TR("[切り取り] ", "[cut] "),
    [STR_CLIP_COPY] = TR("[コピー] ", "[copy] "),
    [STR_DELETE_THIS_FOLDER] = TR("フォルダと中身をすべて削除しますか？", "Delete this folder and everything in it?"),
    [STR_DELETE_THIS_FILE] = TR("このファイルを削除しますか？", "Delete this file?"),
    [STR_NEW_FOLDER_NAME] = TR("新しいフォルダの名前", "New folder name"),
    [STR_RENAME] = TR("名前の変更", "Rename"),
    [STR_RUNNING] = TR("動いています。\n\n", "Running.\n\n"),
    [STR_STOPPED] = TR("止まっています。\n\n", "Stopped.\n\n"),
    [STR_SESSIONS] = TR("接続:   %lu\n", "Sessions:  %lu\n"),
    [STR_TRANSFERS] = TR("転送:   %lu\n\n", "Transfers: %lu\n\n"),
    [STR_RISK] = TR("危険度: %u%% (%s)", "Risk: %u%% (%s)"),
    [STR_LITTLE_DANGEROUS] = TR("少し危険", "slightly dangerous"),
    [STR_MODERATELY_DANGEROUS] = TR("やや危険", "moderately dangerous"),
    [STR_VERY_DANGEROUS] = TR("とても危険", "very dangerous"),
    [STR_COULD_NOT_SCAN_THE_WHOLE_FILE] = TR("最後まで調べられませんでした", "Could not scan the whole file"),
    [STR_MATCHED_KEYWORDS] = TR("見つかった言葉:", "Matched keywords:"),
    [STR_NONE] = TR("（なし）", "(none)"),
    [STR_CHEATS_AUTO_HINT] = TR("A: 入/切  X: 自動（+ 10 秒後、! すぐ）", "A: on/off  X: auto (+ after 10 s, ! at once)"),
};

const char *Lang(u32 id)
{
    return id < STR_COUNT ? g_strings[id] : "";
}

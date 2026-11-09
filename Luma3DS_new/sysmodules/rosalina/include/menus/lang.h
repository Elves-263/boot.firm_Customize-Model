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

#include <3ds/types.h>

// 画面の言葉の言語。/luma/lang.cfg（0: 英語、1: 日本語。v1.3 までの「Language / Gengo」と同じ）に保存する。
// 無ければ本体の言語が日本語なら日本語、それ以外は英語にして lang.cfg に書く。
// 更新アプリと Luma の起動時の設定画面も同じファイルを読む（設定画面は lang.cfg が無ければ日本語）
typedef enum LangCode {
    LANG_EN = 0,
    LANG_JP = 1,
    LANG_COUNT
} LangCode;

#define LANG_CFG_PATH "/luma/lang.cfg"

// 日本語と英語の組の文字列。"\x1B" 日本語 "\0" 英語 の 1 つの文字列リテラルになるので、静的な表の中にも書ける。
// Draw_DrawString などの描く関数・sprintf（fmt.c の書式と %s）・Draw_TextColumns などは、
// 渡された組を Lang_Str() で今の言語のほうにしてから使う。それ以外の関数（strcpy・strlen など）に
// 渡すときは、先に Lang_Str() を通す
#define TR(ja, en) "\x1B" ja "\0" en

// TR() の組なら今の言語のほうを、それ以外ならそのまま返す
const char *Lang_Str(const char *s);
bool Lang_IsEnglish(void);

void RosalinaMenu_Language(void);

// 改変版の画面に出す文字列（日本語と英語）。Lang() で引く
enum StrId {
    STR_OPERATION_FAILED_0X,
    STR_POWER_OFF_REBOOT,
    STR_PRESS_A_TO_POWER_OFF_PRESS_Y_TO_REBOOT_P,
    STR_VERSION_SWITCH_CHAINLOAD,
    STR_CURRENT_V_V,
    STR_A_LAUNCH_B_GO_BACK,
    STR_ROSALINA_SYSTEM_INFO,
    STR_TOP_SCREEN_TYPE,
    STR_BOTTOM_SCREEN_TYPE,
    STR_KERNEL_VERSION,
    STR_MCU_FW_VERSION,
    STR_PMIC_VENDOR,
    STR_BATTERY_VENDOR,
    STR_ROSALINA_DEBUG_INFO,
    STR_RETURN_TO_HOME_MENU,
    STR_PRESS_A_TO_CONFIRM_PRESS_B_TO_GO_BACK,
    STR_TIME_SPENT_CONVERTING_MS,
    STR_TIME_SPENT_WRITING_FILES_MS,
    STR_FREE_MB,
    STR_PRESS_ANY_BUTTON,
    STR_A_OPEN_X_COPY_Y_CUT_R_PASTE_LR_SORT,
    STR_SORT_ADDED,
    STR_SORT_NAME,
    STR_SORT_SIZE,
    STR_EMPTY_FILE,
    STR_UP_DOWN_LINE_LEFT_RIGHT_PAGE,
    STR_X_TEXT_HEX_B_BACK,
    STR_SELECT_DELETE_START_RENAME_L_NEW_DIR,
    STR_ANY_USERNAME_AND_PASSWORD,
    STR_PASSIVE_MODE_ONLY,
    STR_KEEPS_RUNNING_WITH_THE_MENU_CLOSED,
    STR_A_STOP_B_BACK,
    STR_SERVES_THE_SD_CARD_OVER_FTP_SO_YOU,
    STR_CAN_MANAGE_FILES_FROM_A_PC,
    STR_WI_FI_MUST_BE_CONNECTED_FIRST,
    STR_NOT_ENOUGH_MEMORY,
    STR_A_START_B_BACK,
    STR_A_USE_X_PIN_UNPIN_B_NONE,
    STR_X_BACK_TO_THE_WARNING,
    STR_MATCHED_LOCATIONS,
    STR_A_TYPE_X_BACKSPACE_Y_CASE,
    STR_START_CONFIRM_B_CANCEL,
    STR_A_CONFIRM_B_CANCEL,
    STR_COPYING,
    STR_CLIPBOARD_IS_EMPTY,
    STR_PATH_IS_TOO_LONG,
    STR_SOURCE_AND_DESTINATION_ARE_THE_SAME,
    STR_CANNOT_PASTE_A_FOLDER_INTO_ITSELF,
    STR_ALREADY_EXISTS_OVERWRITE,
    STR_MOVED,
    STR_FAILED,
    STR_COPIED_BUT_FAILED_TO_DELETE_THE_SOURCE,
    STR_COPIED,
    STR_CLIP_EMPTY,
    STR_CLIP_CUT,
    STR_CLIP_COPY,
    STR_DELETE_THIS_FOLDER,
    STR_DELETE_THIS_FILE,
    STR_NEW_FOLDER_NAME,
    STR_RENAME,
    STR_RUNNING,
    STR_STOPPED,
    STR_SESSIONS,
    STR_TRANSFERS,
    STR_RISK,
    STR_LITTLE_DANGEROUS,
    STR_MODERATELY_DANGEROUS,
    STR_VERY_DANGEROUS,
    STR_COULD_NOT_SCAN_THE_WHOLE_FILE,
    STR_MATCHED_KEYWORDS,
    STR_NONE,
    STR_CHEATS_AUTO_HINT,
    STR_COUNT
};

const char *Lang(u32 id);

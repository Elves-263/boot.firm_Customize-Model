/*
*   This file is part of Luma3DS
*   Copyright (C) 2016-2018 Aurora Wright, TuxSH
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

// 版の名前（SD:/3ds/luma/<名前>/boot.firm と、版の切り替えのしるし・running.txt に使う）。
// 新しい方の Luma は、上流の版が変わってもフォルダが変わらないよう "new" にする
#define FORK_LUMA_ID       "9.1"
#define FORK_OTHER_ID      "new"
// 画面に出す版。LUMA_NEW_VERSION は Makefile が Luma3DS_new/UPSTREAM_VERSION から渡す（v13.5 なら "13.5"）
#define FORK_LUMA_VERSION  "9.1"
#define FORK_OTHER_VERSION LUMA_NEW_VERSION
#define FORK_PACKAGE_TAG   "v1.4"
#define FORK_TITLE_TAG       "[v" FORK_LUMA_VERSION " / " FORK_PACKAGE_TAG "]"
#define FORK_TITLE_TAG_SHORT "[v" FORK_LUMA_VERSION "]"

void LogLine(const char *msg);
void SDSpaceWarnIfLow(void);
void ChainloadCheckFailed(void);
void ChainloadWarnIfFailed(void);
extern bool g_safeMode;
#define MENU_WORK_BUF_SIZE 0x8000
extern u8 g_menuWorkBuf[MENU_WORK_BUF_SIZE];

#include <3ds/types.h>
#include "MyThread.h"
#include "utils.h"

#define HID_PAD           (REG32(0x10146000) ^ 0xFFF)

#define BUTTON_A          (1 << 0)
#define BUTTON_B          (1 << 1)
#define BUTTON_SELECT     (1 << 2)
#define BUTTON_START      (1 << 3)
#define BUTTON_RIGHT      (1 << 4)
#define BUTTON_LEFT       (1 << 5)
#define BUTTON_UP         (1 << 6)
#define BUTTON_DOWN       (1 << 7)
#define BUTTON_R1         (1 << 8)
#define BUTTON_L1         (1 << 9)
#define BUTTON_X          (1 << 10)
#define BUTTON_Y          (1 << 11)

#define DIRECTIONAL_KEYS  (BUTTON_DOWN | BUTTON_UP | BUTTON_LEFT | BUTTON_RIGHT)

#define DEFAULT_MENU_COMBO  (BUTTON_L1 | BUTTON_DOWN | BUTTON_SELECT)

#define CORE_APPLICATION  0
#define CORE_SYSTEM       1

typedef enum MenuItemAction {
    METHOD,
    MENU
} MenuItemAction;
typedef struct MenuItem {
    const char *title;

    MenuItemAction action_type;
    union {
        struct Menu *menu;
        void (*method)(void);
    };
} MenuItem;
typedef struct Menu {
    const char *title;

    u32 nbItems;
    MenuItem items[0x40];
} Menu;

extern bool terminationRequest;
extern Handle terminationRequestEvent;

extern u32 menuCombo;

u32     waitInputWithTimeout(u32 msec);
u32     waitInput(void);

u32     waitComboWithTimeout(u32 msec);
u32     waitCombo(void);

MyThread *menuCreateThread(void);
bool    menuEnter(void);
bool    menuEnterIfClosed(void);
bool    menuEnterWhenClosed(void);
void    menuEnterForFatalError(void);
void    menuLeave(void);
bool    menuIsOpen(void);
u32     menuPageMove(u32 pos, u32 first, u32 last, u32 page, bool forward);
void    menuThreadMain(void);
void    menuShow(Menu *root);
void    DispMessage(const char *title, const char *message);
u32     DispErrMessage(const char *title, const char *message, const Result error);
void    DisplayPluginMenu(u32   *cmdbuf);

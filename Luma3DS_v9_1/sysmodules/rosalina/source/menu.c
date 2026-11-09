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

#include <3ds.h>
#include "menu.h"
#include "draw.h"
#include "fmt.h"
#include "memory.h"
#include "ifile.h"
#include "menus.h"
#include "utils.h"
#include "plgloader.h"
#include "menus/n3ds.h"
#include "menus/ftpd.h"
#include "menus/cheats.h"
#include "font_jp.h"
#include "minisoc.h"
#include "update_check.h"
#include "menus/lang.h"

// 方向キーを押し続けたときの連続入力。v13.4 の hidSetRepeatParameters(200, 100) に合わせる
#define INPUT_REPEAT_DELAY      (SYSCLOCK_ARM11 / 5)    // 押してから連続入力を始めるまで（200 ms）
#define INPUT_REPEAT_INTERVAL   (SYSCLOCK_ARM11 / 10)   // 連続入力の間隔（100 ms）

static u32 inputRepeatKeys; // 最後に返したキー。そのまま押され続けていれば方向キーを繰り返す
static u64 inputRepeatTick; // 次に繰り返す時刻

u32 waitInputWithTimeout(u32 msec)
{
    bool pressedKey = false;
    u32 key = 0;
    u32 n = 0;

    //Wait for no keys to be pressed. Direction keys held since the last returned input are repeated
    while(HID_PAD && !terminationRequest && (msec == 0 || n < msec))
    {
        if((inputRepeatKeys & DIRECTIONAL_KEYS) != 0 && HID_PAD == inputRepeatKeys && svcGetSystemTick() >= inputRepeatTick)
        {
            inputRepeatTick = svcGetSystemTick() + INPUT_REPEAT_INTERVAL;
            return inputRepeatKeys & DIRECTIONAL_KEYS;
        }

        svcSleepThread(1 * 1000 * 1000LL);
        n++;
    }

    if(terminationRequest || (msec != 0 && n >= msec))
        return 0;

    inputRepeatKeys = 0;

    do
    {
        //Wait for a key to be pressed
        while(!HID_PAD && !terminationRequest && (msec == 0 || n < msec))
        {
            svcSleepThread(1 * 1000 * 1000LL);
            n++;
        }

        if(terminationRequest || (msec != 0 && n >= msec))
            return 0;

        key = HID_PAD;

        //Make sure it's pressed
        for(u32 i = 0x26000; i > 0; i --)
        {
            if(key != HID_PAD) break;
            if(i == 1) pressedKey = true;
        }
    }
    while(!pressedKey);

    inputRepeatKeys = key;
    inputRepeatTick = svcGetSystemTick() + INPUT_REPEAT_DELAY;

    return key;
}

u32 waitInput(void)
{
    return waitInputWithTimeout(0);
}

u32 waitComboWithTimeout(u32 msec)
{
    u32 key = 0;
    u32 n = 0;

    //Wait for no keys to be pressed
    while(HID_PAD && !terminationRequest && (msec == 0 || n < msec))
    {
        svcSleepThread(1 * 1000 * 1000LL);
        n++;
    }

    if(terminationRequest || (msec != 0 && n >= msec))
        return 0;

    do
    {
        svcSleepThread(1 * 1000 * 1000LL);
        n++;

        u32 tempKey = HID_PAD;

        for(u32 i = 0x26000; i > 0; i--)
        {
            if(tempKey != HID_PAD) break;
            if(i == 1) key = tempKey;
        }
    }
    while((!key || HID_PAD) && !terminationRequest && (msec == 0 || n < msec));

    if(terminationRequest || (msec != 0 && n >= msec))
        return 0;

    return key;
}

u32 waitCombo(void)
{
    return waitComboWithTimeout(0);
}

static MyThread menuThread;
static u8 ALIGN(8) menuThreadStack[0x1800];
static u8 batteryLevel = 255;
static u32 homeBtnPressed = 0;

MyThread *menuCreateThread(void)
{
    svcKernelSetState(0x10007, &homeBtnPressed);
    if(R_FAILED(MyThread_Create(&menuThread, menuThreadMain, menuThreadStack, sizeof(menuThreadStack), 52, CORE_SYSTEM)))
        svcBreak(USERBREAK_PANIC);
    return &menuThread;
}

extern bool isN3DS;
u32 menuCombo;

u32     DispWarningOnHome(void);

void    menuThreadMain(void)
{
    if(!isN3DS)
    {
        rosalinaMenu.nbItems--;
        for(u32 i = 0; i <= rosalinaMenu.nbItems; i++)
            rosalinaMenu.items[i] = rosalinaMenu.items[i+1];
    }
    else
        N3DSMenu_UpdateStatus();

    bool isAcURegistered = false;
    bool chainloadChecked = false;

    while(!terminationRequest)
    {
        // SD を読めるようになってから（ac:u が登録されたあと）1 度だけ確かめる
        if(!chainloadChecked)
        {
            bool registered = false;
            if(R_SUCCEEDED(srvIsServiceRegistered(&registered, "ac:u")) && registered)
            {
                ChainloadCheckFailed();
                chainloadChecked = true;
            }
        }

        if((HID_PAD & menuCombo) == menuCombo)
        {
            if (!isAcURegistered)
                isAcURegistered = R_SUCCEEDED(srvIsServiceRegistered(&isAcURegistered, "ac:u"))
                    && isAcURegistered;

            // 3gx セレクターなどを表示している間は開かない
            else if(menuEnterIfClosed())
            {
                if(isN3DS) N3DSMenu_UpdateStatus();
                PluginLoader__UpdateMenu();
                FontJp_Retry();
                menuShow(&rosalinaMenu);
                menuLeave();
            }
        }
        else
        {
            Cheat_ApplyCheats();
            Cheat_AutoEnable();
        }

        // Check for home button on O3DS Mode3 with plugin loaded
        if (homeBtnPressed != 0)
        {
            if (DispWarningOnHome())
                svcKernelSetState(7); ///< reboot is fine since exiting a mode3 game reboot anyway

            homeBtnPressed = 0;
        }

        Ftpd_ReleaseMemory();

        svcSleepThread(50 * 1000 * 1000LL);
    }
}

static s32 menuRefCount = 0;
bool menuEnter(void)
{
    bool ok = true;

    Draw_Lock();
    if(menuRefCount == 0)
    {
        inputRepeatKeys = 0; // 前に開いたときの押しっぱなしを引き継がない
        ok = R_SUCCEEDED(Draw_AllocateFramebufferCache());
        if(ok)
        {
            svcKernelSetState(0x10000, 1);
            svcSleepThread(5 * 1000 * 100LL);
            Draw_SetupFramebuffer();
            Draw_ClearFramebuffer();
        }
    }
    if(ok)
        ++menuRefCount;
    Draw_Unlock();

    return ok;
}

// Rosalina メニューや 3gx セレクターなど、ほかの画面が開いていないときだけ開く
bool menuEnterIfClosed(void)
{
    bool ok = false;

    Draw_Lock();
    if(menuRefCount == 0)
        ok = menuEnter();
    Draw_Unlock();

    return ok;
}

// 3gx セレクターなどの画面を開く。Rosalina メニューを開いている間は閉じるまで待つ。
// 開けなかったとき（メモリ不足など）は false
bool menuEnterWhenClosed(void)
{
    for(;;)
    {
        bool wasOpen = menuIsOpen();

        if(menuEnterIfClosed())
            return true;
        if(!wasOpen && !menuIsOpen())
            return false;
        svcSleepThread(100 * 1000 * 1000LL);
    }
}

// 致命的エラーの表示用。メニュー用のメモリ（画面の退避先）を確保できなくても、
// 退避せずに画面を切り替えて表示する（このあとは再起動するので元に戻す必要がない）
void menuEnterForFatalError(void)
{
    if(menuEnter())
        return;

    Draw_Lock();
    ++menuRefCount;
    svcKernelSetState(0x10000, 1);
    svcSleepThread(5 * 1000 * 100LL);
    Draw_SetupFramebuffer();
    Draw_ClearFramebuffer();
    Draw_Unlock();
}

// メニュー（3gx セレクターなどの画面を含む）を表示しているか
bool menuIsOpen(void)
{
    return menuRefCount > 0;
}

void menuLeave(void)
{
    svcSleepThread(50 * 1000 * 1000);

    Draw_Lock();
    if(--menuRefCount == 0)
    {
        Draw_FlushFramebuffer();
        Draw_RestoreFramebuffer();
        Draw_FreeFramebufferCache();
        svcKernelSetState(0x10000, 1);
    }
    Draw_Unlock();
}

#define MENU_MAX_ROWS 16

static u32 menuScrollOffset;

u8 g_menuWorkBuf[MENU_WORK_BUF_SIZE];

static void menuDraw(Menu *menu, u32 selected)
{
    char versionString[16];
    s64 out;
    u32 version, commitHash;
    bool isRelease;
    bool isMcuHwcRegistered;

    if(R_SUCCEEDED(srvIsServiceRegistered(&isMcuHwcRegistered, "mcu::HWC")) && isMcuHwcRegistered && R_SUCCEEDED(mcuHwcInit()))
    {
        if(R_FAILED(MCUHWC_GetBatteryLevel(&batteryLevel)))
            batteryLevel = 255;
        mcuHwcExit();
    }
    else
        batteryLevel = 255;

    svcGetSystemInfo(&out, 0x10000, 0);
    version = (u32)out;

    svcGetSystemInfo(&out, 0x10000, 1);
    commitHash = (u32)out;

    svcGetSystemInfo(&out, 0x10000, 0x200);
    isRelease = (bool)out;

    if(GET_VERSION_REVISION(version) == 0)
        sprintf(versionString, "v%u.%u", GET_VERSION_MAJOR(version), GET_VERSION_MINOR(version));
    else
        sprintf(versionString, "v%u.%u.%u", GET_VERSION_MAJOR(version), GET_VERSION_MINOR(version), GET_VERSION_REVISION(version));

    // タイトルの右に「[v動作中の Luma / 改変版の版]」と「セーフ」（セーフモードのとき）を並べる。
    // 右上の 15 桁分は IP アドレスの表示に使うため、収まらないときは「[v動作中の Luma]」にし、
    // それでも収まらなければ省く（「セーフ」は残す）
    static const char *const tags[] = { FORK_TITLE_TAG, FORK_TITLE_TAG_SHORT, "" };
    static const char safeTag[] = TR("セーフ", "SAFE");
    const u32 endCol = (SCREEN_BOT_WIDTH - 20) / SPACING_X - 15;
    const u32 safeCols = g_safeMode ? Draw_TextColumns(safeTag) : 0;
    const char *tag = "";
    u32 tagCol = Draw_TextColumns(menu->title) + 1;
    if (tagCol < 15)
        tagCol = 15;
    for (u32 i = 0; i < sizeof(tags) / sizeof(tags[0]); ++i)
    {
        u32 len = strlen(tags[i]);
        if (tagCol + len + (safeCols != 0 && len != 0 ? 1 : 0) + safeCols <= endCol)
        {
            tag = tags[i];
            break;
        }
    }
    Draw_DrawString(10, 10, COLOR_TITLE, menu->title);
    Draw_DrawString(10 + tagCol * SPACING_X, 10, COLOR_TITLE, tag);
    if (g_safeMode)
        Draw_DrawString(10 + (tagCol + strlen(tag) + (tag[0] != 0 ? 1 : 0)) * SPACING_X, 10, COLOR_RED, safeTag);

    u32 nbItems = menu->nbItems;

    if(nbItems <= MENU_MAX_ROWS)
        menuScrollOffset = 0;
    else
    {
        if(selected < menuScrollOffset)
            menuScrollOffset = selected;
        else if(selected >= menuScrollOffset + MENU_MAX_ROWS)
            menuScrollOffset = selected - MENU_MAX_ROWS + 1;

        if(menuScrollOffset > nbItems - MENU_MAX_ROWS)
            menuScrollOffset = nbItems - MENU_MAX_ROWS;
    }

    for(u32 row = 0; row < MENU_MAX_ROWS; row++)
    {
        u32 i = menuScrollOffset + row;
        u32 y = 30 + row * SPACING_Y;

        if(i < nbItems)
        {
            // 前の項目の字を消すため、44 桁まで空白で埋める（%-44s はバイト数で埋めるので使わない）
            const char *title = menu->items[i].title;
            u32 cols = Draw_TextColumns(title);
            Draw_DrawFormattedString(30, y, COLOR_WHITE, "%s%*s", title, cols < 44 ? (int)(44 - cols) : 0, "");
            Draw_DrawCharacter(10, y, COLOR_TITLE, i == selected ? '>' : ' ');
        }
        else
        {
            Draw_DrawFormattedString(30, y, COLOR_WHITE, "%-44s", "");
            Draw_DrawCharacter(10, y, COLOR_TITLE, ' ');
        }
    }

    if(nbItems > MENU_MAX_ROWS)
    {
        Draw_DrawCharacter(SCREEN_BOT_WIDTH - 16, 30, COLOR_TITLE,
                           menuScrollOffset > 0 ? '^' : ' ');
        Draw_DrawCharacter(SCREEN_BOT_WIDTH - 16, 30 + (MENU_MAX_ROWS - 1) * SPACING_Y, COLOR_TITLE,
                           menuScrollOffset + MENU_MAX_ROWS < nbItems ? 'v' : ' ');
    }

    if(miniSocEnabled)
    {
        char ipBuffer[17];
        u32 ip = gethostid();
        u8 *addr = (u8 *)&ip;
        int n = sprintf(ipBuffer, "%hhu.%hhu.%hhu.%hhu", addr[0], addr[1], addr[2], addr[3]);
        Draw_DrawString(SCREEN_BOT_WIDTH - 10 - SPACING_X * n, 10, COLOR_WHITE, ipBuffer);
    }

    Draw_DrawFormattedString(SCREEN_BOT_WIDTH - 10 - 4 * SPACING_X, SCREEN_BOT_HEIGHT - 20, COLOR_WHITE, "    ");

    if(batteryLevel != 255)
        Draw_DrawFormattedString(SCREEN_BOT_WIDTH - 10 - 4 * SPACING_X, SCREEN_BOT_HEIGHT - 20, COLOR_WHITE, "%02hhu%%", batteryLevel);
    else
        Draw_DrawString(SCREEN_BOT_WIDTH - 10 - 4 * SPACING_X, SCREEN_BOT_HEIGHT - 20, COLOR_WHITE, "    ");

    if(isRelease)
        Draw_DrawFormattedString(10, SCREEN_BOT_HEIGHT - 20, COLOR_TITLE, "Luma3DS %s", versionString);
    else
        Draw_DrawFormattedString(10, SCREEN_BOT_HEIGHT - 20, COLOR_TITLE, "Luma3DS %s-%08x", versionString, commitHash);

    Draw_FlushFramebuffer();
}

// 左右キーで 1 画面（page 項目）ずつ動かした先を返す。first から last の範囲で、
// 端を越えるときは端で止め、すでに端にいるときは反対の端へ回る
u32 menuPageMove(u32 pos, u32 first, u32 last, u32 page, bool forward)
{
    if(forward)
    {
        if(pos == last)
            return first;
        return last - pos > page ? pos + page : last;
    }

    if(pos == first)
        return last;
    return pos - first > page ? pos - page : first;
}

void menuShow(Menu *root)
{
    u32 selectedItem = 0;
    Menu *currentMenu = root;
    u32 nbPreviousMenus = 0;
    Menu *previousMenus[0x80];
    u32 previousSelectedItems[0x80];

    SDSpaceWarnIfLow();
    ChainloadWarnIfFailed();
    UpdateCheck_ShowNotice();

    Draw_Lock();
    Draw_ClearFramebuffer();
    Draw_FlushFramebuffer();
    menuDraw(currentMenu, selectedItem);
    Draw_Unlock();

    do
    {
        u32 pressed = waitInputWithTimeout(1000);

        if(pressed & BUTTON_A)
        {
            Draw_Lock();
            Draw_ClearFramebuffer();
            Draw_FlushFramebuffer();
            Draw_Unlock();

            switch(currentMenu->items[selectedItem].action_type)
            {
                case METHOD:
                    if(currentMenu->items[selectedItem].method != NULL)
                        currentMenu->items[selectedItem].method();
                    break;
                case MENU:
                    previousSelectedItems[nbPreviousMenus] = selectedItem;
                    previousMenus[nbPreviousMenus++] = currentMenu;
                    currentMenu = currentMenu->items[selectedItem].menu;
                    selectedItem = 0;
                    break;
            }

            Draw_Lock();
            Draw_ClearFramebuffer();
            Draw_FlushFramebuffer();
            Draw_Unlock();
        }
        else if(pressed & BUTTON_B)
        {
            Draw_Lock();
            Draw_ClearFramebuffer();
            Draw_FlushFramebuffer();
            Draw_Unlock();

            if(nbPreviousMenus > 0)
            {
                currentMenu = previousMenus[--nbPreviousMenus];
                selectedItem = previousSelectedItems[nbPreviousMenus];
            }
            else
                break;
        }
        else if(pressed & BUTTON_DOWN)
        {
            if(++selectedItem >= currentMenu->nbItems)
                selectedItem = 0;
        }
        else if(pressed & BUTTON_UP)
        {
            if(selectedItem-- <= 0)
                selectedItem = currentMenu->nbItems - 1;
        }
        else if(pressed & (BUTTON_LEFT | BUTTON_RIGHT))
            selectedItem = menuPageMove(selectedItem, 0, currentMenu->nbItems - 1, MENU_MAX_ROWS, (pressed & BUTTON_RIGHT) != 0);

        Draw_Lock();
        menuDraw(currentMenu, selectedItem);
        Draw_Unlock();
    }
    while(!terminationRequest);
}

static const char *__press_b_to_close = TR("B: 閉じる", "B: close");

void    DispMessage(const char *title, const char *message)
{
    if(!menuEnter())
        return;

    Draw_Lock();
    Draw_ClearFramebuffer();
    Draw_FlushFramebuffer();

    Draw_DrawString(10, 10, COLOR_TITLE, title);

    Draw_DrawString(30, 30, COLOR_WHITE, message);
    Draw_DrawString(200, 220, COLOR_TITLE, __press_b_to_close);


    u32 keys = 0;

    do
    {
        keys = waitComboWithTimeout(1000);
    }while (!terminationRequest && !(keys & BUTTON_B));

    Draw_Unlock(); ///< Keep it locked until we exit the message
    menuLeave();
}

u32    DispErrMessage(const char *title, const char *message, const Result error)
{
    char buf[100];

    sprintf(buf, "Error code: 0x%08X", error);
    if(!menuEnter())
        return error;

    Draw_Lock();
    Draw_ClearFramebuffer();
    Draw_FlushFramebuffer();

    Draw_DrawString(10, 10, COLOR_TITLE, title);

    u32 posY = Draw_DrawString(30, 30, COLOR_WHITE, message);
    Draw_DrawString(30, posY + 20, COLOR_RED, buf);
    Draw_DrawString(200, 220, COLOR_TITLE, __press_b_to_close);

    u32 keys = 0;

    do
    {
        keys = waitComboWithTimeout(1000);
    }while (!terminationRequest && !(keys & BUTTON_B));

    Draw_Unlock();  ///< Keep it locked until we exit the message
    menuLeave();
    return error;
}

u32     DispWarningOnHome(void)
{
    if(!menuEnter())
        return 0;

    Draw_Lock();
    Draw_ClearFramebuffer();
    Draw_FlushFramebuffer();

    Draw_DrawString(10, 10, COLOR_TITLE, TR("警告", "Warning"));

    u32 posY = Draw_DrawString(30, 40, COLOR_WHITE, TR("メモリが足りないため、\nHOME ボタンを使えません。", "The HOME button is disabled\ndue to insufficient memory."));
    Draw_DrawString(30, posY + 20, COLOR_WHITE, TR("[十字キー上 + B] でソフトを終了します。", "Press [D-PAD Up + B] to exit the app."));
    Draw_DrawString(200, 220, COLOR_TITLE, __press_b_to_close);


    u32 keys = 0;

    do
    {
        keys = waitComboWithTimeout(1000);
    }while (!terminationRequest && !(keys & BUTTON_B));

    Draw_Unlock(); ///< Keep it locked until we exit the message
    menuLeave();

    return (keys & BUTTON_UP) > 0;
}


typedef char string[50];
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

void    DisplayPluginMenu(u32   *cmdbuf)
{
    u32             cursor = 0;
    u32             nbItems = cmdbuf[1];
    u8              *states = (u8 *)cmdbuf[3];
    char            buffer[60];
    const char      *title = (const char *)cmdbuf[5];
    const string    *items = (const string *)cmdbuf[7];
    const string    *hints = (const string *)cmdbuf[9];

    if(!menuEnter())
        return;
    Draw_Lock();

    do
    {
        // Draw the menu
        {
            // Clear screen
            Draw_ClearFramebuffer();
            Draw_FlushFramebuffer();

            // Draw title
            Draw_DrawString(10, 10, COLOR_TITLE, title);

            // Draw items
            u32 i = MAX(0, (int)cursor - 7);
            u32 end = MIN(nbItems, i + 16);
            u32 posY = 30;

            for (; i < end; ++i, posY += 10)
            {
                sprintf(buffer, "[ ] %s", items[i]);
                Draw_DrawString(30, posY, COLOR_WHITE, buffer);

                if (i == cursor) Draw_DrawCharacter(10, posY, COLOR_TITLE, '>');
                if (states[i]) Draw_DrawCharacter(36, posY, COLOR_LIME, 'x');
            }

            // Draw hint

            
            if (hints[cursor][0])
                Draw_DrawString(10, 200, COLOR_TITLE, hints[cursor]);
        }

        // Wait for input
        u32 pressed = waitInput();

        if (pressed & BUTTON_A)
            states[cursor] = !states[cursor];

        if (pressed & BUTTON_B)
            break;

        if (pressed & BUTTON_DOWN)
            if (++cursor >= nbItems)
                cursor = 0;

        if (pressed & BUTTON_UP)
            if (--cursor >= nbItems)
                cursor = nbItems - 1;

    } while (true);

    Draw_Unlock();
    menuLeave();
}

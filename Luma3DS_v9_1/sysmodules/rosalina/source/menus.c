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
#include <3ds/os.h>
#include "menus.h"
#include "menus/lang.h"
#include "menu.h"
#include "draw.h"
#include "menus/process_list.h"
#include "menus/process_patches.h"
#include "menus/n3ds.h"
#include "menus/debugger.h"
#include "menus/miscellaneous.h"
#include "menus/sysconfig.h"
#include "menus/ftpd.h"
#include "menus/text_input.h"
#include "plgloader.h"
#include "ifile.h"
#include "memory.h"
#include "fmt.h"

Menu rosalinaMenu = {
    TR("Rosalina メニュー", "Rosalina menu"),
    .nbItems = 14,
    {
        { TR("New 3DS メニュー...", "New 3DS menu..."), MENU, .menu = &N3DSMenu },
        { TR("チート...", "Cheats..."), METHOD, .method = &RosalinaMenu_Cheats },
        { "", METHOD, .method = PluginLoader__MenuCallback},
        { TR("プロセス一覧", "Process list"), METHOD, .method = &RosalinaMenu_ProcessList },
        { TR("スクリーンショットを撮る（遅い）", "Take screenshot (slow!)"), METHOD, .method = &RosalinaMenu_TakeScreenshot },
        { TR("デバッガーの設定...", "Debugger options..."), MENU, .menu = &debuggerMenu },
        { TR("システム設定...", "System configuration..."), MENU, .menu = &sysconfigMenu },
        { TR("その他の設定...", "Miscellaneous options..."), MENU, .menu = &miscellaneousMenu },
        { TR("ファイルブラウザー", "File browser"), METHOD, .method = &RosalinaMenu_FileBrowser },
        { TR("バージョン切り替え (v9.1/v" LUMA_NEW_VERSION ")", "Switch version (v9.1/v" LUMA_NEW_VERSION ")"), METHOD, .method = &RosalinaMenu_ChainloadVersionToggle },
        { TR("電源オフ", "Power off"), METHOD, .method = &RosalinaMenu_PowerOff },
        { TR("再起動", "Reboot"), METHOD, .method = &RosalinaMenu_Reboot },
        { TR("FTP サーバー", "FTP server"), METHOD, .method = &RosalinaMenu_Ftpd },
        { TR("クレジット", "Credits"), METHOD, .method = &RosalinaMenu_ShowCredits },
    }
};

void RosalinaMenu_ShowCredits(void)
{
    Draw_Lock();
    Draw_ClearFramebuffer();
    Draw_FlushFramebuffer();
    Draw_Unlock();

    do
    {
        Draw_Lock();
        Draw_DrawString(10, 10, COLOR_TITLE, TR("Rosalina -- Luma3DS のクレジット", "Rosalina -- Luma3DS credits"));

        u32 posY = Draw_DrawString(10, 30, COLOR_WHITE, "Luma3DS (c) 2016-2018 AuroraWright, TuxSH") + SPACING_Y;

        posY = Draw_DrawString(10, posY + SPACING_Y, COLOR_WHITE, TR("3DSX の読み込み: fincs", "3DSX loading code by fincs"));
        posY = Draw_DrawString(10, posY + SPACING_Y, COLOR_WHITE, TR("通信と GDB の基本機能: Stary", "Networking code & basic GDB functionality by Stary"));
        posY = Draw_DrawString(10, posY + SPACING_Y, COLOR_WHITE, TR("InputRedirection: Stary（試作: ShinyQuagsire）", "InputRedirection by Stary (PoC by ShinyQuagsire)"));
        posY = Draw_DrawString(10, posY + SPACING_Y, COLOR_WHITE, TR("この boot.firm は ELVES と Claude が改変しました", "This boot.firm was modified by ELVES and Claude"));
        posY = Draw_DrawString(10, posY + SPACING_Y, COLOR_WHITE, TR("デバッガー: Yunic0n(kgmnln)", "Debugger: Yunic0n(kgmnln)"));

        posY += 2 * SPACING_Y;

        Draw_DrawString(10, posY, COLOR_WHITE,
            TR(
                "スペシャルサンクス:\n"
                "  Bond697, WinterMute, yifanlu,\n"
                "  Luma3DS の貢献者, ctrulib の貢献者,\n"
                "  Tekito_256,\n"
                "  ほかの皆さん",
                "Special thanks to:\n"
                "  Bond697, WinterMute, yifanlu,\n"
                "  Luma3DS contributors, ctrulib contributors,\n"
                "  Tekito_256,\n"
                "  and others"
            ));

        Draw_FlushFramebuffer();
        Draw_Unlock();
    }
    while(!(waitInput() & BUTTON_B) && !terminationRequest);
}

static bool firmFileValid(const char *path)
{
    IFile f;
    u64 size = 0;

    if(R_FAILED(IFile_Open(&f, ARCHIVE_SDMC, fsMakePath(PATH_EMPTY, ""),
                           fsMakePath(PATH_ASCII, path), FS_OPEN_READ)))
        return false;

    IFile_GetSize(&f, &size);

    
    u64 total = 0;
    char magic[4] = {0};
    bool ok = size > 0x200
              && R_SUCCEEDED(IFile_Read(&f, &total, magic, sizeof(magic)))
              && total == sizeof(magic)
              && memcmp(magic, "FIRM", 4) == 0;

    IFile_Close(&f);
    return ok;
}

bool chainloadVersionFirmExists(const char *id)
{
    char path[40];

    sprintf(path, "/3ds/luma/%s/boot.firm", id);
    return firmFileValid(path);
}

void RosalinaMenu_Reboot(void)
{
    Draw_Lock();
    Draw_ClearFramebuffer();
    Draw_FlushFramebuffer();
    Draw_Unlock();

    do
    {
        Draw_Lock();
        Draw_DrawString(10, 10, COLOR_TITLE, TR("Rosalina メニュー", "Rosalina menu"));
        Draw_DrawString(10, 30, COLOR_WHITE, TR("A: 再起動   B: 戻る", "A: reboot   B: back"));
        Draw_FlushFramebuffer();
        Draw_Unlock();

        u32 pressed = waitInputWithTimeout(1000);

        if(pressed & BUTTON_A)
        {
            menuLeave();
            APT_HardwareResetAsync();
            return;
        } else if(pressed & BUTTON_B)
            return;
    }
    while(!terminationRequest);
}

void RosalinaMenu_PowerOff(void) // Soft shutdown.
{
    Draw_Lock();
    Draw_ClearFramebuffer();
    Draw_FlushFramebuffer();
    Draw_Unlock();

    do
    {
        Draw_Lock();
        Draw_DrawString(10, 10, COLOR_TITLE, TR("Rosalina メニュー", "Rosalina menu"));
        Draw_DrawString(10, 30, COLOR_WHITE, TR("A: 電源オフ   B: 戻る", "A: power off   B: back"));
        Draw_FlushFramebuffer();
        Draw_Unlock();

        u32 pressed = waitInputWithTimeout(1000);

        if(pressed & BUTTON_A)
        {
            menuLeave();
            srvPublishToSubscriber(0x203, 0);
            return;
        }
        else if(pressed & BUTTON_B)
            return;
    }
    while(!terminationRequest);
}

extern u8 *framebufferCache;
void RosalinaMenu_TakeScreenshot(void)
{
#define TRY(expr) if(R_FAILED(res = (expr))) goto end;

    u64 total;
    IFile file;
    Result res;

    char filename[64];

    FS_Archive archive;
    FS_ArchiveID archiveId;
    s64 out;
    bool isSdMode;

    if(R_FAILED(svcGetSystemInfo(&out, 0x10000, 0x203))) svcBreak(USERBREAK_ASSERT);
    isSdMode = (bool)out;

    archiveId = isSdMode ? ARCHIVE_SDMC : ARCHIVE_NAND_RW;
    Draw_Lock();
    Draw_RestoreFramebuffer();

    svcFlushEntireDataCache();

    res = FSUSER_OpenArchive(&archive, archiveId, fsMakePath(PATH_EMPTY, ""));
    if(R_SUCCEEDED(res))
    {
        res = FSUSER_CreateDirectory(archive, fsMakePath(PATH_ASCII, "/luma/screenshots"), 0);
        if((u32)res == 0xC82044BE) // directory already exists
            res = 0;
        FSUSER_CloseArchive(archive);
    }

    u32 seconds, minutes, hours, days, year, month;
    u64 milliseconds = osGetTime();
    seconds = milliseconds/1000;
    milliseconds %= 1000;
    minutes = seconds / 60;
    seconds %= 60;
    hours = minutes / 60;
    minutes %= 60;
    days = hours / 24;
    hours %= 24;

    year = 1900; // osGetTime starts in 1900

    while(true)
    {
        bool leapYear = (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0));
        u16 daysInYear = leapYear ? 366 : 365;
        if(days >= daysInYear)
        {
            days -= daysInYear;
            ++year;
        }
        else
        {
            static const u8 daysInMonth[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
            for(month = 0; month < 12; ++month)
            {
                u8 dim = daysInMonth[month];

                if (month == 1 && leapYear)
                    ++dim;

                if (days >= dim)
                    days -= dim;
                else
                    break;
            }
            break;
        }
    }
    days++;
    month++;

    sprintf(filename, "/luma/screenshots/%04u-%02u-%02u_%02u-%02u-%02u.%03u_top.bmp", year, month, days, hours, minutes, seconds, milliseconds);
    TRY(IFile_Open(&file, archiveId, fsMakePath(PATH_EMPTY, ""), fsMakePath(PATH_ASCII, filename), FS_OPEN_CREATE | FS_OPEN_WRITE));
    Draw_CreateBitmapHeader(framebufferCache, 400, 240);

    for(u32 y = 0; y < 120; y++)
        Draw_ConvertFrameBufferLine(framebufferCache + 54 + 3 * 400 * y, true, true, y);

    TRY(IFile_Write(&file, &total, framebufferCache, 54 + 3 * 400 * 120, 0));

    for(u32 y = 120; y < 240; y++)
        Draw_ConvertFrameBufferLine(framebufferCache + 3 * 400 * (y - 120), true, true, y);

    TRY(IFile_Write(&file, &total, framebufferCache, 3 * 400 * 120, 0));
    TRY(IFile_Close(&file));

    sprintf(filename, "/luma/screenshots/%04u-%02u-%02u_%02u-%02u-%02u.%03u_bot.bmp", year, month, days, hours, minutes, seconds, milliseconds);
    TRY(IFile_Open(&file, archiveId, fsMakePath(PATH_EMPTY, ""), fsMakePath(PATH_ASCII, filename), FS_OPEN_CREATE | FS_OPEN_WRITE));
    Draw_CreateBitmapHeader(framebufferCache, 320, 240);

    for(u32 y = 0; y < 120; y++)
        Draw_ConvertFrameBufferLine(framebufferCache + 54 + 3 * 320 * y, false, true, y);

    TRY(IFile_Write(&file, &total, framebufferCache, 54 + 3 * 320 * 120, 0));

    for(u32 y = 120; y < 240; y++)
        Draw_ConvertFrameBufferLine(framebufferCache + 3 * 320 * (y - 120), false, true, y);

    TRY(IFile_Write(&file, &total, framebufferCache, 3 * 320 * 120, 0));
    TRY(IFile_Close(&file));

    if((GPU_FB_TOP_FMT & 0x20) && (Draw_GetCurrentFramebufferAddress(true, true) != Draw_GetCurrentFramebufferAddress(true, false)))
    {
        sprintf(filename, "/luma/screenshots/%04u-%02u-%02u_%02u-%02u-%02u.%03u_top_right.bmp", year, month, days, hours, minutes, seconds, milliseconds);
        TRY(IFile_Open(&file, archiveId, fsMakePath(PATH_EMPTY, ""), fsMakePath(PATH_ASCII, filename), FS_OPEN_CREATE | FS_OPEN_WRITE));
        Draw_CreateBitmapHeader(framebufferCache, 400, 240);

        for(u32 y = 0; y < 120; y++)
            Draw_ConvertFrameBufferLine(framebufferCache + 54 + 3 * 400 * y, true, false, y);

        TRY(IFile_Write(&file, &total, framebufferCache, 54 + 3 * 400 * 120, 0));

        for(u32 y = 120; y < 240; y++)
            Draw_ConvertFrameBufferLine(framebufferCache + 3 * 400 * (y - 120), true, false, y);

        TRY(IFile_Write(&file, &total, framebufferCache, 3 * 400 * 120, 0));
        TRY(IFile_Close(&file));
    }

end:
    IFile_Close(&file);
    svcFlushEntireDataCache();
    Draw_SetupFramebuffer();
    Draw_ClearFramebuffer();
    Draw_FlushFramebuffer();
    Draw_Unlock();

    do
    {
        Draw_Lock();
        Draw_DrawString(10, 10, COLOR_TITLE, TR("スクリーンショット", "Screenshot"));
        if(R_FAILED(res))
            Draw_DrawFormattedString(10, 30, COLOR_WHITE, "Operation failed (0x%08x).", (u32)res);
        else
            Draw_DrawString(10, 30, COLOR_WHITE, TR("完了しました。", "Operation succeeded."));

        Draw_FlushFramebuffer();
        Draw_Unlock();
    }
    while(!(waitInput() & BUTTON_B) && !terminationRequest);

#undef TRY
}

/*
*   This file is part of Luma3DS
*   Copyright (C) 2016-2020 Aurora Wright, TuxSH
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
#include "menus/n3ds.h"
#include "menus/debugger.h"
#include "menus/miscellaneous.h"
#include "menus/sysconfig.h"
#include "menus/screen_filters.h"
#include "menus/ftpd.h"
#include "menus/text_input.h"
#include "plugin.h"
#include "ifile.h"
#include "memory.h"
#include "fmt.h"
#include "process_patches.h"
#include "luma_config.h"

Menu rosalinaMenu = {
    TR("Rosalina メニュー", "Rosalina menu"),
    {
        { TR("スクリーンショットを撮る", "Take screenshot"), METHOD, .method = &RosalinaMenu_TakeScreenshot },
        { TR("画面フィルター...", "Screen filters..."), MENU, .menu = &screenFiltersMenu },
        { TR("チート...", "Cheats..."), METHOD, .method = &RosalinaMenu_Cheats },
        { "", METHOD, .method = PluginLoader__MenuCallback},
        { TR("New 3DS メニュー...", "New 3DS menu..."), MENU, .menu = &N3DSMenu, .visibility = &menuCheckN3ds },
        { TR("プロセス一覧", "Process list"), METHOD, .method = &RosalinaMenu_ProcessList },
        { TR("デバッガーの設定...", "Debugger options..."), MENU, .menu = &debuggerMenu },
        { TR("システム設定...", "System configuration..."), MENU, .menu = &sysconfigMenu },
        { TR("その他の設定...", "Miscellaneous options..."), MENU, .menu = &miscellaneousMenu },
        { TR("ファイルブラウザー", "File browser"), METHOD, .method = &RosalinaMenu_FileBrowser },
        { TR("設定を保存", "Save settings"), METHOD, .method = &RosalinaMenu_SaveSettings },
        { TR("HOME メニューに戻る", "Return to HOME Menu"), METHOD, .method = &RosalinaMenu_ReturnToHomeMenu },
        { TR("バージョン切り替え (v9.1/v" LUMA_NEW_VERSION ")", "Switch version (v9.1/v" LUMA_NEW_VERSION ")"), METHOD, .method = &RosalinaMenu_ChainloadVersionToggle },
        { TR("電源オフ／再起動", "Power off / reboot"), METHOD, .method = &RosalinaMenu_PowerOffOrReboot },
        { TR("FTP サーバー", "FTP server"), METHOD, .method = &RosalinaMenu_Ftpd },
        { TR("システム情報", "System info"), METHOD, .method = &RosalinaMenu_ShowSystemInfo },
        { TR("クレジット", "Credits"), METHOD, .method = &RosalinaMenu_ShowCredits },
        { TR("デバッグ情報", "Debug info"), METHOD, .method = &RosalinaMenu_ShowDebugInfo, .visibility = &rosalinaMenuShouldShowDebugInfo },
        {},
    }
};

bool rosalinaMenuShouldShowDebugInfo(void)
{
    // Don't show on release builds

    s64 out;
    svcGetSystemInfo(&out, 0x10000, 0x200);
    return out == 0;
}

void RosalinaMenu_SaveSettings(void)
{
    Result res = LumaConfig_SaveSettings();
    Draw_Lock();
    Draw_ClearFramebuffer();
    Draw_FlushFramebuffer();
    Draw_Unlock();

    do
    {
        Draw_Lock();
        Draw_DrawString(10, 10, COLOR_TITLE, TR("設定を保存", "Save settings"));
        if(R_SUCCEEDED(res))
            Draw_DrawString(10, 30, COLOR_WHITE, TR("完了しました。", "Operation succeeded."));
        else
            Draw_DrawFormattedString(10, 30, COLOR_WHITE, Lang(STR_OPERATION_FAILED_0X), res);
        Draw_FlushFramebuffer();
        Draw_Unlock();
    }
    while(!(waitInput() & KEY_B) && !menuShouldExit);
}

void RosalinaMenu_PowerOffOrReboot(void)
{
    Draw_Lock();
    Draw_ClearFramebuffer();
    Draw_FlushFramebuffer();
    Draw_Unlock();

    do
    {
        Draw_Lock();
        Draw_DrawString(10, 10, COLOR_TITLE, Lang(STR_POWER_OFF_REBOOT));
        Draw_DrawString(10, 30, COLOR_WHITE, Lang(STR_PRESS_A_TO_POWER_OFF_PRESS_Y_TO_REBOOT_P));
        Draw_FlushFramebuffer();
        Draw_Unlock();

        u32 pressed = waitInputWithTimeout(1000);

        if(pressed & KEY_Y)
        {
            menuLeave();
            APT_HardwareResetAsync();
            return;
        }
        else if(pressed & KEY_A)
        {
            // Soft shutdown
            menuLeave();
            srvPublishToSubscriber(0x203, 0);
            return;
        }
        else if(pressed & KEY_B)
            return;
    }
    while(!menuShouldExit);
}

bool chainloadVersionFirmExists(const char *id)
{
    IFile f;
    u64 size = 0;
    char path[40];

    sprintf(path, "/3ds/luma/%s/boot.firm", id);

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

void RosalinaMenu_ShowSystemInfo(void)
{
    u32 kver = osGetKernelVersion();

    do
    {
        Draw_Lock();
        Draw_DrawString(10, 10, COLOR_TITLE, Lang(STR_ROSALINA_SYSTEM_INFO));

        u32 posY = 30;

        if (areScreenTypesInitialized)
        {
            posY = Draw_DrawFormattedString(10, posY, COLOR_WHITE, Lang(STR_TOP_SCREEN_TYPE), topScreenType);
            posY = Draw_DrawFormattedString(10, posY, COLOR_WHITE, Lang(STR_BOTTOM_SCREEN_TYPE), bottomScreenType);
        }

        posY = Draw_DrawFormattedString(10, posY, COLOR_WHITE, Lang(STR_KERNEL_VERSION), GET_VERSION_MAJOR(kver), GET_VERSION_MINOR(kver), GET_VERSION_REVISION(kver));
        if (mcuFwVersion != 0 && mcuInfoTableRead)
        {
            posY = Draw_DrawFormattedString(10, posY, COLOR_WHITE, Lang(STR_MCU_FW_VERSION), GET_VERSION_MAJOR(mcuFwVersion), GET_VERSION_MINOR(mcuFwVersion));
            posY = Draw_DrawFormattedString(10, posY, COLOR_WHITE, Lang(STR_PMIC_VENDOR), mcuInfoTable[1]);
            posY = Draw_DrawFormattedString(10, posY, COLOR_WHITE, Lang(STR_BATTERY_VENDOR), mcuInfoTable[2]);
        }

        Draw_FlushFramebuffer();
        Draw_Unlock();
    }
    while(!(waitInput() & KEY_B) && !menuShouldExit);
}

void RosalinaMenu_ShowDebugInfo(void)
{
    Draw_Lock();
    Draw_ClearFramebuffer();
    Draw_FlushFramebuffer();
    Draw_Unlock();

    char memoryMap[512];
    formatMemoryMapOfProcess(memoryMap, 511, CUR_PROCESS_HANDLE);

    s64 kextAddrSize;
    svcGetSystemInfo(&kextAddrSize, 0x10000, 0x300);
    u32 kextPa = (u32)((u64)kextAddrSize >> 32);
    u32 kextSize = (u32)kextAddrSize;

    FS_SdMmcSpeedInfo speedInfo;

    do
    {
        Draw_Lock();
        Draw_DrawString(10, 10, COLOR_TITLE, Lang(STR_ROSALINA_DEBUG_INFO));

        u32 posY = 30;

        posY = Draw_DrawString(10, posY, COLOR_WHITE, memoryMap);
        posY = Draw_DrawFormattedString(10, posY, COLOR_WHITE, TR("カーネル拡張の PA: %08lx - %08lx\n\n", "Kernel ext PA: %08lx - %08lx\n\n"), kextPa, kextPa + kextSize);
        if (R_SUCCEEDED(FSUSER_GetSdmcSpeedInfo(&speedInfo)))
        {
            u32 clkDiv = 1 << (1 + (speedInfo.sdClkCtrl & 0xFF));
            posY = Draw_DrawFormattedString(
                10, posY, COLOR_WHITE, TR("SDMC の速度: HS=%d %lukHz\n", "SDMC speed: HS=%d %lukHz\n"),
                (int)speedInfo.highSpeedModeEnabled, SYSCLOCK_SDMMC / (1000 * clkDiv)
            );
        }
        if (R_SUCCEEDED(FSUSER_GetNandSpeedInfo(&speedInfo)))
        {
            u32 clkDiv = 1 << (1 + (speedInfo.sdClkCtrl & 0xFF));
            posY = Draw_DrawFormattedString(
                10, posY, COLOR_WHITE, TR("NAND の速度: HS=%d %lukHz\n", "NAND speed: HS=%d %lukHz\n"),
                (int)speedInfo.highSpeedModeEnabled, SYSCLOCK_SDMMC / (1000 * clkDiv)
            );
        }
        {
            posY = Draw_DrawFormattedString(
                10, posY, COLOR_WHITE, "APPMEMTYPE: %lu\n",
                OS_KernelConfig->app_memtype
            );
        }
        Draw_FlushFramebuffer();
        Draw_Unlock();
    }
    while(!(waitInput() & KEY_B) && !menuShouldExit);
}

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

        u32 posY = Draw_DrawString(10, 30, COLOR_WHITE, "Luma3DS (c) 2016-2026 LumaTeam") + SPACING_Y;

        posY = Draw_DrawString(10, posY + SPACING_Y, COLOR_WHITE, TR("3DSX の読み込み: fincs", "3DSX loading code by fincs"));
        posY = Draw_DrawString(10, posY + SPACING_Y, COLOR_WHITE, TR("通信と GDB の基本機能: Stary", "Networking code & basic GDB functionality by Stary"));
        posY = Draw_DrawString(10, posY + SPACING_Y, COLOR_WHITE, TR("InputRedirection: Stary（試作: ShinyQuagsire）", "InputRedirection by Stary (PoC by ShinyQuagsire)"));
        posY = Draw_DrawString(10, posY + SPACING_Y, COLOR_WHITE, TR("この boot.firm は ELVES と Claude が改変しました", "This boot.firm was modified by ELVES and Claude"));
        posY = Draw_DrawString(10, posY + SPACING_Y, COLOR_WHITE, TR("デバッガー: Yunic0n(kgmnln)", "Debugger: Yunic0n(kgmnln)"));

        posY += 2 * SPACING_Y;

        Draw_DrawString(10, posY, COLOR_WHITE,
            TR(
                "スペシャルサンクス:\n"
                "  fincs, WinterMute, mtheall, piepie62,\n"
                "  Luma3DS の貢献者, libctru の貢献者,\n"
                "  Tekito_256,\n"
                "  ほかの皆さん",
                "Special thanks to:\n"
                "  fincs, WinterMute, mtheall, piepie62,\n"
                "  Luma3DS contributors, libctru contributors,\n"
                "  Tekito_256,\n"
                "  and others"
            ));

        Draw_FlushFramebuffer();
        Draw_Unlock();
    }
    while(!(waitInput() & KEY_B) && !menuShouldExit);
}

#define TRY(expr) if(R_FAILED(res = (expr))) goto end;

static s64 timeSpentConvertingScreenshot = 0;
static s64 timeSpentWritingScreenshot = 0;

static Result RosalinaMenu_WriteScreenshot(IFile *file, u32 width, bool top, bool left)
{
    u64 total;
    Result res = 0;
    u32 lineSize = 3 * width;

    // When dealing with 800px mode (800x240 with half-width pixels), duplicate each line
    // to restore aspect ratio and obtain faithful 800x480 screenshots
    u32 scaleFactorY = width > 400 ? 2 : 1;
    u32 numLinesScaled = 240 * scaleFactorY;
    u32 remaining = lineSize * numLinesScaled;

    TRY(Draw_AllocateFramebufferCacheForScreenshot(remaining));

    u8 *framebufferCache = (u8 *)Draw_GetFramebufferCache();
    u8 *framebufferCacheEnd = framebufferCache + Draw_GetFramebufferCacheSize();

    u8 *buf = framebufferCache;
    Draw_CreateBitmapHeader(framebufferCache, width, numLinesScaled);
    const u32 headerSize = 0x40;
    buf += headerSize;

    u32 y = 0;
    // Our buffer might be smaller than the size of the screenshot...
    while (remaining != 0)
    {
        s64 t0 = svcGetSystemTick();
        u32 available = (u32)(framebufferCacheEnd - buf);
        u32 size = available < remaining ? available : remaining;
        u32 nlines = size / (lineSize * scaleFactorY);
        Draw_ConvertFrameBufferLines(buf, width, y, nlines, scaleFactorY, top, left);

        s64 t1 = svcGetSystemTick();
        timeSpentConvertingScreenshot += t1 - t0;
        TRY(IFile_Write(file, &total, framebufferCache, (y == 0 ? headerSize : 0) + lineSize * nlines * scaleFactorY, 0)); // don't forget to write the header
        timeSpentWritingScreenshot += svcGetSystemTick() - t1;

        y += nlines;
        remaining -= lineSize * nlines * scaleFactorY;
        buf = framebufferCache;
    }
    end:

    Draw_FreeFramebufferCache();
    return res;
}

void RosalinaMenu_ReturnToHomeMenu(void)
{
    Draw_Lock();
    Draw_ClearFramebuffer();
    Draw_FlushFramebuffer();
    Draw_Unlock();

    do
    {
        Draw_Lock();
        Draw_DrawString(10, 10, COLOR_TITLE, Lang(STR_RETURN_TO_HOME_MENU));
        Draw_DrawString(10, 30, COLOR_WHITE, Lang(STR_PRESS_A_TO_CONFIRM_PRESS_B_TO_GO_BACK));
        Draw_FlushFramebuffer();
        Draw_Unlock();

        u32 pressed = waitInputWithTimeout(1000);

        if(pressed & KEY_A)
        {
            srvPublishToSubscriber(0x204, 0);
            menuRequestClose();
            return;
        }
        else if(pressed & KEY_B)
            return;
    }
    while(!menuShouldExit);
}

void RosalinaMenu_TakeScreenshot(void)
{
    IFile file = {0};
    Result res = 0;

    char filename[64];
    char dateTimeStr[32];

    FS_Archive archive;
    FS_ArchiveID archiveId;
    s64 out;
    bool isSdMode;

    timeSpentConvertingScreenshot = 0;
    timeSpentWritingScreenshot = 0;

    if(R_FAILED(svcGetSystemInfo(&out, 0x10000, 0x203))) svcBreak(USERBREAK_ASSERT);
    isSdMode = (bool)out;

    archiveId = isSdMode ? ARCHIVE_SDMC : ARCHIVE_NAND_RW;
    Draw_Lock();
    Draw_RestoreFramebuffer();
    Draw_FreeFramebufferCache();

    svcFlushEntireDataCache();

    bool is3d;
    u32 topWidth, bottomWidth; // actually Y-dim

    Draw_GetCurrentScreenInfo(&bottomWidth, &is3d, false);
    Draw_GetCurrentScreenInfo(&topWidth, &is3d, true);

    res = FSUSER_OpenArchive(&archive, archiveId, fsMakePath(PATH_EMPTY, ""));
    if(R_SUCCEEDED(res))
    {
        res = FSUSER_CreateDirectory(archive, fsMakePath(PATH_ASCII, "/luma/screenshots"), 0);
        if((u32)res == 0xC82044BE) // directory already exists
            res = 0;
        FSUSER_CloseArchive(archive);
    }
    else
    {
        archive = 0;
        goto end;
    }

    dateTimeToString(dateTimeStr, osGetTime(), true);

    sprintf(filename, "/luma/screenshots/%s_top.bmp", dateTimeStr);
    TRY(IFile_Open(&file, archiveId, fsMakePath(PATH_EMPTY, ""), fsMakePath(PATH_ASCII, filename), FS_OPEN_CREATE | FS_OPEN_WRITE));
    TRY(RosalinaMenu_WriteScreenshot(&file, topWidth, true, true));
    TRY(IFile_Close(&file));

    sprintf(filename, "/luma/screenshots/%s_bot.bmp", dateTimeStr);
    TRY(IFile_Open(&file, archiveId, fsMakePath(PATH_EMPTY, ""), fsMakePath(PATH_ASCII, filename), FS_OPEN_CREATE | FS_OPEN_WRITE));
    TRY(RosalinaMenu_WriteScreenshot(&file, bottomWidth, false, true));
    TRY(IFile_Close(&file));

    if(is3d && (Draw_GetCurrentFramebufferAddress(true, true) != Draw_GetCurrentFramebufferAddress(true, false)))
    {
        sprintf(filename, "/luma/screenshots/%s_top_right.bmp", dateTimeStr);
        TRY(IFile_Open(&file, archiveId, fsMakePath(PATH_EMPTY, ""), fsMakePath(PATH_ASCII, filename), FS_OPEN_CREATE | FS_OPEN_WRITE));
        TRY(RosalinaMenu_WriteScreenshot(&file, topWidth, true, false));
        TRY(IFile_Close(&file));
    }

end:
    IFile_Close(&file);

    if (archive != 0)
        FSUSER_CloseArchive(archive);

    if (R_FAILED(Draw_AllocateFramebufferCache(FB_BOTTOM_SIZE)))
        __builtin_trap(); // We're f***ed if this happens

    svcFlushEntireDataCache();
    Draw_SetupFramebuffer();
    Draw_Unlock();

    do
    {
        Draw_Lock();
        Draw_DrawString(10, 10, COLOR_TITLE, TR("スクリーンショット", "Screenshot"));
        if(R_FAILED(res))
            Draw_DrawFormattedString(10, 30, COLOR_WHITE, Lang(STR_OPERATION_FAILED_0X), (u32)res);
        else
        {
            u32 t1 = (u32)(1000 * timeSpentConvertingScreenshot / SYSCLOCK_ARM11);
            u32 t2 = (u32)(1000 * timeSpentWritingScreenshot / SYSCLOCK_ARM11);
            u32 posY = 30;
            posY = Draw_DrawString(10, posY, COLOR_WHITE, TR("完了しました。\n\n", "Operation succeeded.\n\n"));
            posY = Draw_DrawFormattedString(10, posY, COLOR_WHITE, Lang(STR_TIME_SPENT_CONVERTING_MS), t1);
            posY = Draw_DrawFormattedString(10, posY, COLOR_WHITE, Lang(STR_TIME_SPENT_WRITING_FILES_MS), t2);
        }

        Draw_FlushFramebuffer();
        Draw_Unlock();
    }
    while(!(waitInput() & KEY_B) && !menuShouldExit);
}

static Result menuWriteSelfScreenshot(IFile *file)
{
    u64 total;
    Result res = 0;

    u32 width = 320;
    u32 lineSize = 3 * width;

    u32 scaleFactorY = 1;
    u32 numLinesScaled = 240 * scaleFactorY;

    u32 addr = 0x0D800000; // keep this in check
    u32 tmp;

    u32 size = ((54 + lineSize * numLinesScaled * scaleFactorY) + 0xFFF) >> 12 << 12; // round-up
    u8 *buffer = NULL;

    TRY(svcControlMemoryEx(&tmp, addr, 0, size, MEMOP_ALLOC | MEMOP_REGION_SYSTEM, MEMPERM_READWRITE, true));
    buffer = (u8 *)addr;

    Draw_CreateBitmapHeader(buffer, width, numLinesScaled);

    Draw_ConvertFrameBufferLines(buffer + 54, width, 0, numLinesScaled, scaleFactorY, false, false);
    TRY(IFile_Write(file, &total, buffer, 54 + lineSize * numLinesScaled * scaleFactorY, 0)); // don't forget to write the header

end:
    if (buffer)
        svcControlMemoryEx(&tmp, addr, 0, size, MEMOP_FREE, MEMPERM_DONTCARE, false);

    return res;
}

void menuTakeSelfScreenshot(void)
{
    // Optimized for N3DS. May fail due to OOM.

    IFile file = {0};
    Result res = 0;

    char filename[100];
    char dateTimeStr[64];

    FS_Archive archive;
    FS_ArchiveID archiveId;
    s64 out;
    bool isSdMode;

    timeSpentConvertingScreenshot = 0;
    timeSpentWritingScreenshot = 0;

    if(R_FAILED(svcGetSystemInfo(&out, 0x10000, 0x203))) svcBreak(USERBREAK_ASSERT);
    isSdMode = (bool)out;

    archiveId = isSdMode ? ARCHIVE_SDMC : ARCHIVE_NAND_RW;
    Draw_Lock();
    svcFlushEntireDataCache();

    res = FSUSER_OpenArchive(&archive, archiveId, fsMakePath(PATH_EMPTY, ""));
    if(R_SUCCEEDED(res))
    {
        res = FSUSER_CreateDirectory(archive, fsMakePath(PATH_ASCII, "/luma/screenshots"), 0);
        if((u32)res == 0xC82044BE) // directory already exists
            res = 0;
        FSUSER_CloseArchive(archive);
    }
    else
    {
        archive = 0;
        goto end;
    }

    dateTimeToString(dateTimeStr, osGetTime(), true);

    sprintf(filename, "/luma/screenshots/rosalina_menu_%s.bmp", dateTimeStr);

    TRY(IFile_Open(&file, archiveId, fsMakePath(PATH_EMPTY, ""), fsMakePath(PATH_ASCII, filename), FS_OPEN_CREATE | FS_OPEN_WRITE));
    TRY(menuWriteSelfScreenshot(&file));

end:
    IFile_Close(&file);

    if (archive != 0)
        FSUSER_CloseArchive(archive);
}

#undef TRY

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
#include "menu.h"
#include "draw.h"
#include "menus/process_list.h"
#include "menus/n3ds.h"
#include "menus/debugger.h"
#include "menus/miscellaneous.h"
#include "menus/sysconfig.h"
#include "menus/screen_filters.h"
#include "plugin.h"
#include "ifile.h"
#include "memory.h"
#include "fmt.h"
#include "process_patches.h"
#include "luma_config.h"

Menu rosalinaMenu = {
    "Rosalina menu",
    {
        { "Take screenshot", METHOD, .method = &RosalinaMenu_TakeScreenshot },
        { "Screen filters...", MENU, .menu = &screenFiltersMenu },
        { "Cheats...", METHOD, .method = &RosalinaMenu_Cheats },
        { "", METHOD, .method = PluginLoader__MenuCallback},
        { "New 3DS menu...", MENU, .menu = &N3DSMenu, .visibility = &menuCheckN3ds },
        { "Process list", METHOD, .method = &RosalinaMenu_ProcessList },
        { "Debugger options...", MENU, .menu = &debuggerMenu },
        { "System configuration...", MENU, .menu = &sysconfigMenu },
        { "Miscellaneous options...", MENU, .menu = &miscellaneousMenu },
        { "Colors...", METHOD, .method = &RosalinaMenu_AccentColor },
        { "3gx / txt checker", METHOD, .method = &RosalinaMenu_3gxTxtChecker },
        { "Self test", METHOD, .method = &RosalinaMenu_SelfTest },
        { "Restore settings", METHOD, .method = &RosalinaMenu_RestoreSettings },
        { "File browser", METHOD, .method = &RosalinaMenu_FileBrowser },
        { "Save settings", METHOD, .method = &RosalinaMenu_SaveSettings },
        { "Return To HOME Menu", METHOD, .method = &RosalinaMenu_ReturnToHomeMenu },
        { "Switch version (v9.1/v13.4)", METHOD, .method = &RosalinaMenu_ChainloadVersionToggle },
        { "Power off / reboot", METHOD, .method = &RosalinaMenu_PowerOffOrReboot },
        { "System info", METHOD, .method = &RosalinaMenu_ShowSystemInfo },
        { "Credits", METHOD, .method = &RosalinaMenu_ShowCredits },
        { "Debug info", METHOD, .method = &RosalinaMenu_ShowDebugInfo, .visibility = &rosalinaMenuShouldShowDebugInfo },
        {},
    }
};

bool rosalinaMenuShouldShowDebugInfo(void)
{
    

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
        Draw_DrawString(10, 10, COLOR_TITLE, "Save settings");
        if(R_SUCCEEDED(res))
            Draw_DrawString(10, 30, COLOR_WHITE, "Operation succeeded.");
        else
            Draw_DrawFormattedString(10, 30, COLOR_WHITE, "Operation failed (0x%08lx).", res);
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
        Draw_DrawString(10, 10, COLOR_TITLE, "Power Off / Reboot");
        Draw_DrawString(10, 30, COLOR_WHITE, "Press A to power off.\nPress Y to reboot.\nPress B to go back.");
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
            
            menuLeave();
            srvPublishToSubscriber(0x203, 0);
            return;
        }
        else if(pressed & KEY_B)
            return;
    }
    while(!menuShouldExit);
}




#define MY_LUMA_VERSION FORK_LUMA_VERSION
#define OTHER_LUMA_VERSION FORK_OTHER_VERSION

#define CHAINLOAD_FLAG_PATH "/luma/chainload_ver.flag"


static bool chainloadVersionFlagSet(const char *version)
{
    IFile f;
    u64 total = 0;

    if(R_FAILED(IFile_Open(&f, ARCHIVE_SDMC, fsMakePath(PATH_EMPTY, ""),
                           fsMakePath(PATH_ASCII, CHAINLOAD_FLAG_PATH),
                           FS_OPEN_CREATE | FS_OPEN_WRITE)))
        return false;

    
    char buf[4];
    u32 len = strlen(version);

    memcpy(buf, version, len);
    memset(buf + len, '\n', sizeof(buf) - len);

    bool ok = R_SUCCEEDED(IFile_Write(&f, &total, buf, sizeof(buf), FS_WRITE_FLUSH));
    IFile_Close(&f);
    return ok;
}

static bool chainloadVersionFirmExists(const char *version)
{
    IFile f;
    u64 size = 0;
    char path[40];

    sprintf(path, "/3ds/luma/%s/boot.firm", version);

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


void RosalinaMenu_ChainloadVersionToggle(void)
{
    Draw_Lock();
    Draw_ClearFramebuffer();
    Draw_FlushFramebuffer();
    Draw_Unlock();

    const char *target = OTHER_LUMA_VERSION;

    do
    {
        Draw_Lock();
        Draw_DrawString(10, 10, COLOR_TITLE, "Version switch chainload");
        Draw_DrawFormattedString(10, 30, COLOR_WHITE, "Current v%s   ->   v%s",
            MY_LUMA_VERSION, target);
        Draw_DrawString(10, 30 + SPACING_Y, COLOR_WHITE, "A: launch   B: go back");
        Draw_FlushFramebuffer();
        Draw_Unlock();

        u32 pressed = waitInputWithTimeout(1000);

        if(pressed & KEY_A)
        {
            if(!chainloadVersionFirmExists(target))
            {
                LogLine("version switch: target boot.firm missing");
                Draw_Lock();
                Draw_ClearFramebuffer();
                Draw_FlushFramebuffer();
                Draw_Unlock();

                do
                {
                    Draw_Lock();
                    Draw_DrawString(10, 10, COLOR_TITLE, "Version switch chainload");
                    Draw_DrawFormattedString(10, 30, COLOR_RED, "boot.firm for v%s was not found.", target);
                    Draw_DrawString(10, 30 + 2 * SPACING_Y, COLOR_WHITE, "B: go back");
                    Draw_FlushFramebuffer();
                    Draw_Unlock();
                }
                while(!(waitInput() & KEY_B) && !menuShouldExit);

                return;
            }

            
            if(!chainloadVersionFlagSet(target))
            {
                LogLine("version switch: SD write failed");
                Draw_Lock();
                Draw_ClearFramebuffer();
                Draw_FlushFramebuffer();
                Draw_Unlock();

                do
                {
                    Draw_Lock();
                    Draw_DrawString(10, 10, COLOR_TITLE, "Version switch chainload");
                    Draw_DrawString(10, 30, COLOR_RED, "Failed to write to the SD card.");
                    Draw_DrawString(10, 30 + 2 * SPACING_Y, COLOR_WHITE, "B: go back");
                    Draw_FlushFramebuffer();
                    Draw_Unlock();
                }
                while(!(waitInput() & KEY_B) && !menuShouldExit);

                return;
            }

            menuLeave();
            APT_HardwareResetAsync();
            return;
        }
        else if(pressed & KEY_B)
            return;
    }
    while(!menuShouldExit);
}

void RosalinaMenu_ShowSystemInfo(void)
{
    u32 kver = osGetKernelVersion();

    do
    {
        Draw_Lock();
        Draw_DrawString(10, 10, COLOR_TITLE, "Rosalina -- System info");

        u32 posY = 30;

        if (areScreenTypesInitialized)
        {
            posY = Draw_DrawFormattedString(10, posY, COLOR_WHITE, "Top screen type:    %s\n", topScreenType);
            posY = Draw_DrawFormattedString(10, posY, COLOR_WHITE, "Bottom screen type: %s\n\n", bottomScreenType);
        }

        posY = Draw_DrawFormattedString(10, posY, COLOR_WHITE, "Kernel version:     %lu.%lu-%lu\n\n", GET_VERSION_MAJOR(kver), GET_VERSION_MINOR(kver), GET_VERSION_REVISION(kver));
        if (mcuFwVersion != 0 && mcuInfoTableRead)
        {
            posY = Draw_DrawFormattedString(10, posY, COLOR_WHITE, "MCU FW version:     %lu.%lu\n", GET_VERSION_MAJOR(mcuFwVersion), GET_VERSION_MINOR(mcuFwVersion));
            posY = Draw_DrawFormattedString(10, posY, COLOR_WHITE, "PMIC vendor:        %hhu\n", mcuInfoTable[1]);
            posY = Draw_DrawFormattedString(10, posY, COLOR_WHITE, "Battery vendor:     %hhu\n\n", mcuInfoTable[2]);
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
        Draw_DrawString(10, 10, COLOR_TITLE, "Rosalina -- Debug info");

        u32 posY = 30;

        posY = Draw_DrawString(10, posY, COLOR_WHITE, memoryMap);
        posY = Draw_DrawFormattedString(10, posY, COLOR_WHITE, "Kernel ext PA: %08lx - %08lx\n\n", kextPa, kextPa + kextSize);
        if (R_SUCCEEDED(FSUSER_GetSdmcSpeedInfo(&speedInfo)))
        {
            u32 clkDiv = 1 << (1 + (speedInfo.sdClkCtrl & 0xFF));
            posY = Draw_DrawFormattedString(
                10, posY, COLOR_WHITE, "SDMC speed: HS=%d %lukHz\n",
                (int)speedInfo.highSpeedModeEnabled, SYSCLOCK_SDMMC / (1000 * clkDiv)
            );
        }
        if (R_SUCCEEDED(FSUSER_GetNandSpeedInfo(&speedInfo)))
        {
            u32 clkDiv = 1 << (1 + (speedInfo.sdClkCtrl & 0xFF));
            posY = Draw_DrawFormattedString(
                10, posY, COLOR_WHITE, "NAND speed: HS=%d %lukHz\n",
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
        Draw_DrawString(10, 10, COLOR_TITLE, "Rosalina -- Luma3DS credits");

        u32 posY = Draw_DrawString(10, 30, COLOR_WHITE, "Luma3DS (c) 2016-2026 LumaTeam") + SPACING_Y;

        posY = Draw_DrawString(10, posY + SPACING_Y, COLOR_WHITE, "3DSX loading code by fincs");
        posY = Draw_DrawString(10, posY + SPACING_Y, COLOR_WHITE, "Networking code & basic GDB functionality by Stary");
        posY = Draw_DrawString(10, posY + SPACING_Y, COLOR_WHITE, "InputRedirection by Stary (PoC by ShinyQuagsire)");

        posY += 2 * SPACING_Y;

        Draw_DrawString(10, posY, COLOR_WHITE,
            (
                "Special thanks to:\n"
                "  fincs, WinterMute, mtheall, piepie62,\n"
                "  Luma3DS contributors, libctru contributors,\n"
                "  other people"
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
    
    while (remaining != 0)
    {
        s64 t0 = svcGetSystemTick();
        u32 available = (u32)(framebufferCacheEnd - buf);
        u32 size = available < remaining ? available : remaining;
        u32 nlines = size / (lineSize * scaleFactorY);
        Draw_ConvertFrameBufferLines(buf, width, y, nlines, scaleFactorY, top, left);

        s64 t1 = svcGetSystemTick();
        timeSpentConvertingScreenshot += t1 - t0;
        TRY(IFile_Write(file, &total, framebufferCache, (y == 0 ? headerSize : 0) + lineSize * nlines * scaleFactorY, 0)); 
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
        Draw_DrawString(10, 10, COLOR_TITLE, "Return to HOME Menu");
        Draw_DrawString(10, 30, COLOR_WHITE, "Press A to confirm.\nPress B to go back.");
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
    u32 topWidth, bottomWidth; 

    Draw_GetCurrentScreenInfo(&bottomWidth, &is3d, false);
    Draw_GetCurrentScreenInfo(&topWidth, &is3d, true);

    res = FSUSER_OpenArchive(&archive, archiveId, fsMakePath(PATH_EMPTY, ""));
    if(R_SUCCEEDED(res))
    {
        res = FSUSER_CreateDirectory(archive, fsMakePath(PATH_ASCII, "/luma/screenshots"), 0);
        if((u32)res == 0xC82044BE) 
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
        __builtin_trap(); 

    svcFlushEntireDataCache();
    Draw_SetupFramebuffer();
    Draw_Unlock();

    do
    {
        Draw_Lock();
        Draw_DrawString(10, 10, COLOR_TITLE, "Screenshot");
        if(R_FAILED(res))
            Draw_DrawFormattedString(10, 30, COLOR_WHITE, "Operation failed (0x%08lx).", (u32)res);
        else
        {
            u32 t1 = (u32)(1000 * timeSpentConvertingScreenshot / SYSCLOCK_ARM11);
            u32 t2 = (u32)(1000 * timeSpentWritingScreenshot / SYSCLOCK_ARM11);
            u32 posY = 30;
            posY = Draw_DrawString(10, posY, COLOR_WHITE, "Operation succeeded.\n\n");
            posY = Draw_DrawFormattedString(10, posY, COLOR_WHITE, "Time spent converting:    %5lums\n", t1);
            posY = Draw_DrawFormattedString(10, posY, COLOR_WHITE, "Time spent writing files: %5lums\n", t2);
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

    u32 addr = 0x0D800000; 
    u32 tmp;

    u32 size = ((54 + lineSize * numLinesScaled * scaleFactorY) + 0xFFF) >> 12 << 12; 
    u8 *buffer = NULL;

    TRY(svcControlMemoryEx(&tmp, addr, 0, size, MEMOP_ALLOC | MEMOP_REGION_SYSTEM, MEMPERM_READWRITE, true));
    buffer = (u8 *)addr;

    Draw_CreateBitmapHeader(buffer, width, numLinesScaled);

    Draw_ConvertFrameBufferLines(buffer + 54, width, 0, numLinesScaled, scaleFactorY, false, false);
    TRY(IFile_Write(file, &total, buffer, 54 + lineSize * numLinesScaled * scaleFactorY, 0)); 

end:
    if (buffer)
        svcControlMemoryEx(&tmp, addr, 0, size, MEMOP_FREE, MEMPERM_DONTCARE, false);

    return res;
}

void menuTakeSelfScreenshot(void)
{
    

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
        if((u32)res == 0xC82044BE) 
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

u32 g_accentColor = COLOR_TITLE;
u32 g_textColor   = COLOR_WHITE;
u32 g_cursorColor = COLOR_TITLE;
u16 g_menuBgColor = COLOR_BLACK;
bool g_safeMode = false;

static u32  g_colH[4] = {180, 0,   180, 0};
static u32  g_colS[4] = {255, 0,   255, 0};
static u32  g_colV[4] = {255, 255, 255, 0};
static bool g_colorsLoaded = false;

#define COLORS_PATH "/luma/colors.cfg"

static void Backup_Snapshot(void);

#define HUE_X 10
#define HUE_Y 35
#define HUE_W 300
#define HUE_H 16
#define SV_X  10
#define SV_Y  58
#define SV_W  200
#define SV_H  118
#define PRE_X 280
#define PRE_Y 58
#define PRE_W 30
#define PRE_H 13
#define PRE_N 9
#define PRV_X 10
#define PRV_Y 182
#define PRV_W 90
#define PRV_H 32

static const u16 g_presetHSV[PRE_N][3] =
{
    {180, 255, 255},
    {0,   255, 255},
    {30,  255, 255},
    {60,  255, 255},
    {120, 255, 255},
    {200, 255, 255},
    {240, 255, 255},
    {300, 255, 255},
    {0,   0,   255},
};

static u16 AccentHsv(u32 h, u32 s, u32 v)
{
    u32 r, g, b;

    h %= 360;

    if (s == 0)
        r = g = b = v;
    else
    {
        u32 region = h / 60;
        u32 rem = (h % 60) * 255 / 60;
        u32 p = v * (255 - s) / 255;
        u32 q = v * (255 - s * rem / 255) / 255;
        u32 t = v * (255 - s * (255 - rem) / 255) / 255;

        switch (region)
        {
            case 0:  r = v; g = t; b = p; break;
            case 1:  r = q; g = v; b = p; break;
            case 2:  r = p; g = v; b = t; break;
            case 3:  r = p; g = q; b = v; break;
            case 4:  r = t; g = p; b = v; break;
            default: r = v; g = p; b = q; break;
        }
    }

    return RGB565(r >> 3, g >> 2, b >> 3);
}

static void AccentPixel(u32 x, u32 y, u16 color)
{
    if (x < SCREEN_BOT_WIDTH && y < SCREEN_BOT_HEIGHT)
        ((volatile u16 *)FB_BOTTOM_VRAM_ADDR)[x * SCREEN_BOT_HEIGHT + (SCREEN_BOT_HEIGHT - 1 - y)] = color;
}

static void AccentFillRect(u32 x, u32 y, u32 w, u32 h, u16 color)
{
    for (u32 sy = y; sy < y + h; ++sy)
        for (u32 sx = x; sx < x + w; ++sx)
            AccentPixel(sx, sy, color);
}

static void AccentDrawBase(u32 hue, u32 sat, u32 val)
{
    for (u32 i = 0; i < HUE_W; ++i)
        AccentFillRect(HUE_X + i, HUE_Y, 1, HUE_H, AccentHsv(i * 360 / HUE_W, 255, 255));

    for (u32 dy = 0; dy < SV_H; ++dy)
    {
        u32 vv = 255 - dy * 255 / (SV_H - 1);
        for (u32 dx = 0; dx < SV_W; ++dx)
            AccentPixel(SV_X + dx, SV_Y + dy, AccentHsv(hue, dx * 255 / (SV_W - 1), vv));
    }

    for (u32 i = 0; i < PRE_N; ++i)
        AccentFillRect(PRE_X, PRE_Y + i * PRE_H, PRE_W, PRE_H - 1,
                       AccentHsv(g_presetHSV[i][0], g_presetHSV[i][1], g_presetHSV[i][2]));

    u32 hx = HUE_X + hue * HUE_W / 360;
    AccentFillRect(hx, HUE_Y - 2, 2, HUE_H + 4, COLOR_WHITE);

    u32 mx = SV_X + sat * (SV_W - 1) / 255;
    u32 my = SV_Y + (255 - val) * (SV_H - 1) / 255;
    AccentFillRect(mx >= 2 ? mx - 2 : 0, my >= 2 ? my - 2 : 0, 5, 5, COLOR_WHITE);
    AccentPixel(mx, my, COLOR_BLACK);

    AccentFillRect(PRV_X, PRV_Y, PRV_W, PRV_H, AccentHsv(hue, sat, val));
}

static void ColorsApply(void)
{
    g_accentColor = AccentHsv(g_colH[0], g_colS[0], g_colV[0]);
    g_textColor   = AccentHsv(g_colH[1], g_colS[1], g_colV[1]);
    g_cursorColor = AccentHsv(g_colH[2], g_colS[2], g_colV[2]);
    g_menuBgColor = (u16)AccentHsv(g_colH[3], g_colS[3], g_colV[3]);
}

void AccentColor_EnsureLoaded(void)
{
    if (g_colorsLoaded)
        return;
    g_colorsLoaded = true;

    if (g_safeMode)
        return;

    IFile f;
    if (R_SUCCEEDED(IFile_Open(&f, ARCHIVE_SDMC, fsMakePath(PATH_EMPTY, ""),
                               fsMakePath(PATH_ASCII, COLORS_PATH), FS_OPEN_READ)))
    {
        char buf[160];
        u64 total = 0;

        memset(buf, 0, sizeof(buf));
        if (R_SUCCEEDED(IFile_Read(&f, &total, buf, sizeof(buf) - 1)) && total > 0)
        {
            u32 nums[12];
            u32 cnt = 0;
            u32 i = 0;

            while (buf[i] && cnt < 12)
            {
                while (buf[i] == ' ' || buf[i] == '\r' || buf[i] == '\n')
                    ++i;
                if (buf[i] < '0' || buf[i] > '9')
                    break;

                u32 v = 0;
                while (buf[i] >= '0' && buf[i] <= '9')
                {
                    v = v * 10 + (u32)(buf[i] - '0');
                    ++i;
                }
                nums[cnt++] = v;
            }

            if (cnt >= 12)
            {
                for (u32 r = 0; r < 4; ++r)
                {
                    g_colH[r] = nums[r * 3 + 0] % 360;
                    g_colS[r] = nums[r * 3 + 1] > 255 ? 255 : nums[r * 3 + 1];
                    g_colV[r] = nums[r * 3 + 2] > 255 ? 255 : nums[r * 3 + 2];
                }
                ColorsApply();
            }
        }
        IFile_Close(&f);
    }
}

static void SdmcDeleteFile(const char *path)
{
    FS_Archive archive = 0;
    if (R_SUCCEEDED(FSUSER_OpenArchive(&archive, ARCHIVE_SDMC, fsMakePath(PATH_EMPTY, ""))))
    {
        FSUSER_DeleteFile(archive, fsMakePath(PATH_ASCII, path));
        FSUSER_CloseArchive(archive);
    }
}

static void ColorsSave(void)
{
    SdmcDeleteFile(COLORS_PATH);

    IFile f;
    if (R_FAILED(IFile_Open(&f, ARCHIVE_SDMC, fsMakePath(PATH_EMPTY, ""),
                            fsMakePath(PATH_ASCII, COLORS_PATH), FS_OPEN_CREATE | FS_OPEN_WRITE)))
        return;

    char buf[160];
    u64 total = 0;
    u32 len = (u32)sprintf(buf, "%u %u %u\n%u %u %u\n%u %u %u\n%u %u %u\n",
        (unsigned)g_colH[0], (unsigned)g_colS[0], (unsigned)g_colV[0],
        (unsigned)g_colH[1], (unsigned)g_colS[1], (unsigned)g_colV[1],
        (unsigned)g_colH[2], (unsigned)g_colS[2], (unsigned)g_colV[2],
        (unsigned)g_colH[3], (unsigned)g_colS[3], (unsigned)g_colV[3]);
    IFile_Write(&f, &total, buf, len, FS_WRITE_FLUSH);
    IFile_Close(&f);

    Backup_Snapshot();
}

void RosalinaMenu_AccentColor(void)
{
    AccentColor_EnsureLoaded();

    u32 H[4], S[4], V[4];
    for (u32 i = 0; i < 4; ++i) { H[i] = g_colH[i]; S[i] = g_colS[i]; V[i] = g_colV[i]; }

    static const char *const names[4] = { "accent", "text", "cursor", "bg" };
    u32 target = 0;

    menuEnter();
    Draw_Lock();
    Draw_ClearFramebuffer();
    Draw_Unlock();

    do
    {
        hidScanInput();
        u32 down = hidKeysDown();
        u32 held = hidKeysHeld();

        if (down & KEY_SELECT)
            target = (target + 1) & 3;

        if (held & KEY_TOUCH)
        {
            touchPosition tp;
            hidTouchRead(&tp);
            u32 px = tp.px;
            u32 py = tp.py;

            if (px >= HUE_X && px < HUE_X + HUE_W && py + 4 >= HUE_Y && py <= HUE_Y + HUE_H + 4)
                H[target] = (px - HUE_X) * 360 / HUE_W;
            else if (px >= SV_X && px < SV_X + SV_W && py >= SV_Y && py < SV_Y + SV_H)
            {
                S[target] = (px - SV_X) * 255 / (SV_W - 1);
                V[target] = 255 - (py - SV_Y) * 255 / (SV_H - 1);
            }
            else if (px >= PRE_X && px < PRE_X + PRE_W && py >= PRE_Y && py < PRE_Y + PRE_N * PRE_H)
            {
                u32 idx = (py - PRE_Y) / PRE_H;
                if (idx < PRE_N) { H[target] = g_presetHSV[idx][0]; S[target] = g_presetHSV[idx][1]; V[target] = g_presetHSV[idx][2]; }
            }

            if (H[target] > 359) H[target] = 359;
            if (S[target] > 255) S[target] = 255;
            if (V[target] > 255) V[target] = 255;
        }

        Draw_Lock();
        Draw_DrawString(10, 10, COLOR_WHITE, "Colors");
        Draw_DrawFormattedString(90, 10, COLOR_WHITE, "editing: %s    ", names[target]);
        AccentDrawBase(H[target], S[target], V[target]);
        Draw_DrawFormattedString(110, PRV_Y, COLOR_WHITE, "H %u    ", (unsigned)H[target]);
        Draw_DrawFormattedString(110, PRV_Y + SPACING_Y, COLOR_WHITE, "S %u  V %u    ", (unsigned)S[target], (unsigned)V[target]);
        Draw_DrawString(110, PRV_Y + 2 * SPACING_Y + 2, COLOR_WHITE, "SELECT: next   A: save   B: cancel");
        Draw_DrawString(10, 220, COLOR_WHITE, "Touch bar / box / swatches to pick");
        Draw_FlushFramebuffer();
        Draw_Unlock();

        if (down & KEY_A)
        {
            for (u32 i = 0; i < 4; ++i) { g_colH[i] = H[i]; g_colS[i] = S[i]; g_colV[i] = V[i]; }
            ColorsApply();
            ColorsSave();
            break;
        }
        if (down & KEY_B)
            break;

        svcSleepThread(20 * 1000 * 1000LL);
    }
    while (!menuShouldExit);

    menuLeave();
}

static u64 Checker_GetForegroundTitleId(void)
{
    u32 pidList[0x40];
    s32 count = 0;

    if (R_FAILED(svcGetProcessList(&count, pidList, 0x40)))
        return 0;

    for (s32 i = 0; i < count; ++i)
    {
        Handle h;
        if (R_FAILED(svcOpenProcess(&h, pidList[i])))
            continue;

        u64 tid = 0;
        svcGetProcessInfo((s64 *)&tid, h, 0x10001);
        svcCloseHandle(h);

        u32 hi = (u32)(tid >> 32);
        if (hi == 0x00040010 || hi == 0x00040000)
            return tid;
    }

    return 0;
}

static bool Checker_HasPlugin(u64 tid, char *outName, u32 outSize)
{
    char        dirPath[64];
    FS_Archive  arch = 0;
    Handle      dir = 0;
    bool        found = false;

    sprintf(dirPath, "/luma/plugins/%016llX", tid);

    if (R_FAILED(FSUSER_OpenArchive(&arch, ARCHIVE_SDMC, fsMakePath(PATH_EMPTY, ""))))
        return false;

    if (R_SUCCEEDED(FSUSER_OpenDirectory(&dir, arch, fsMakePath(PATH_ASCII, dirPath))))
    {
        u32                 n = 0;
        FS_DirectoryEntry   entries[8];
        static const u16    ext[] = u"3gx";

        while (!found && R_SUCCEEDED(FSDIR_Read(dir, &n, 8, entries)) && n > 0)
        {
            for (u32 i = 0; i < n; ++i)
            {
                if (entries[i].attributes & 1)
                    continue;

                u32 len = 0;
                while (entries[i].name[len])
                    ++len;
                if (len <= 4)
                    continue;

                if (memcmp(entries[i].name + len - 3, ext, 3 * sizeof(u16)) == 0)
                {
                    if (outName && outSize > 0)
                    {
                        int units = utf16_to_utf8((u8 *)outName, entries[i].name, outSize - 1);
                        if (units < 0)
                            units = 0;
                        outName[units] = 0;
                    }
                    found = true;
                    break;
                }
            }
        }
        FSDIR_Close(dir);
    }

    FSUSER_CloseArchive(arch);
    return found;
}

static bool Checker_HasCheats(u64 tid, char *outPath, u32 outSize)
{
    char  path[64];
    IFile f;

    sprintf(path, "/luma/titles/%016llX/cheats.txt", tid);
    if (R_SUCCEEDED(IFile_Open(&f, ARCHIVE_SDMC, fsMakePath(PATH_EMPTY, ""),
                               fsMakePath(PATH_ASCII, path), FS_OPEN_READ)))
    {
        IFile_Close(&f);
        if (outPath && outSize > 0) { strncpy(outPath, path, outSize - 1); outPath[outSize - 1] = 0; }
        return true;
    }

    sprintf(path, "/cheats/%016llX.txt", tid);
    if (R_SUCCEEDED(IFile_Open(&f, ARCHIVE_SDMC, fsMakePath(PATH_EMPTY, ""),
                               fsMakePath(PATH_ASCII, path), FS_OPEN_READ)))
    {
        IFile_Close(&f);
        if (outPath && outSize > 0) { strncpy(outPath, path, outSize - 1); outPath[outSize - 1] = 0; }
        return true;
    }

    return false;
}

static void Checker_DeleteReport(u64 tid)
{
    char path[64];

    sprintf(path, "/luma/%016llX.txt", tid);
    SdmcDeleteFile(path);
}

static void Checker_WriteReport(u64 tid, bool hasPlugin, bool hasCheats)
{
    char  path[64];
    IFile f;
    u64   total = 0;
    char  buf[512];

    Checker_DeleteReport(tid);

    sprintf(path, "/luma/%016llX.txt", tid);

    if (R_FAILED(IFile_Open(&f, ARCHIVE_SDMC, fsMakePath(PATH_EMPTY, ""),
                            fsMakePath(PATH_ASCII, path), FS_OPEN_CREATE | FS_OPEN_WRITE)))
        return;

    int n = sprintf(buf,
        "Title ID: %016llX\r\n\r\n"
        "3gx plugin: %s\r\n"
        "  put a .3gx file in this folder:\r\n"
        "  sdmc:/luma/plugins/%016llX/\r\n\r\n"
        "cheats txt: %s\r\n"
        "  put the cheats file at either path:\r\n"
        "  sdmc:/luma/titles/%016llX/cheats.txt\r\n"
        "  sdmc:/cheats/%016llX.txt\r\n",
        tid,
        hasPlugin ? "FOUND" : "MISSING", tid,
        hasCheats ? "FOUND" : "MISSING", tid, tid);

    IFile_Write(&f, &total, buf, (u32)n, FS_WRITE_FLUSH);
    IFile_Close(&f);
}

void RosalinaMenu_3gxTxtChecker(void)
{
    u64  tid = Checker_GetForegroundTitleId();
    char pluginName[64] = {0};
    char cheatsPath[64] = {0};
    bool hasPlugin = false;
    bool hasCheats = false;

    if (tid != 0)
    {
        hasPlugin = Checker_HasPlugin(tid, pluginName, sizeof(pluginName));
        hasCheats = Checker_HasCheats(tid, cheatsPath, sizeof(cheatsPath));

        if (hasPlugin && hasCheats)
            Checker_DeleteReport(tid);
        else
        {
            Checker_WriteReport(tid, hasPlugin, hasCheats);
            LogLine("checker: missing 3gx or cheats txt");
        }
    }

    menuEnter();

    do
    {
        Draw_Lock();
        Draw_ClearFramebuffer();
        Draw_DrawString(10, 10, COLOR_TITLE, "3gx / txt checker");

        if (tid == 0)
        {
            Draw_DrawString(10, 40, COLOR_RED, "No game is running.");
            Draw_DrawString(10, 40 + SPACING_Y, COLOR_WHITE, "Open a game first, then run this.");
        }
        else
        {
            u32 y = 34;
            Draw_DrawFormattedString(10, y, COLOR_WHITE, "Title ID: %016llX", tid);
            y += SPACING_Y + 6;

            Draw_DrawFormattedString(10, y, hasPlugin ? COLOR_LIME : COLOR_RED,
                "3gx : %s", hasPlugin ? "found" : "MISSING");
            y += SPACING_Y;
            if (hasPlugin)
                Draw_DrawFormattedString(20, y, COLOR_WHITE, "%s", pluginName);
            else
                Draw_DrawFormattedString(20, y, COLOR_WHITE, "/luma/plugins/%016llX/", tid);
            y += SPACING_Y + 4;

            Draw_DrawFormattedString(10, y, hasCheats ? COLOR_LIME : COLOR_RED,
                "txt : %s", hasCheats ? "found" : "MISSING");
            y += SPACING_Y;
            if (hasCheats)
            {
                Draw_DrawFormattedString(20, y, COLOR_WHITE, "%s", cheatsPath);
                y += SPACING_Y;
            }
            else
            {
                Draw_DrawFormattedString(20, y, COLOR_WHITE, "/luma/titles/%016llX/cheats.txt", tid);
                y += SPACING_Y;
                Draw_DrawFormattedString(20, y, COLOR_WHITE, "/cheats/%016llX.txt", tid);
                y += SPACING_Y;
            }
            y += 6;

            if (hasPlugin && hasCheats)
                Draw_DrawString(10, y, COLOR_LIME, "All present. Report removed.");
            else
                Draw_DrawFormattedString(10, y, COLOR_YELLOW, "Wrote /luma/%016llX.txt", tid);
        }

        Draw_DrawString(10, 220, COLOR_WHITE, "B: back");
        Draw_FlushFramebuffer();
        Draw_Unlock();
    }
    while (!(waitInput() & KEY_B) && !menuShouldExit);

    menuLeave();
}

void LogLine(const char *msg)
{
    IFile f;
    u64   size = 0;

    if (R_FAILED(IFile_Open(&f, ARCHIVE_SDMC, fsMakePath(PATH_EMPTY, ""),
                            fsMakePath(PATH_ASCII, "/luma/log.txt"), FS_OPEN_CREATE | FS_OPEN_WRITE)))
        return;

    IFile_GetSize(&f, &size);

    if (size > 65536)
    {
        IFile_Close(&f);
        SdmcDeleteFile("/luma/log.txt");
        if (R_FAILED(IFile_Open(&f, ARCHIVE_SDMC, fsMakePath(PATH_EMPTY, ""),
                                fsMakePath(PATH_ASCII, "/luma/log.txt"), FS_OPEN_CREATE | FS_OPEN_WRITE)))
            return;
        size = 0;
    }

    f.pos = size;

    char line[256];
    u64  wr = 0;
    u32  n = (u32)sprintf(line, "%s\r\n", msg);
    IFile_Write(&f, &wr, line, n, FS_WRITE_FLUSH);
    IFile_Close(&f);
}

static u64 SD_FreeBytes(void)
{
    FS_ArchiveResource res;
    if (R_FAILED(FSUSER_GetArchiveResource(&res, SYSTEM_MEDIATYPE_SD)))
        return (u64)-1;
    return (u64)res.freeClusters * (u64)res.clusterSize;
}

static bool PathExists(const char *path, bool dir)
{
    if (dir)
    {
        FS_Archive a = 0;
        Handle     h = 0;
        bool       ok = false;

        if (R_SUCCEEDED(FSUSER_OpenArchive(&a, ARCHIVE_SDMC, fsMakePath(PATH_EMPTY, ""))))
        {
            if (R_SUCCEEDED(FSUSER_OpenDirectory(&h, a, fsMakePath(PATH_ASCII, path))))
            {
                FSDIR_Close(h);
                ok = true;
            }
            FSUSER_CloseArchive(a);
        }
        return ok;
    }

    IFile f;
    if (R_SUCCEEDED(IFile_Open(&f, ARCHIVE_SDMC, fsMakePath(PATH_EMPTY, ""),
                               fsMakePath(PATH_ASCII, path), FS_OPEN_READ)))
    {
        IFile_Close(&f);
        return true;
    }
    return false;
}

static bool BackupCopy(const char *src, const char *dst)
{
    IFile in;
    char  buf[512];
    u64   rd = 0;

    if (R_FAILED(IFile_Open(&in, ARCHIVE_SDMC, fsMakePath(PATH_EMPTY, ""),
                            fsMakePath(PATH_ASCII, src), FS_OPEN_READ)))
        return false;

    Result r = IFile_Read(&in, &rd, buf, sizeof(buf));
    IFile_Close(&in);
    if (R_FAILED(r))
        return false;

    SdmcDeleteFile(dst);

    IFile out;
    if (R_FAILED(IFile_Open(&out, ARCHIVE_SDMC, fsMakePath(PATH_EMPTY, ""),
                            fsMakePath(PATH_ASCII, dst), FS_OPEN_CREATE | FS_OPEN_WRITE)))
        return false;

    u64 wr = 0;
    IFile_Write(&out, &wr, buf, (u32)rd, FS_WRITE_FLUSH);
    IFile_Close(&out);
    return true;
}

static void Backup_Snapshot(void)
{
    FS_Archive a = 0;
    if (R_SUCCEEDED(FSUSER_OpenArchive(&a, ARCHIVE_SDMC, fsMakePath(PATH_EMPTY, ""))))
    {
        FSUSER_CreateDirectory(a, fsMakePath(PATH_ASCII, "/luma/backup"), 0);
        FSUSER_CloseArchive(a);
    }

    BackupCopy("/luma/backup/colors1.cfg", "/luma/backup/colors2.cfg");
    BackupCopy("/luma/backup/colors0.cfg", "/luma/backup/colors1.cfg");
    BackupCopy(COLORS_PATH, "/luma/backup/colors0.cfg");
}

void SDSpaceWarnIfLow(void)
{
    static bool warned = false;
    if (warned)
        return;
    warned = true;

    u64 freeBytes = SD_FreeBytes();
    if (freeBytes == (u64)-1 || freeBytes >= 100ULL * 1024 * 1024)
        return;

    LogLine("warning: low SD space");

    menuEnter();
    do
    {
        Draw_Lock();
        Draw_ClearFramebuffer();
        Draw_DrawString(10, 10, COLOR_RED, "Low SD space");
        Draw_DrawFormattedString(10, 40, COLOR_WHITE, "Free: %u MB", (unsigned)(freeBytes >> 20));
        Draw_DrawString(10, 40 + SPACING_Y, COLOR_WHITE, "Consider freeing some space.");
        Draw_DrawString(10, 40 + 3 * SPACING_Y, COLOR_WHITE, "B: continue");
        Draw_FlushFramebuffer();
        Draw_Unlock();
    }
    while (!(waitInput() & KEY_B) && !menuShouldExit);
    menuLeave();
}

void RosalinaMenu_SelfTest(void)
{
    static const char *const paths[] = {
        "/luma", "/luma/plugins", "/3ds/luma/9.1/boot.firm", "/3ds/luma/13.4/boot.firm", "/boot.firm"
    };
    static const bool isDir[] = { true, true, false, false, false };
    bool ok[5];

    for (u32 i = 0; i < 5; ++i)
        ok[i] = PathExists(paths[i], isDir[i]);

    u64 freeBytes = SD_FreeBytes();

    menuEnter();
    do
    {
        Draw_Lock();
        Draw_ClearFramebuffer();
        Draw_DrawString(10, 10, COLOR_TITLE, "Self test");

        u32 y = 34;
        for (u32 i = 0; i < 5; ++i)
        {
            Draw_DrawFormattedString(10, y, ok[i] ? COLOR_LIME : COLOR_RED, "[%s] %s",
                ok[i] ? "OK" : "--", paths[i]);
            y += SPACING_Y;
        }
        y += 4;
        if (freeBytes == (u64)-1)
            Draw_DrawString(10, y, COLOR_WHITE, "SD free: N/A");
        else
            Draw_DrawFormattedString(10, y, COLOR_WHITE, "SD free: %u MB", (unsigned)(freeBytes >> 20));

        Draw_DrawString(10, 220, COLOR_WHITE, "B: back");
        Draw_FlushFramebuffer();
        Draw_Unlock();
    }
    while (!(waitInput() & KEY_B) && !menuShouldExit);
    menuLeave();
}

void RosalinaMenu_RestoreSettings(void)
{
    s32  sel = 0;
    s32  status = 0;

    menuEnter();
    do
    {
        Draw_Lock();
        Draw_ClearFramebuffer();
        Draw_DrawString(10, 10, COLOR_TITLE, "Restore settings");
        Draw_DrawString(10, 34, COLOR_WHITE, "Restore a saved colour backup:");

        for (s32 i = 0; i < 3; ++i)
            Draw_DrawFormattedString(20, 58 + i * SPACING_Y, i == sel ? COLOR_LIME : COLOR_WHITE,
                "%s backup %d%s", i == sel ? ">" : " ", (int)i, i == 0 ? " (newest)" : "");

        if (status == 1)
            Draw_DrawString(10, 58 + 4 * SPACING_Y, COLOR_LIME, "Restored. Colours reloaded.");
        else if (status == 2)
            Draw_DrawString(10, 58 + 4 * SPACING_Y, COLOR_RED, "That backup was not found.");

        Draw_DrawString(10, 210, COLOR_WHITE, "A: restore   B: back");
        Draw_FlushFramebuffer();
        Draw_Unlock();

        u32 k = waitInput();
        if (menuShouldExit)
            break;

        if (k & KEY_DOWN)
            sel = (sel + 1) % 3;
        else if (k & KEY_UP)
            sel = (sel + 2) % 3;
        else if (k & KEY_A)
        {
            char src[64];
            sprintf(src, "/luma/backup/colors%d.cfg", (int)sel);
            if (BackupCopy(src, COLORS_PATH))
            {
                g_colorsLoaded = false;
                AccentColor_EnsureLoaded();
                status = 1;
                LogLine("settings restored from backup");
            }
            else
                status = 2;
        }
        else if (k & KEY_B)
            break;
    }
    while (!menuShouldExit);
    menuLeave();
}

#define FB_MAX 256
static char               g_fbName[FB_MAX][64];
static bool               g_fbDir[FB_MAX];
static u64                g_fbSize[FB_MAX];
static FS_DirectoryEntry  g_fbEntries[16];

static u32 FileBrowser_Load(const char *path)
{
    u32 count = 0;

    for (int wantDir = 1; wantDir >= 0; --wantDir)
    {
        FS_Archive arch = 0;
        Handle     dir = 0;

        if (R_FAILED(FSUSER_OpenArchive(&arch, ARCHIVE_SDMC, fsMakePath(PATH_EMPTY, ""))))
            break;

        if (R_SUCCEEDED(FSUSER_OpenDirectory(&dir, arch, fsMakePath(PATH_ASCII, path))))
        {
            u32 n = 0;
            while (count < FB_MAX && R_SUCCEEDED(FSDIR_Read(dir, &n, 16, g_fbEntries)) && n > 0)
            {
                for (u32 i = 0; i < n && count < FB_MAX; ++i)
                {
                    bool isDir = (g_fbEntries[i].attributes & 1) != 0;
                    if ((int)isDir != wantDir)
                        continue;

                    int units = utf16_to_utf8((u8 *)g_fbName[count], g_fbEntries[i].name, 63);
                    if (units < 0)
                        units = 0;
                    g_fbName[count][units] = 0;
                    g_fbDir[count] = isDir;
                    g_fbSize[count] = g_fbEntries[i].fileSize;
                    ++count;
                }
            }
            FSDIR_Close(dir);
        }

        FSUSER_CloseArchive(arch);
    }

    return count;
}

static void FileBrowser_Up(char *path)
{
    u32 l = 0;
    while (path[l]) ++l;
    if (l > 1)
    {
        --l;
        while (l > 0 && path[l - 1] != '/') --l;
        path[l] = 0;
    }
}

void RosalinaMenu_FileBrowser(void)
{
    static char path[512];
    path[0] = '/';
    path[1] = 0;

    u32 count = FileBrowser_Load(path);
    s32 sel = 0, scroll = 0;
    const s32 rows = 17;

    menuEnter();
    do
    {
        bool atRoot = (path[0] == '/' && path[1] == 0);
        s32  total = (s32)count + (atRoot ? 0 : 1);

        if (sel >= total) sel = total > 0 ? total - 1 : 0;
        if (sel < 0) sel = 0;
        if (sel < scroll) scroll = sel;
        if (sel >= scroll + rows) scroll = sel - rows + 1;

        Draw_Lock();
        Draw_ClearFramebuffer();
        Draw_DrawString(10, 4, COLOR_TITLE, "File browser");

        u32 plen = 0; while (path[plen]) ++plen;
        const char *pshow = path;
        if (plen > 48) pshow = path + (plen - 48);
        Draw_DrawFormattedString(10, 4 + SPACING_Y, COLOR_WHITE, "%s%s   ", plen > 48 ? "..." : "", pshow);

        u32 y = 4 + 3 * SPACING_Y;
        for (s32 r = 0; r < rows; ++r)
        {
            s32 idx = scroll + r;
            if (idx >= total) break;

            u32 col = (idx == sel) ? COLOR_LIME : COLOR_WHITE;
            const char *cur = (idx == sel) ? ">" : " ";

            if (!atRoot && idx == 0)
                Draw_DrawFormattedString(10, y, col, "%s [..]", cur);
            else
            {
                s32 e = idx - (atRoot ? 0 : 1);
                if (g_fbDir[e])
                    Draw_DrawFormattedString(10, y, col, "%s [%s]", cur, g_fbName[e]);
                else
                    Draw_DrawFormattedString(10, y, col, "%s %s  (%u KB)", cur, g_fbName[e],
                        (unsigned)(g_fbSize[e] >> 10));
            }
            y += SPACING_Y;
        }

        Draw_DrawString(10, 225, COLOR_WHITE, "A: open   B: up / exit");
        Draw_FlushFramebuffer();
        Draw_Unlock();

        u32 k = waitInput();
        if (menuShouldExit)
            break;

        if (k & KEY_DOWN)      { if (sel + 1 < total) ++sel; }
        else if (k & KEY_UP)   { if (sel > 0) --sel; }
        else if (k & KEY_A)
        {
            if (!atRoot && sel == 0)
            {
                FileBrowser_Up(path);
                count = FileBrowser_Load(path); sel = 0; scroll = 0;
            }
            else
            {
                s32 e = sel - (atRoot ? 0 : 1);
                if (e >= 0 && e < (s32)count && g_fbDir[e])
                {
                    u32 l = 0; while (path[l]) ++l;
                    const char *nm = g_fbName[e];
                    u32 nl = 0; while (nm[nl]) ++nl;
                    if (l + nl + 2 < sizeof(path))
                    {
                        for (u32 i = 0; i < nl; ++i) path[l + i] = nm[i];
                        path[l + nl] = '/';
                        path[l + nl + 1] = 0;
                        count = FileBrowser_Load(path); sel = 0; scroll = 0;
                    }
                }
            }
        }
        else if (k & KEY_B)
        {
            if (atRoot) break;
            FileBrowser_Up(path);
            count = FileBrowser_Load(path); sel = 0; scroll = 0;
        }
    }
    while (!menuShouldExit);
    menuLeave();
}

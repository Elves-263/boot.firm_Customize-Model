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
    .nbItems = 15,
    {
        { TR("New 3DS メニュー...", "New 3DS menu..."), MENU, .menu = &N3DSMenu },
        { TR("チート...", "Cheats..."), METHOD, .method = &RosalinaMenu_Cheats },
        { "", METHOD, .method = PluginLoader__MenuCallback},
        { TR("プロセス一覧", "Process list"), METHOD, .method = &RosalinaMenu_ProcessList },
        { TR("スクリーンショットを撮る（遅い）", "Take screenshot (slow!)"), METHOD, .method = &RosalinaMenu_TakeScreenshot },
        { TR("デバッガーの設定...", "Debugger options..."), MENU, .menu = &debuggerMenu },
        { TR("システム設定...", "System configuration..."), MENU, .menu = &sysconfigMenu },
        { TR("その他の設定...", "Miscellaneous options..."), MENU, .menu = &miscellaneousMenu },
        { "言語 / Language", METHOD, .method = &RosalinaMenu_Language },
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




#define MY_LUMA_VERSION FORK_LUMA_VERSION
#define OTHER_LUMA_VERSION FORK_OTHER_VERSION

#define CHAINLOAD_FLAG_PATH "/luma/chainload_ver.flag"


static bool chainloadVersionFlagSet(const char *id)
{
    IFile f;
    u64 total = 0;

    if(R_FAILED(IFile_Open(&f, ARCHIVE_SDMC, fsMakePath(PATH_EMPTY, ""),
                           fsMakePath(PATH_ASCII, CHAINLOAD_FLAG_PATH),
                           FS_OPEN_CREATE | FS_OPEN_WRITE)))
        return false;

    
    char buf[4];
    u32 len = strlen(id);

    memcpy(buf, id, len);
    memset(buf + len, '\n', sizeof(buf) - len);

    bool ok = R_SUCCEEDED(IFile_Write(&f, &total, buf, sizeof(buf), FS_WRITE_FLUSH));
    IFile_Close(&f);
    return ok;
}

bool chainloadVersionFirmExists(const char *id)
{
    char path[40];

    sprintf(path, "/3ds/luma/%s/boot.firm", id);
    return firmFileValid(path);
}


void RosalinaMenu_ChainloadVersionToggle(void)
{
    Draw_Lock();
    Draw_ClearFramebuffer();
    Draw_FlushFramebuffer();
    Draw_Unlock();

    const char *target = OTHER_LUMA_VERSION;     // 画面に出す版
    const char *targetId = FORK_OTHER_ID;        // フォルダとしるしに使う名前

    do
    {
        Draw_Lock();
        Draw_DrawString(10, 10, COLOR_TITLE, Lang(STR_VERSION_SWITCH_CHAINLOAD));
        Draw_DrawFormattedString(10, 30, COLOR_WHITE, Lang(STR_CURRENT_V_V),
            MY_LUMA_VERSION, target);
        Draw_DrawString(10, 30 + SPACING_Y, COLOR_WHITE, Lang(STR_A_LAUNCH_B_GO_BACK));
        Draw_FlushFramebuffer();
        Draw_Unlock();

        u32 pressed = waitInputWithTimeout(1000);

        if(pressed & BUTTON_A)
        {
            if(!chainloadVersionFirmExists(targetId))
            {
                LogLine("[バージョン切り替え: 切り替え先の boot.firm が見つかりません] / [version switch: target boot.firm missing]");
                Draw_Lock();
                Draw_ClearFramebuffer();
                Draw_FlushFramebuffer();
                Draw_Unlock();

                do
                {
                    Draw_Lock();
                    Draw_DrawString(10, 10, COLOR_TITLE, Lang(STR_VERSION_SWITCH_CHAINLOAD));
                    Draw_DrawFormattedString(10, 30, COLOR_RED, "[v%s の boot.firm が見つかりません]", target);
                    Draw_DrawFormattedString(10, 30 + SPACING_Y, COLOR_RED, "[boot.firm for v%s was not found]", target);
                    Draw_DrawString(10, 30 + 3 * SPACING_Y, COLOR_WHITE, TR("B: 戻る", "B: back"));
                    Draw_FlushFramebuffer();
                    Draw_Unlock();
                }
                while(!(waitInput() & BUTTON_B) && !terminationRequest);

                return;
            }

            
            if(!chainloadVersionFlagSet(targetId))
            {
                LogLine("[バージョン切り替え: SD への書き込みに失敗しました] / [version switch: SD write failed]");
                Draw_Lock();
                Draw_ClearFramebuffer();
                Draw_FlushFramebuffer();
                Draw_Unlock();

                do
                {
                    Draw_Lock();
                    Draw_DrawString(10, 10, COLOR_TITLE, Lang(STR_VERSION_SWITCH_CHAINLOAD));
                    Draw_DrawBilingual(10, 30, COLOR_RED, "SD カードへの書き込みに失敗しました", "Failed to write to the SD card");
                    Draw_DrawString(10, 30 + 3 * SPACING_Y, COLOR_WHITE, TR("B: 戻る", "B: back"));
                    Draw_FlushFramebuffer();
                    Draw_Unlock();
                }
                while(!(waitInput() & BUTTON_B) && !terminationRequest);

                return;
            }

            menuLeave();
            APT_HardwareResetAsync();
            return;
        }
        else if(pressed & BUTTON_B)
            return;
    }
    while(!terminationRequest);
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

bool g_safeMode = false;

static u16 g_utf16Path[IFILE_PATH_UNITS];
static u16 g_utf16Path2[IFILE_PATH_UNITS];

static void SdmcDeleteFile(const char *path)
{
    FS_Archive archive = 0;
    if (R_SUCCEEDED(FSUSER_OpenArchive(&archive, ARCHIVE_SDMC, fsMakePath(PATH_EMPTY, ""))))
    {
        FSUSER_DeleteFile(archive, IFile_MakeUtf8Path(g_utf16Path, path));
        FSUSER_CloseArchive(archive);
    }
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
            if (R_SUCCEEDED(FSUSER_OpenDirectory(&h, a, IFile_MakeUtf8Path(g_utf16Path, path))))
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
                               IFile_MakeUtf8Path(g_utf16Path, path), FS_OPEN_READ)))
    {
        IFile_Close(&f);
        return true;
    }
    return false;
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

    LogLine("[警告: SD カードの空き容量が不足しています] / [warning: low SD space]");

    menuEnter();
    do
    {
        Draw_Lock();
        Draw_ClearFramebuffer();
        Draw_DrawString(10, 10, COLOR_RED, TR("SD の空き容量が足りません", "Low free space on the SD card"));
        Draw_DrawFormattedString(10, 40, COLOR_WHITE, Lang(STR_FREE_MB), (unsigned)(freeBytes >> 20));
        Draw_DrawString(10, 40 + SPACING_Y, COLOR_WHITE, TR("空き容量を増やしてください", "Please free up some space"));
        Draw_DrawString(10, 40 + 3 * SPACING_Y, COLOR_WHITE, TR("B: 続ける", "B: continue"));
        Draw_FlushFramebuffer();
        Draw_Unlock();
    }
    while (!(waitInput() & BUTTON_B) && !terminationRequest);
    menuLeave();
}

#define CHAINLOAD_FAIL_FLAG_PATH "/luma/chainload_fail.flag"

static bool g_chainloadFailed;

// バージョン切り替えで SD:/boot.firm を差し替えられなかったとき、ARM9 が印を残す。
// 起動後に 1 度だけ呼び、ここで読んで消す（メニューを開かないまま電源を切っても、
// 次の起動に持ち越さないため）。
void ChainloadCheckFailed(void)
{
    if (!PathExists(CHAINLOAD_FAIL_FLAG_PATH, false))
        return;

    g_chainloadFailed = true;
    SdmcDeleteFile(CHAINLOAD_FAIL_FLAG_PATH);
    LogLine("[バージョン切り替え: boot.firm を差し替えられませんでした] / [version switch: could not replace boot.firm]");
}

// 差し替えに失敗していたら、起動後に初めてメニューを開いたときに知らせる。
// この回は切り替え先で動いているが、次に電源を入れると元の版で起動する。
void ChainloadWarnIfFailed(void)
{
    char kanji[64];
    char english[64];

    if (!g_chainloadFailed)
        return;
    g_chainloadFailed = false;

    sprintf(kanji, "次の起動は v%s になります", OTHER_LUMA_VERSION);
    sprintf(english, "v%s will boot next time", OTHER_LUMA_VERSION);

    menuEnter();
    do
    {
        Draw_Lock();
        Draw_ClearFramebuffer();
        Draw_DrawString(10, 10, COLOR_RED, Lang(STR_VERSION_SWITCH_CHAINLOAD));
        u32 posY = Draw_DrawBilingual(10, 40, COLOR_WHITE, "SD:/boot.firm を差し替えられませんでした",
                                      "Could not replace SD:/boot.firm");
        posY = Draw_DrawBilingual(10, posY + 2 * SPACING_Y, COLOR_WHITE, kanji, english);
        posY = Draw_DrawBilingual(10, posY + 2 * SPACING_Y, COLOR_WHITE, "SD カードを確認してください",
                                  "Please check the SD card");
        Draw_DrawString(10, posY + 3 * SPACING_Y, COLOR_WHITE, TR("B: 続ける", "B: continue"));
        Draw_FlushFramebuffer();
        Draw_Unlock();
    }
    while (!(waitInput() & BUTTON_B) && !terminationRequest);
    menuLeave();
}

#define FB_MAX 256
#define FB_READ_MAX 4
#define FB_NAME_COLS ((SCROLL_MARK_X - 10) / SPACING_X - 3)
#define FB_TITLE      TR("ファイルブラウザー", "File browser")
#define FB_CLIP_LABEL TR("クリップボード: ", "Clipboard: ")
#define FB_SORT_ADDED 0
#define FB_SORT_NAME  1
#define FB_SORT_SIZE  2
#define FB_SORT_COUNT 3
static char               g_fbName[FB_MAX + 1][64];
static bool               g_fbDir[FB_MAX + 1];
static u64                g_fbSize[FB_MAX + 1];
static u16                g_fbIndex[FB_MAX + 1];
static u16                g_fbOrder[FB_MAX];
static FS_DirectoryEntry  g_fbEntries[FB_READ_MAX];
static char               g_fbScratch[0x106 * 3];
static u32                g_fbSort;
static u32                g_fbBlock;
static u32                g_fbTotal;

static s32 FileBrowser_Compare(bool isDir, const char *name, u64 size, u32 index, u32 e)
{
    if (isDir != g_fbDir[e])
        return isDir ? -1 : 1;

    if (g_fbSort == FB_SORT_NAME || (g_fbSort == FB_SORT_SIZE && isDir))
    {
        for (u32 i = 0; name[i] != 0 || g_fbName[e][i] != 0; ++i)
        {
            char a = name[i], b = g_fbName[e][i];

            if (a >= 'a' && a <= 'z')
                a -= 'a' - 'A';
            if (b >= 'a' && b <= 'z')
                b -= 'a' - 'A';
            if (a != b)
                return (u8)a < (u8)b ? -1 : 1;
        }
    }
    else if (g_fbSort == FB_SORT_SIZE && size != g_fbSize[e])
        return size > g_fbSize[e] ? -1 : 1;

    return index < g_fbIndex[e] ? -1 : (index > g_fbIndex[e] ? 1 : 0);
}

static void FileBrowser_Insert(u32 *count, bool isDir, const char *name, u64 size, u32 index)
{
    u32 used = *count, slot = *count, lo = 0;

    if (used == FB_MAX)
    {
        if (FileBrowser_Compare(isDir, name, size, index, g_fbOrder[FB_MAX - 1]) >= 0)
            return;
        slot = g_fbOrder[--used];
    }
    else
        ++*count;

    for (u32 hi = used; lo < hi;)
    {
        u32 mid = (lo + hi) / 2;

        if (FileBrowser_Compare(isDir, name, size, index, g_fbOrder[mid]) < 0)
            hi = mid;
        else
            lo = mid + 1;
    }

    for (u32 i = used; i > lo; --i)
        g_fbOrder[i] = g_fbOrder[i - 1];

    g_fbOrder[lo] = (u16)slot;
    memcpy(g_fbName[slot], name, sizeof(g_fbName[0]));
    g_fbDir[slot] = isDir;
    g_fbSize[slot] = size;
    g_fbIndex[slot] = (u16)index;
}

static u32 FileBrowser_Load(const char *path)
{
    u32 count = 0;

    g_fbTotal = 0;

    for (u32 pass = 0; pass <= g_fbBlock; ++pass)
    {
        FS_Archive arch = 0;
        Handle     dir = 0;
        bool       limited = pass > 0;

        if (limited)
        {
            if (count == 0)
            {
                g_fbBlock = pass - 1;
                break;
            }

            u32 last = g_fbOrder[count - 1];

            memcpy(g_fbName[FB_MAX], g_fbName[last], sizeof(g_fbName[0]));
            g_fbDir[FB_MAX] = g_fbDir[last];
            g_fbSize[FB_MAX] = g_fbSize[last];
            g_fbIndex[FB_MAX] = g_fbIndex[last];
        }

        count = 0;

        if (R_FAILED(FSUSER_OpenArchive(&arch, ARCHIVE_SDMC, fsMakePath(PATH_EMPTY, ""))))
            break;

        if (R_SUCCEEDED(FSUSER_OpenDirectory(&dir, arch, IFile_MakeUtf8Path(g_utf16Path, path))))
        {
            u32 n = 0, index = 0;
            while (index < 0xFFFF && R_SUCCEEDED(FSDIR_Read(dir, &n, FB_READ_MAX, g_fbEntries)) && n > 0)
            {
                for (u32 i = 0; i < n && index < 0xFFFF; ++i, ++index)
                {
                    bool isDir = (g_fbEntries[i].attributes & 1) != 0;
                    char name[sizeof(g_fbName[0])];

                    int units = utf16_to_utf8((u8 *)g_fbScratch, g_fbEntries[i].name, sizeof(g_fbScratch) - 1);
                    if (units < 0 || (u32)units >= sizeof(g_fbScratch))
                        continue;
                    g_fbScratch[units] = 0;
                    Draw_ClipText(name, sizeof(name), g_fbScratch, sizeof(name) - 1);

                    if (pass == 0)
                        ++g_fbTotal;

                    if (limited && FileBrowser_Compare(isDir, name, g_fbEntries[i].fileSize, index, FB_MAX) <= 0)
                        continue;

                    FileBrowser_Insert(&count, isDir, name, g_fbEntries[i].fileSize, index);
                }
            }
            FSDIR_Close(dir);
        }

        FSUSER_CloseArchive(arch);

        if (pass == 0 && g_fbBlock * FB_MAX >= g_fbTotal)
            g_fbBlock = g_fbTotal > 0 ? (g_fbTotal - 1) / FB_MAX : 0;
    }

    return count;
}

static bool FileBrowser_GetName(const char *dirPath, u32 e)
{
    FS_Archive arch = 0;
    Handle     dir = 0;
    bool       found = false;

    if (R_FAILED(FSUSER_OpenArchive(&arch, ARCHIVE_SDMC, fsMakePath(PATH_EMPTY, ""))))
        return false;

    if (R_SUCCEEDED(FSUSER_OpenDirectory(&dir, arch, IFile_MakeUtf8Path(g_utf16Path, dirPath))))
    {
        u32 n = 0, index = 0;
        while (R_SUCCEEDED(FSDIR_Read(dir, &n, FB_READ_MAX, g_fbEntries)) && n > 0)
        {
            if (g_fbIndex[e] < index + n)
            {
                const FS_DirectoryEntry *entry = &g_fbEntries[g_fbIndex[e] - index];
                int units = utf16_to_utf8((u8 *)g_fbScratch, entry->name, sizeof(g_fbScratch) - 1);
                char shown[sizeof(g_fbName[0])];

                if (units >= 0 && (u32)units < sizeof(g_fbScratch) && ((entry->attributes & 1) != 0) == g_fbDir[e])
                {
                    g_fbScratch[units] = 0;
                    Draw_ClipText(shown, sizeof(shown), g_fbScratch, sizeof(shown) - 1);
                    found = strcmp(shown, g_fbName[e]) == 0;
                }
                break;
            }
            index += n;
        }
        FSDIR_Close(dir);
    }

    FSUSER_CloseArchive(arch);
    return found;
}

#define FB_CLIP_NONE  0
#define FB_CLIP_COPY  1
#define FB_CLIP_CUT   2

static char g_fbClipPath[512];
static bool g_fbClipIsDir;
static u32  g_fbClipMode;

static bool FileBrowser_Join(char *dst, u32 dstSize, const char *dir, const char *name)
{
    u32 i = 0, j = 0;

    while (dir[i] != 0 && i + 1 < dstSize)
    {
        dst[i] = dir[i];
        ++i;
    }
    if (i == 0 || dst[i - 1] != '/')
    {
        if (i + 1 < dstSize)
            dst[i++] = '/';
    }
    while (name[j] != 0 && i + 1 < dstSize)
        dst[i++] = name[j++];

    dst[i] = 0;
    return name[j] == 0;
}

static const char *FileBrowser_BaseName(const char *path)
{
    const char *base = path;

    for (const char *p = path; *p != 0; ++p)
        if (*p == '/' && p[1] != 0)
            base = p + 1;

    return base;
}

static bool FileBrowser_IsPrefixOf(const char *prefix, const char *path)
{
    u32 i = 0;

    while (prefix[i] != 0)
    {
        if (path[i] != prefix[i])
            return false;
        ++i;
    }

    return path[i] == 0 || path[i] == '/';
}

static void FileBrowser_DrawProgress(const char *src, u64 done, u64 total, bool start)
{
    static char text[40];

    sprintf(text, "%lu / %lu KB   ", (u32)(done >> 10), (u32)(total >> 10));

    Draw_Lock();
    if (start)
    {
        Draw_ClipText(g_fbScratch, sizeof(g_fbScratch), FileBrowser_BaseName(src), FB_NAME_COLS);
        Draw_ClearFramebuffer();
        Draw_DrawString(10, 4, COLOR_TITLE, TR("貼り付け", "Paste"));
        Draw_DrawString(10, 4 + 3 * SPACING_Y, COLOR_WHITE, Lang(STR_COPYING));
        Draw_DrawString(10, 4 + 5 * SPACING_Y, COLOR_WHITE, g_fbScratch);
    }
    Draw_DrawString(10, 4 + 6 * SPACING_Y, COLOR_WHITE, text);
    Draw_FlushFramebuffer();
    Draw_Unlock();
}

// コピーの書き込み先（一時ファイル）と、上書きするときの元のファイルの退避先。
// どちらもコピー先と同じフォルダに、コピー先の名前に付け足した名前で作る（同じ長さにする）
#define FB_TMP_SUFFIX ".fbtmp"
#define FB_BAK_SUFFIX ".fbbak"

static char g_fbTmpPath[512];
static char g_fbBakPath[512];

static Result FileBrowser_Rename(const char *src, const char *dst, bool isDir);

// src を dst にコピーする。一時ファイルに書き終えてから dst と差し替えるので、
// 途中で失敗しても（SD の空き不足など）上書きしようとした元の dst は残る
static Result __attribute__((noinline)) FileBrowser_CopyFile(const char *src, const char *dst)
{
    IFile in, out;
    Result res;
    u64 size = 0;

    if (strlen(dst) + sizeof(FB_TMP_SUFFIX) > sizeof(g_fbTmpPath))
        return -1;
    sprintf(g_fbTmpPath, "%s" FB_TMP_SUFFIX, dst);
    sprintf(g_fbBakPath, "%s" FB_BAK_SUFFIX, dst);

    res = IFile_Open(&in, ARCHIVE_SDMC, fsMakePath(PATH_EMPTY, ""),
                     IFile_MakeUtf8Path(g_utf16Path, src), FS_OPEN_READ);
    if (R_FAILED(res))
        return res;

    res = IFile_GetSize(&in, &size);
    if (R_FAILED(res))
    {
        IFile_Close(&in);
        return res;
    }

    SdmcDeleteFile(g_fbTmpPath);

    res = IFile_Open(&out, ARCHIVE_SDMC, fsMakePath(PATH_EMPTY, ""),
                     IFile_MakeUtf8Path(g_utf16Path, g_fbTmpPath), FS_OPEN_CREATE | FS_OPEN_WRITE);
    if (R_FAILED(res))
    {
        IFile_Close(&in);
        return res;
    }

    in.pos = 0;
    out.pos = 0;

    FileBrowser_DrawProgress(src, 0, size, true);

    while (in.pos < size)
    {
        u64 read = 0, written = 0;

        res = IFile_Read(&in, &read, g_menuWorkBuf, sizeof(g_menuWorkBuf));
        if (R_FAILED(res))
            break;
        if (read == 0)
        {
            res = -1;
            break;
        }

        res = IFile_Write(&out, &written, g_menuWorkBuf, (u32)read, FS_WRITE_FLUSH);
        if (R_FAILED(res))
            break;

        FileBrowser_DrawProgress(src, in.pos, size, false);
    }

    IFile_Close(&out);
    IFile_Close(&in);

    // 書き終えたら差し替える。元の dst は退避しておき、差し替えられなかったら戻す
    if (R_SUCCEEDED(res))
    {
        bool hadDst = PathExists(dst, false);

        if (hadDst)
        {
            SdmcDeleteFile(g_fbBakPath);
            res = FileBrowser_Rename(dst, g_fbBakPath, false);
        }

        if (R_SUCCEEDED(res))
        {
            res = FileBrowser_Rename(g_fbTmpPath, dst, false);
            if (hadDst)
            {
                if (R_SUCCEEDED(res))
                    SdmcDeleteFile(g_fbBakPath);
                else
                    FileBrowser_Rename(g_fbBakPath, dst, false);
            }
        }
    }

    if (R_FAILED(res))
        SdmcDeleteFile(g_fbTmpPath);

    return res;
}

static Result FileBrowser_CopyTree(char *src, char *dst, u32 size, bool isDir, u32 depth)
{
    FS_Archive archive = 0;
    Result res;

    if (!isDir)
        return FileBrowser_CopyFile(src, dst);

    if (depth > 8)
        return -1;

    res = FSUSER_OpenArchive(&archive, ARCHIVE_SDMC, fsMakePath(PATH_EMPTY, ""));
    if (R_FAILED(res))
        return res;

    FSUSER_CreateDirectory(archive, IFile_MakeUtf8Path(g_utf16Path, dst), 0);

    Handle dir = 0;
    res = FSUSER_OpenDirectory(&dir, archive, IFile_MakeUtf8Path(g_utf16Path, src));
    if (R_FAILED(res))
    {
        FSUSER_CloseArchive(archive);
        return res;
    }

    u32 srcLen = strlen(src);
    u32 dstLen = strlen(dst);

    for (;;)
    {
        FS_DirectoryEntry *entry = &g_fbEntries[0];
        u32 n = 0;

        res = FSDIR_Read(dir, &n, 1, entry);
        if (R_FAILED(res) || n == 0)
            break;

        int units = utf16_to_utf8((u8 *)g_fbScratch, entry->name, sizeof(g_fbScratch) - 1);
        if (units < 0 || (u32)units >= sizeof(g_fbScratch))
        {
            res = -1;
            break;
        }
        g_fbScratch[units] = 0;

        if (!FileBrowser_Join(src, size, src, g_fbScratch) || !FileBrowser_Join(dst, size, dst, g_fbScratch))
            res = -1;
        else
            res = FileBrowser_CopyTree(src, dst, size, (entry->attributes & 1) != 0, depth + 1);

        src[srcLen] = 0;
        dst[dstLen] = 0;

        if (R_FAILED(res))
            break;
    }

    FSDIR_Close(dir);
    FSUSER_CloseArchive(archive);

    return res;
}

static Result FileBrowser_Delete(const char *path, bool isDir)
{
    FS_Archive archive = 0;
    Result res;

    res = FSUSER_OpenArchive(&archive, ARCHIVE_SDMC, fsMakePath(PATH_EMPTY, ""));
    if (R_FAILED(res))
        return res;

    if (isDir)
        res = FSUSER_DeleteDirectoryRecursively(archive, IFile_MakeUtf8Path(g_utf16Path, path));
    else
        res = FSUSER_DeleteFile(archive, IFile_MakeUtf8Path(g_utf16Path, path));

    FSUSER_CloseArchive(archive);
    return res;
}

static Result FileBrowser_Rename(const char *src, const char *dst, bool isDir)
{
    FS_Archive archive = 0;
    Result res;

    res = FSUSER_OpenArchive(&archive, ARCHIVE_SDMC, fsMakePath(PATH_EMPTY, ""));
    if (R_FAILED(res))
        return res;

    if (isDir)
        res = FSUSER_RenameDirectory(archive, IFile_MakeUtf8Path(g_utf16Path, src), archive, IFile_MakeUtf8Path(g_utf16Path2, dst));
    else
        res = FSUSER_RenameFile(archive, IFile_MakeUtf8Path(g_utf16Path, src), archive, IFile_MakeUtf8Path(g_utf16Path2, dst));

    FSUSER_CloseArchive(archive);
    return res;
}

static Result FileBrowser_MakeDir(const char *path)
{
    FS_Archive archive = 0;
    Result res;

    res = FSUSER_OpenArchive(&archive, ARCHIVE_SDMC, fsMakePath(PATH_EMPTY, ""));
    if (R_FAILED(res))
        return res;

    res = FSUSER_CreateDirectory(archive, IFile_MakeUtf8Path(g_utf16Path, path), 0);
    FSUSER_CloseArchive(archive);
    return res;
}

static void FileBrowser_Notify(const char *title, const char *msg, bool ok)
{
    Draw_Lock();
    Draw_ClearFramebuffer();
    Draw_DrawString(10, 4, COLOR_TITLE, title);
    Draw_DrawString(10, 4 + 3 * SPACING_Y, ok ? COLOR_LIME : COLOR_RED, msg);
    Draw_DrawString(10, 220, COLOR_WHITE, Lang(STR_PRESS_ANY_BUTTON));
    Draw_FlushFramebuffer();
    Draw_Unlock();
    waitInput();
}

static void FileBrowser_Paste(const char *destDir)
{
    char dst[512];
    Result res;

    if (g_fbClipMode == FB_CLIP_NONE)
    {
        FileBrowser_Notify(TR("貼り付け", "Paste"), Lang(STR_CLIPBOARD_IS_EMPTY), false);
        return;
    }

    if (!FileBrowser_Join(dst, sizeof(dst), destDir, FileBrowser_BaseName(g_fbClipPath)))
    {
        FileBrowser_Notify(TR("貼り付け", "Paste"), Lang(STR_PATH_IS_TOO_LONG), false);
        return;
    }

    if (strcmp(dst, g_fbClipPath) == 0)
    {
        FileBrowser_Notify(TR("貼り付け", "Paste"), Lang(STR_SOURCE_AND_DESTINATION_ARE_THE_SAME), false);
        return;
    }

    if (g_fbClipIsDir && FileBrowser_IsPrefixOf(g_fbClipPath, destDir))
    {
        FileBrowser_Notify(TR("貼り付け", "Paste"), Lang(STR_CANNOT_PASTE_A_FOLDER_INTO_ITSELF), false);
        return;
    }

    bool existed = PathExists(dst, true) || PathExists(dst, false);

    if (existed)
    {
        Draw_ClipText(g_fbScratch, sizeof(g_fbScratch), FileBrowser_BaseName(dst), sizeof(g_fbName[0]) - 1);
        if (!ConfirmPrompt(TR("貼り付け", "Paste"), Lang(STR_ALREADY_EXISTS_OVERWRITE), g_fbScratch))
            return;
    }

    if (g_fbClipMode == FB_CLIP_CUT)
    {
        res = FileBrowser_Rename(g_fbClipPath, dst, g_fbClipIsDir);
        if (R_SUCCEEDED(res))
        {
            g_fbClipMode = FB_CLIP_NONE;
            FileBrowser_Notify(TR("移動", "Move"), Lang(STR_MOVED), true);
            return;
        }
    }

    res = FileBrowser_CopyTree(g_fbClipPath, dst, sizeof(dst), g_fbClipIsDir, 0);

    if (R_FAILED(res))
    {
        // 新しく作ったコピー先は途中までの状態で残さない。
        // 上書き（既存のフォルダへの合流）のときは元に戻せないのでそのままにする。
        if (!existed)
            FileBrowser_Delete(dst, g_fbClipIsDir);
        FileBrowser_Notify(TR("貼り付け", "Paste"), Lang(STR_FAILED), false);
        return;
    }

    if (g_fbClipMode == FB_CLIP_CUT)
    {
        if (R_FAILED(FileBrowser_Delete(g_fbClipPath, g_fbClipIsDir)))
        {
            FileBrowser_Notify(TR("移動", "Move"), Lang(STR_COPIED_BUT_FAILED_TO_DELETE_THE_SOURCE), false);
            return;
        }
        g_fbClipMode = FB_CLIP_NONE;
        FileBrowser_Notify(TR("移動", "Move"), Lang(STR_MOVED), true);
    }
    else
    {
        FileBrowser_Notify(TR("コピー", "Copy"), Lang(STR_COPIED), true);
    }
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

#define FV_ROWS 18
#define FV_COLS ((SCREEN_BOT_WIDTH - 20) / SPACING_X)

static IFile g_fvFile;
static u64   g_fvSize;
static u64   g_fvWinStart;
static u32   g_fvWinLen;

static s32 FileView_Byte(u64 pos)
{
    if (pos >= g_fvSize)
        return -1;

    if (pos < g_fvWinStart || pos >= g_fvWinStart + g_fvWinLen)
    {
        u64 read = 0;

        g_fvWinStart = pos > MENU_WORK_BUF_SIZE / 2 ? (pos - MENU_WORK_BUF_SIZE / 2) & ~0xFFFULL : 0;
        g_fvWinLen = 0;
        g_fvFile.pos = g_fvWinStart;

        if (R_FAILED(IFile_Read(&g_fvFile, &read, g_menuWorkBuf, MENU_WORK_BUF_SIZE)))
            return -1;

        g_fvWinLen = (u32)read;
        if (pos >= g_fvWinStart + g_fvWinLen)
            return -1;
    }

    return g_menuWorkBuf[pos - g_fvWinStart];
}

static bool FileView_IsBinary(void)
{
    u32 n = g_fvSize < 512 ? (u32)g_fvSize : 512;
    u32 ctrl = 0;

    for (u32 i = 0; i < n; ++i)
    {
        s32 c = FileView_Byte(i);

        if (c == 0)
            return true;
        if ((c < 0x20 && c != '\t' && c != '\n' && c != '\r') || c == 0x7F)
            ++ctrl;
    }

    return ctrl * 16 > n;
}

static u64 FileView_TextRow(u64 pos, char *out)
{
    u32 n = 0;
    s32 c;

    while (n < FV_COLS && (c = FileView_Byte(pos)) >= 0)
    {
        ++pos;
        if (c == '\n')
            break;
        if (c == '\r' || (c & 0xC0) == 0x80)
            continue;
        if (out != NULL)
            out[n] = c >= 0x80 ? '?' : (c == '\t' ? ' ' : ((c < 0x20 || c == 0x7F) ? '.' : (char)c));
        ++n;
    }

    if (n == FV_COLS)
    {
        if (FileView_Byte(pos) == '\r' && FileView_Byte(pos + 1) == '\n')
            pos += 2;
        else if (FileView_Byte(pos) == '\n')
            ++pos;
    }

    if (out != NULL)
        out[n] = 0;

    return pos;
}

static u64 FileView_PrevTextRow(u64 pos)
{
    u64 start, row, next;
    u64 limit = pos > MENU_WORK_BUF_SIZE ? pos - MENU_WORK_BUF_SIZE : 0;

    if (pos == 0)
        return 0;

    start = pos - 1;
    while (start > limit && FileView_Byte(start - 1) != '\n')
        --start;

    row = start;
    while ((next = FileView_TextRow(row, NULL)) < pos && next > row)
        row = next;

    return row;
}

static u64 FileView_Move(u64 off, bool hex, bool down)
{
    if (hex)
    {
        if (down)
            return off + 8 < g_fvSize ? off + 8 : off;
        return off >= 8 ? off - 8 : 0;
    }

    if (down)
    {
        u64 next = FileView_TextRow(off, NULL);
        return next < g_fvSize ? next : off;
    }

    return FileView_PrevTextRow(off);
}

static void FileBrowser_View(const char *path)
{
    char line[FV_COLS + 1];
    u64  off = 0;
    bool hex;

    if (R_FAILED(IFile_Open(&g_fvFile, ARCHIVE_SDMC, fsMakePath(PATH_EMPTY, ""),
                            IFile_MakeUtf8Path(g_utf16Path, path), FS_OPEN_READ)))
    {
        FileBrowser_Notify(TR("表示", "View"), Lang(STR_FAILED), false);
        return;
    }

    g_fvSize = 0;
    g_fvWinStart = 0;
    g_fvWinLen = 0;
    IFile_GetSize(&g_fvFile, &g_fvSize);
    hex = FileView_IsBinary();

    do
    {
        Draw_Lock();
        Draw_ClearFramebuffer();
        Draw_ClipText(g_fbScratch, sizeof(g_fbScratch), FileBrowser_BaseName(path), FV_COLS);
        Draw_DrawString(10, 4, COLOR_TITLE, g_fbScratch);
        Draw_DrawFormattedString(10, 4 + SPACING_Y, COLOR_WHITE, TR("%s  %lu / %lu バイト", "%s  %lu / %lu bytes"),
                                 hex ? TR("16 進数", "Hex") : TR("テキスト", "Text"), (u32)off, (u32)g_fvSize);

        u32 y = 4 + 3 * SPACING_Y;
        u64 pos = off;

        if (g_fvSize == 0)
            Draw_DrawString(10, y, COLOR_WHITE, Lang(STR_EMPTY_FILE));

        for (u32 r = 0; r < FV_ROWS && pos < g_fvSize; ++r, y += SPACING_Y)
        {
            if (hex)
            {
                u32 n = sprintf(line, "%08lX ", (u32)pos);

                for (u32 i = 0; i < 8; ++i)
                {
                    s32 c = FileView_Byte(pos + i);
                    n += c >= 0 ? sprintf(line + n, " %02lX", (u32)c) : sprintf(line + n, "   ");
                }

                line[n++] = ' ';
                line[n++] = ' ';
                for (u32 i = 0; i < 8; ++i)
                {
                    s32 c = FileView_Byte(pos + i);
                    if (c >= 0)
                        line[n++] = (c >= 0x20 && c < 0x7F) ? (char)c : '.';
                }
                line[n] = 0;
                pos += 8;
            }
            else
                pos = FileView_TextRow(pos, line);

            Draw_DrawString(10, y, COLOR_WHITE, line);
        }

        Draw_DrawString(10, 214, COLOR_WHITE, Lang(STR_UP_DOWN_LINE_LEFT_RIGHT_PAGE));
        Draw_DrawString(10, 225, COLOR_WHITE, Lang(STR_X_TEXT_HEX_B_BACK));
        Draw_FlushFramebuffer();
        Draw_Unlock();

        u32 k = waitInput();
        if (terminationRequest || (k & BUTTON_B))
            break;

        if (k & BUTTON_X)
        {
            hex = !hex;
            off = hex ? off & ~7ULL : FileView_PrevTextRow(FileView_TextRow(off, NULL));
        }
        else if (k & (BUTTON_DOWN | BUTTON_UP))
            off = FileView_Move(off, hex, (k & BUTTON_DOWN) != 0);
        else if (k & (BUTTON_RIGHT | BUTTON_LEFT))
        {
            for (u32 r = 0; r < FV_ROWS; ++r)
                off = FileView_Move(off, hex, (k & BUTTON_RIGHT) != 0);
        }
    }
    while (!terminationRequest);

    IFile_Close(&g_fvFile);
}

void RosalinaMenu_FileBrowser(void)
{
    static char path[512];
    path[0] = '/';
    path[1] = 0;

    static const u32 sortNames[FB_SORT_COUNT] = { STR_SORT_ADDED, STR_SORT_NAME, STR_SORT_SIZE };

    g_fbBlock = 0;
    u32 count = FileBrowser_Load(path);
    s32 sel = 0, scroll = 0;
    const s32 rows = 15;

    menuEnter();
    do
    {
        bool atRoot = (path[0] == '/' && path[1] == 0);
        s32  up = (!atRoot && g_fbBlock == 0) ? 1 : 0;
        s32  total = (s32)count + up;
        u32  first = g_fbBlock * FB_MAX + (atRoot || g_fbBlock == 0 ? 0 : 1);

        if (sel >= total) sel = total > 0 ? total - 1 : 0;
        if (sel < 0) sel = 0;
        if (sel < scroll) scroll = sel;
        if (sel >= scroll + rows) scroll = sel - rows + 1;

        Draw_Lock();
        Draw_ClearFramebuffer();
        Draw_DrawString(10, 4, COLOR_TITLE, FB_TITLE);
        Draw_DrawFormattedString(10 + (Draw_TextColumns(FB_TITLE) + 2) * SPACING_X, 4, COLOR_WHITE, "< %s >  %lu/%lu   ", Lang(sortNames[g_fbSort]),
                                 sel < up ? 0 : g_fbBlock * FB_MAX + (u32)(sel - up) + 1, g_fbTotal);

        Draw_ClipText(g_fbScratch, sizeof(g_fbScratch), path, sizeof(g_fbScratch) - 1);
        const char *pshow = Draw_TextTail(g_fbScratch, 48);
        Draw_DrawFormattedString(10, 4 + SPACING_Y, COLOR_WHITE, "%s%s   ", pshow != g_fbScratch ? "..." : "", pshow);

        u32 y = 4 + 3 * SPACING_Y;
        for (s32 r = 0; r < rows; ++r)
        {
            s32 idx = scroll + r;
            if (idx >= total) break;

            u32 col = (idx == sel) ? COLOR_LIME : COLOR_WHITE;
            const char *cur = (idx == sel) ? ">" : " ";

            if (idx < up)
                Draw_DrawFormattedString(10, y, col, "%s [..]", cur);
            else
            {
                u32  e = g_fbOrder[idx - up];
                char name[DRAW_TEXT_SIZE(FB_NAME_COLS)];
                if (g_fbDir[e])
                {
                    Draw_ClipText(name, sizeof(name), g_fbName[e], FB_NAME_COLS - 2);
                    Draw_DrawFormattedString(10, y, col, "%s [%s]", cur, name);
                }
                else
                {
                    char sz[16];
                    u32  szLen = sprintf(sz, "  (%u KB)", (unsigned)(g_fbSize[e] >> 10));
                    Draw_ClipText(name, sizeof(name), g_fbName[e], FB_NAME_COLS - szLen);
                    Draw_DrawFormattedString(10, y, col, "%s %s%s", cur, name, sz);
                }
            }
            y += SPACING_Y;
        }
        Draw_DrawScrollMarks(4 + 3 * SPACING_Y, rows, first + scroll, g_fbTotal + (atRoot ? 0 : 1), COLOR_TITLE);

        const char *clipMode = Lang(g_fbClipMode == FB_CLIP_NONE ? STR_CLIP_EMPTY :
                                    (g_fbClipMode == FB_CLIP_CUT ? STR_CLIP_CUT : STR_CLIP_COPY));
        Draw_ClipText(g_fbScratch, sizeof(g_fbScratch), FileBrowser_BaseName(g_fbClipPath),
                      FB_NAME_COLS + 2 - Draw_TextColumns(FB_CLIP_LABEL) - Draw_TextColumns(clipMode));
        Draw_DrawFormattedString(10, 203, COLOR_WHITE, "%s%s%s   ", FB_CLIP_LABEL, clipMode,
            g_fbClipMode == FB_CLIP_NONE ? "" : g_fbScratch);
        Draw_DrawString(10, 214, COLOR_WHITE, Lang(STR_A_OPEN_X_COPY_Y_CUT_R_PASTE_LR_SORT));
        Draw_DrawString(10, 225, COLOR_WHITE, Lang(STR_SELECT_DELETE_START_RENAME_L_NEW_DIR));
        Draw_FlushFramebuffer();
        Draw_Unlock();

        u32 k = waitInput();
        if (terminationRequest)
            break;

        if (k & BUTTON_DOWN)
        {
            if (sel + 1 < total)
                ++sel;
            else if ((g_fbBlock + 1) * FB_MAX < g_fbTotal)
            {
                ++g_fbBlock;
                count = FileBrowser_Load(path); sel = 0; scroll = 0;
            }
        }
        else if (k & BUTTON_UP)
        {
            if (sel > 0)
                --sel;
            else if (g_fbBlock > 0)
            {
                --g_fbBlock;
                count = FileBrowser_Load(path);
                sel = (s32)count + ((!atRoot && g_fbBlock == 0) ? 1 : 0) - 1;
            }
        }
        else if (k & (BUTTON_LEFT | BUTTON_RIGHT))
        {
            g_fbSort = (g_fbSort + ((k & BUTTON_RIGHT) ? 1 : FB_SORT_COUNT - 1)) % FB_SORT_COUNT;
            g_fbBlock = 0;
            count = FileBrowser_Load(path); sel = 0; scroll = 0;
        }
        else if (k & BUTTON_A)
        {
            if (sel < up)
            {
                FileBrowser_Up(path);
                g_fbBlock = 0;
                count = FileBrowser_Load(path); sel = 0; scroll = 0;
            }
            else if (sel - up < (s32)count)
            {
                u32 e = g_fbOrder[sel - up];
                char full[512];

                if (!FileBrowser_GetName(path, e) ||
                    (!g_fbDir[e] && !FileBrowser_Join(full, sizeof(full), path, g_fbScratch)))
                {
                    FileBrowser_Notify(FB_TITLE, Lang(STR_FAILED), false);
                    count = FileBrowser_Load(path);
                    continue;
                }

                if (!g_fbDir[e])
                    FileBrowser_View(full);
                else
                {
                    u32 l = 0; while (path[l]) ++l;
                    const char *nm = g_fbScratch;
                    u32 nl = 0; while (nm[nl]) ++nl;
                    if (l + nl + 2 < sizeof(path))
                    {
                        for (u32 i = 0; i < nl; ++i) path[l + i] = nm[i];
                        path[l + nl] = '/';
                        path[l + nl + 1] = 0;
                        g_fbBlock = 0;
                        count = FileBrowser_Load(path); sel = 0; scroll = 0;
                    }
                }
            }
        }
        else if (k & BUTTON_B)
        {
            if (atRoot) break;
            FileBrowser_Up(path);
            g_fbBlock = 0;
            count = FileBrowser_Load(path); sel = 0; scroll = 0;
        }
        else if (k & BUTTON_R1)
        {
            FileBrowser_Paste(path);
            count = FileBrowser_Load(path);
        }
        else if (k & BUTTON_L1)
        {
            char name[64];

            if (TextInput_Prompt(Lang(STR_NEW_FOLDER_NAME), name, sizeof(name), NULL))
            {
                char full[512];
                FileBrowser_Join(full, sizeof(full), path, name);

                if (R_SUCCEEDED(FileBrowser_MakeDir(full)))
                    count = FileBrowser_Load(path);
                else
                    FileBrowser_Notify(TR("新しいフォルダ", "New folder"), Lang(STR_FAILED), false);
            }
        }
        else if (k & (BUTTON_X | BUTTON_Y | BUTTON_SELECT | BUTTON_START))
        {
            if (sel < up || sel - up >= (s32)count)
                continue;

            u32 e = g_fbOrder[sel - up];
            char full[512];
            bool isDir = g_fbDir[e];
            if (!FileBrowser_GetName(path, e) || !FileBrowser_Join(full, sizeof(full), path, g_fbScratch))
            {
                FileBrowser_Notify(FB_TITLE, Lang(STR_FAILED), false);
                count = FileBrowser_Load(path);
                continue;
            }

            if (k & (BUTTON_X | BUTTON_Y))
            {
                u32 i = 0;
                while (full[i] != 0 && i + 1 < sizeof(g_fbClipPath))
                {
                    g_fbClipPath[i] = full[i];
                    ++i;
                }
                while (i > 1 && g_fbClipPath[i - 1] == '/')
                    --i;
                g_fbClipPath[i] = 0;
                g_fbClipIsDir = isDir;
                g_fbClipMode = (k & BUTTON_X) ? FB_CLIP_COPY : FB_CLIP_CUT;
            }
            else if (k & BUTTON_SELECT)
            {
                if (ConfirmPrompt(TR("削除", "Delete"), Lang(isDir ? STR_DELETE_THIS_FOLDER : STR_DELETE_THIS_FILE), g_fbName[e]))
                {
                    if (R_SUCCEEDED(FileBrowser_Delete(full, isDir)))
                        count = FileBrowser_Load(path);
                    else
                        FileBrowser_Notify(TR("削除", "Delete"), Lang(STR_FAILED), false);
                }
            }
            else
            {
                char name[64];
                const char *initial = g_fbScratch;

                // 英数字以外を含む名前や長い名前は、? 置き換え・切り詰め済みの表示名しか
                // 入力欄に入れられず、そのまま確定すると名前が壊れる。その場合は空欄から入力する
                for (u32 i = 0; initial != NULL && g_fbScratch[i] != 0; ++i)
                    if ((u8)g_fbScratch[i] >= 0x80 || i + 1 >= sizeof(name))
                        initial = NULL;

                if (TextInput_Prompt(Lang(STR_RENAME), name, sizeof(name), initial))
                {
                    char dst[512];
                    FileBrowser_Join(dst, sizeof(dst), path, name);

                    if (R_SUCCEEDED(FileBrowser_Rename(full, dst, isDir)))
                        count = FileBrowser_Load(path);
                    else
                        FileBrowser_Notify(TR("名前の変更", "Rename"), Lang(STR_FAILED), false);
                }
            }
        }
    }
    while (!terminationRequest);
    menuLeave();
}

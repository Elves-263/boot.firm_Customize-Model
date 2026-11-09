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

#include <3ds.h>
#include <arpa/inet.h>
#include <string.h>
#include "update_check.h"
#include "update_tls.h"
#include "minisoc.h"
#include "sock_util.h"
#include "sleep.h"
#include "ifile.h"
#include "menu.h"
#include "draw.h"
#include "fmt.h"
#include "utils.h"
#include "menus/lang.h"

#ifndef UPDATE_REPO
#define UPDATE_REPO ""
#endif

#define UPDATE_HOST             "github.com"
#define UPDATE_OFF_PATH         UPDATE_DIR "/off.flag"
#define UPDATE_RUNNING_PATH     UPDATE_DIR "/running.txt"
#define UPDATE_CHECK_PATH       UPDATE_DIR "/check.txt"
#define UPDATE_NOTIFIED_PATH    UPDATE_DIR "/notified.txt"

#define UPDATE_MEM_ADDR         0x0E800000
#define UPDATE_MEM_SIZE         ((sizeof(UpdateCheckMem) + 0xFFF) & ~0xFFF)

#define UPDATE_SECOND           (1000 * 1000 * 1000LL)
#define UPDATE_FIRST_WAIT       (20 * UPDATE_SECOND)        // 起動直後は HOME メニューが落ち着くまで待つ
#define UPDATE_WIFI_POLL        (30 * UPDATE_SECOND)        // Wi-Fi に繋がるのを待つ間隔
#define UPDATE_RETRY_WAIT       (10 * 60 * UPDATE_SECOND)   // 通信に失敗したらやり直すまでの時間
#define UPDATE_MAX_TRIES        3
#define UPDATE_IO_TIMEOUT_MS    15000

// UPDATE_MEM_ADDR（ページの境界）に置くので tls は 8 バイト境界になる
typedef struct UpdateCheckMem
{
    u8  tls[UPDATE_TLS_MEM_SIZE];
    u8  hostent[SOC_HOSTENT_BUFFER_SIZE];
} UpdateCheckMem;

// check.txt の result= に書く名前（UpdateCheckResult の順）
typedef enum UpdateCheckResult
{
    UPDCHK_OK,
    UPDCHK_NO_RELEASE,
    UPDCHK_HTTP,
    UPDCHK_TLS,
    UPDCHK_DNS,
    UPDCHK_CONNECT,
    UPDCHK_SOCKET,
    UPDCHK_MEMORY,
} UpdateCheckResult;

static const char *const g_resultNames[] =
{
    "ok", "no_release", "http", "tls", "dns", "connect", "socket", "memory",
};

static MyThread updateCheckThread;
static u8 CTR_ALIGN(8) updateCheckThreadStack[0x2000];

static char g_updateLatest[UPDATE_TAG_SIZE];
static volatile bool g_updateNoticePending;

// ns だけ待つ。その間に終了を頼まれたら false
static bool UpdateCheck_Wait(s64 ns)
{
    return R_DESCRIPTION(svcWaitSynchronization(preTerminationEvent, ns)) == RD_TIMEOUT;
}

// ---------------------------------------------------------------- SD カードのファイル

// 中身を buf に読み、終端を付けてバイト数を返す。無ければ 0
static u32 UpdateCheck_ReadFile(const char *path, char *buf, u32 size)
{
    IFile f;
    u64   total = 0;

    if (R_SUCCEEDED(IFile_Open(&f, ARCHIVE_SDMC, fsMakePath(PATH_EMPTY, ""), fsMakePath(PATH_ASCII, path), FS_OPEN_READ)))
    {
        if (R_FAILED(IFile_Read(&f, &total, buf, size - 1)))
            total = 0;
        IFile_Close(&f);
    }

    buf[total] = 0;
    return (u32)total;
}

static bool UpdateCheck_FileExists(const char *path)
{
    IFile f;

    if (R_FAILED(IFile_Open(&f, ARCHIVE_SDMC, fsMakePath(PATH_EMPTY, ""), fsMakePath(PATH_ASCII, path), FS_OPEN_READ)))
        return false;
    IFile_Close(&f);
    return true;
}

// 中身を data に置き換える（/luma/update が無ければ作る）
static bool UpdateCheck_WriteFile(const char *path, const char *data, u32 len)
{
    FS_Archive  archive;
    IFile       f;
    u64         total = 0;
    bool        ok;

    if (R_SUCCEEDED(FSUSER_OpenArchive(&archive, ARCHIVE_SDMC, fsMakePath(PATH_EMPTY, ""))))
    {
        FSUSER_CreateDirectory(archive, fsMakePath(PATH_ASCII, UPDATE_DIR), 0); // 既にあれば失敗するだけ
        FSUSER_CloseArchive(archive);
    }

    if (R_FAILED(IFile_Open(&f, ARCHIVE_SDMC, fsMakePath(PATH_EMPTY, ""), fsMakePath(PATH_ASCII, path), FS_OPEN_CREATE | FS_OPEN_WRITE)))
        return false;

    ok = R_SUCCEEDED(IFile_SetSize(&f, 0)) && R_SUCCEEDED(IFile_Write(&f, &total, data, len, FS_WRITE_FLUSH)) && total == len;
    IFile_Close(&f);
    return ok;
}

// 動いている版を更新アプリに知らせる。同じなら書かない（起動のたびに SD に書かないため）
static void UpdateCheck_WriteRunning(void)
{
    static const char running[] = FORK_PACKAGE_TAG "\n" FORK_LUMA_ID "\n";
    char old[sizeof(running) + 1];

    if (UpdateCheck_ReadFile(UPDATE_RUNNING_PATH, old, sizeof(old)) != sizeof(running) - 1 || strcmp(old, running) != 0)
        UpdateCheck_WriteFile(UPDATE_RUNNING_PATH, running, sizeof(running) - 1);
}

static void UpdateCheck_WriteResult(UpdateCheckResult res, int code, const char *tag)
{
    char buf[128];
    int  n = sprintf(buf, "time=%llu\nresult=%s\ncode=%d\nlatest=%s\n", osGetTime(), g_resultNames[res], code, tag);

    UpdateCheck_WriteFile(UPDATE_CHECK_PATH, buf, (u32)n);
}

// ---------------------------------------------------------------- 通信

static int UpdateCheck_SockRead(void *ctx, unsigned char *buf, size_t len)
{
    int             fd = *(int *)ctx;
    struct pollfd   pfd = { fd, POLLIN, 0 };
    ssize_t         n;

    if (socPoll(&pfd, 1, UPDATE_IO_TIMEOUT_MS) <= 0 || preTerminationRequested)
        return -1;

    n = socRecv(fd, buf, len < 0x1000 ? len : 0x1000, 0);
    return n > 0 ? (int)n : -1;
}

static int UpdateCheck_SockWrite(void *ctx, const unsigned char *buf, size_t len)
{
    ssize_t n = socSend(*(int *)ctx, buf, len, 0);
    return n > 0 ? (int)n : -1;
}

// TLS の乱数の種。ps:ps の乱数に、取れなかったときのために時刻を混ぜる
static void UpdateCheck_MakeSeed(u8 *seed, u32 size)
{
    u64 tick = svcGetSystemTick();

    memset(seed, 0, size);
    memcpy(seed, &tick, sizeof(tick));
    if (R_SUCCEEDED(psInit()))
    {
        PS_GenerateRandomBytes(seed + sizeof(tick), size - sizeof(tick));
        psExit();
    }
}

static UpdateCheckResult UpdateCheck_Run(char *tag, int *code)
{
    UpdateCheckMem      *mem = (UpdateCheckMem *)UPDATE_MEM_ADDR;
    UpdateCheckResult   res = UPDCHK_SOCKET;
    struct sockaddr_in  addr;
    u8                  seed[48];
    u32                 tmp, ip;
    int                 fd;

    *code = 0;
    tag[0] = 0;

    if (R_FAILED(svcControlMemoryEx(&tmp, UPDATE_MEM_ADDR, 0, UPDATE_MEM_SIZE, MEMOP_ALLOC | MEMOP_REGION_SYSTEM,
                                    MEMPERM_READ | MEMPERM_WRITE, true)))
        return UPDCHK_MEMORY;

    if (R_SUCCEEDED(*code = miniSocInit()))
    {
        if (socGethostbyname(UPDATE_HOST, &ip, mem->hostent) != 0)
            res = UPDCHK_DNS;
        else if ((fd = socSocket(AF_INET, SOCK_STREAM, 0)) < 0)
            res = UPDCHK_SOCKET;
        else
        {
            memset(&addr, 0, sizeof(addr));
            addr.sin_family = AF_INET;
            addr.sin_port = htons(443);
            addr.sin_addr.s_addr = ip;

            if (socConnect(fd, (struct sockaddr *)&addr, sizeof(addr)) != 0)
                res = UPDCHK_CONNECT;
            else
            {
                UpdateTlsIo io = { UpdateCheck_SockRead, UpdateCheck_SockWrite, &fd };

                UpdateCheck_MakeSeed(seed, sizeof(seed));
                switch (UpdateTls_GetLatestTag(mem->tls, &io, UPDATE_HOST, UPDATE_REPO, "Luma3DS_New/" FORK_PACKAGE_TAG,
                                               seed, sizeof(seed), tag, code))
                {
                    case UPDATE_TLS_OK:         res = UPDCHK_OK; break;
                    case UPDATE_TLS_ERR_HTTP:   res = UPDCHK_HTTP; break;
                    case UPDATE_TLS_ERR_NOTAG:  res = UPDCHK_NO_RELEASE; break;
                    default:                    res = UPDCHK_TLS; break;
                }
            }
            socClose(fd);
        }
        miniSocExit();
    }

    svcControlMemory(&tmp, UPDATE_MEM_ADDR, 0, UPDATE_MEM_SIZE, MEMOP_FREE, MEMPERM_DONTCARE);
    return res;
}

// ---------------------------------------------------------------- 知らせる

// お知らせリストに送る。差出人を更新アプリにして、お知らせから更新アプリを開けるようにする
static Result UpdateCheck_AddNews(const char *title, const char *message)
{
    NotificationHeader  header;
    u16                 message16[256];
    Handle              handle;
    ssize_t             units;
    Result              res;
    u32                 *cmdbuf;

    memset(&header, 0, sizeof(header));
    header.dataSet = true;
    header.unread = true;
    header.processID = UPDATER_TITLE_ID;
    header.time = osGetTime();
    utf8_to_utf16(header.title, (const u8 *)title, sizeof(header.title) / sizeof(u16) - 1);

    units = utf8_to_utf16(message16, (const u8 *)message, sizeof(message16) / sizeof(u16) - 1);
    message16[units < 0 ? 0 : units] = 0;
    units = units < 0 ? 0 : units;

    if (R_FAILED(res = srvGetServiceHandle(&handle, "news:s")))
        return res;

    cmdbuf = getThreadCommandBuffer();
    cmdbuf[0] = IPC_MakeHeader(0x1, 3, 6); // 0x100C6 AddNotification
    cmdbuf[1] = sizeof(header);
    cmdbuf[2] = (units + 1) * sizeof(u16);
    cmdbuf[3] = 0;
    cmdbuf[4] = IPC_Desc_Buffer(sizeof(header), IPC_BUFFER_R);
    cmdbuf[5] = (u32)&header;
    cmdbuf[6] = IPC_Desc_Buffer((units + 1) * sizeof(u16), IPC_BUFFER_R);
    cmdbuf[7] = (u32)message16;
    cmdbuf[8] = IPC_Desc_Buffer(0, IPC_BUFFER_R);
    cmdbuf[9] = 0;

    res = svcSendSyncRequest(handle);
    if (R_SUCCEEDED(res))
        res = (Result)cmdbuf[1];
    svcCloseHandle(handle);
    return res;
}

static void UpdateCheck_Notify(const char *tag)
{
    char notified[UPDATE_TAG_SIZE + 2];
    char message[320];

    memcpy(g_updateLatest, tag, sizeof(g_updateLatest));
    g_updateNoticePending = true;

    UpdateCheck_ReadFile(UPDATE_NOTIFIED_PATH, notified, sizeof(notified));
    if (strcmp(notified, tag) == 0)
        return;

    sprintf(message, TR("Luma3DS_New の新しい版 %s が出ています（今の版は %s）。\n\n"
                        "HOME メニューの「Luma3DS_New 更新」で、更新内容の確認と更新ができます。",
                        "Luma3DS_New %s is available (you have %s).\n\n"
                        "Open \"Luma3DS_New Updater\" on the HOME Menu to see what changed and update."),
            tag, FORK_PACKAGE_TAG);
    // お知らせリストには文字列のまま渡すので、ここで言語を選ぶ
    if (R_SUCCEEDED(UpdateCheck_AddNews(Lang_Str(TR("Luma3DS_New の新しい版", "New version of Luma3DS_New")), message)))
        UpdateCheck_WriteFile(UPDATE_NOTIFIED_PATH, tag, strlen(tag));
}

void UpdateCheck_ShowNotice(void)
{
    if (!g_updateNoticePending)
        return;
    g_updateNoticePending = false;

    do
    {
        Draw_Lock();
        Draw_ClearFramebuffer();
        Draw_DrawString(10, 10, COLOR_TITLE, TR("Luma3DS_New の新しい版", "New version of Luma3DS_New"));
        Draw_DrawFormattedString(10, 30, COLOR_WHITE, TR("新しい版:  %s", "New version:     %s"), g_updateLatest);
        Draw_DrawString(10, 30 + SPACING_Y, COLOR_WHITE, TR("今の版:    " FORK_PACKAGE_TAG, "Current version: " FORK_PACKAGE_TAG));
        Draw_DrawString(10, 30 + 3 * SPACING_Y, COLOR_WHITE, TR("HOME メニューの「Luma3DS_New 更新」で、\n更新内容の確認と更新ができます。", "Open \"Luma3DS_New Updater\" on the HOME Menu\nto see what changed and update."));
        Draw_DrawString(10, 30 + 6 * SPACING_Y, COLOR_WHITE, TR("B: 閉じる", "B: close"));
        Draw_FlushFramebuffer();
        Draw_Unlock();
    }
    while (!(waitInput() & KEY_B) && !menuShouldExit);
}

// ---------------------------------------------------------------- スレッド

static void updateCheckThreadMain(void)
{
    char tag[UPDATE_TAG_SIZE];
    int  code;

    // SD カードを読めるようになるまで待つ（ac:u が登録されるのはその後）
    while (!isServiceUsable("ac:u"))
        if (!UpdateCheck_Wait(UPDATE_SECOND))
            return;

    UpdateCheck_WriteRunning();
    if (UpdateCheck_FileExists(UPDATE_OFF_PATH) || !UpdateCheck_Wait(UPDATE_FIRST_WAIT))
        return;

    for (u32 tries = 0; tries < UPDATE_MAX_TRIES; ++tries)
    {
        // 蓋を閉じている間は開くまで待つ
        while (Sleep__Status() || !Wifi__IsConnected())
            if (!UpdateCheck_Wait(UPDATE_WIFI_POLL))
                return;

        UpdateCheckResult res = UpdateCheck_Run(tag, &code);
        UpdateCheck_WriteResult(res, code, tag);

        if (res == UPDCHK_OK && UpdateVersion_Compare(tag, FORK_PACKAGE_TAG) > 0)
            UpdateCheck_Notify(tag);

        // リリースが無い・リポジトリが違う（404 など）ときは、やり直しても同じ
        if (res == UPDCHK_OK || res == UPDCHK_NO_RELEASE || (res == UPDCHK_HTTP && code < 500))
            return;

        if (!UpdateCheck_Wait(UPDATE_RETRY_WAIT))
            return;
    }
}

MyThread *updateCheckCreateThread(void)
{
    if (UPDATE_REPO[0] == 0)
        return NULL;
    if (R_FAILED(MyThread_Create(&updateCheckThread, updateCheckThreadMain, updateCheckThreadStack,
                                 sizeof(updateCheckThreadStack), 0x3F, CORE_SYSTEM)))
        return NULL;
    return &updateCheckThread;
}

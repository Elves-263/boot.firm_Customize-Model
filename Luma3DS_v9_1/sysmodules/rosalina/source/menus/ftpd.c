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
#include "menus/ftpd.h"
#include "menus/lang.h"
#include "minisoc.h"
#include "draw.h"
#include "font_jp.h"
#include "fmt.h"
#include "ifile.h"
#include "memory.h"
#include "utils.h"
#include "csvc.h"

#define FTPD_CMD_MAX       1024
#define FTPD_PATH_MAX      512
#define FTPD_XFER_SIZE     0x8000
#define FTPD_LIST_MAX      1024
#define FTPD_MAX_SESSIONS  4
#define FTPD_DATA_TIMEOUT  10
#define FTPD_MS_1900_2000  3155673600000ULL
#define FTPD_RECENT_MS     (180ULL * 24 * 60 * 60 * 1000)
#define FTPD_MAX_MS        (8000ULL * 365 * 24 * 60 * 60 * 1000)
#define FTPD_STACK_SIZE    0x2000
#define FTPD_STATE_ADDR    0x0E000000
#define FTPD_STATE_SIZE    ((sizeof(FtpdState) + 0xFFF) & ~0xFFF)
#define FTPD_LINE_COLS     ((SCREEN_BOT_WIDTH - 20) / SPACING_X)
#define FTPD_NAME_COLS     19          // 転送中のファイル名を出す桁数
#define FTPD_NO_TRUNCATE   (~0ULL)

enum
{
    FTPD_IDLE,
    FTPD_LIST,
    FTPD_NLST,
    FTPD_RETR,
    FTPD_STOR,
};

typedef struct FtpdSession
{
    int         ctrl;
    int         pasv;
    int         data;
    u32         ip;
    u32         state;
    u64         deadline;
    u64         rest;
    u64         size;
    u64         startTick;
    u64         startPos;
    u64         truncateAt;
    bool        created;
    IFile       file;
    Handle      dir;
    FS_Archive  archive;
    u32         cmdLen;
    char        shown[40];
    char        cwd[FTPD_PATH_MAX];
    char        renameFrom[FTPD_PATH_MAX];
    char        xferPath[FTPD_PATH_MAX];
    char        cmd[FTPD_CMD_MAX];
} FtpdSession;

typedef struct FtpdState
{
    u8          stack[FTPD_STACK_SIZE];
    u8          xfer[FTPD_XFER_SIZE];
    FtpdSession sessions[FTPD_MAX_SESSIONS];
    char        line[FTPD_CMD_MAX];
    char        scratch[FTPD_PATH_MAX];
    char        reply[FTPD_PATH_MAX + 64];
    u16         pathW[IFILE_PATH_UNITS];
    u16         pathW2[IFILE_PATH_UNITS];
} FtpdState;

bool ftpdEnabled = false;

static MyThread ftpdThread;

static int  g_listenSock  = -1;

static FtpdState *g_state;

static u32  g_transfers;

// ---------------------------------------------------------------- utilities

static bool ftpdWait(int sock, int events)
{
    struct pollfd pfd;

    pfd.fd = sock;
    pfd.events = events;

    for (u32 tries = 0; ftpdEnabled && tries < 60; ++tries)
    {
        pfd.revents = 0;

        int pr = socPoll(&pfd, 1, 500);
        if (pr != 0)
            return pr > 0;
    }

    return false;
}

static bool ftpdReadable(int sock)
{
    struct pollfd pfd;

    pfd.fd = sock;
    pfd.events = POLLIN;
    pfd.revents = 0;

    return socPoll(&pfd, 1, 0) > 0;
}

static bool ftpdSend(int sock, const void *buf, u32 len)
{
    u32 sent = 0;

    while (sent < len)
    {
        if (!ftpdWait(sock, POLLOUT))
            return false;

        ssize_t n = soc_send(sock, (const u8 *)buf + sent, len - sent, 0);
        if (n <= 0)
            return false;
        sent += (u32)n;
    }

    return true;
}

static void ftpdReply(int sock, const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    vsprintf(g_state->reply, fmt, args);
    va_end(args);

    ftpdSend(sock, g_state->reply, strlen(g_state->reply));
}

static bool ftpdIsRoot(const char *path)
{
    return path[0] == '/' && path[1] == 0;
}

static const char *ftpdBaseName(const char *path)
{
    const char *base = path;

    for (const char *p = path; *p != 0; ++p)
        if (*p == '/' && p[1] != 0)
            base = p + 1;

    return base;
}

// FTP のパスを絶対パスへ正規化する。".." と "." を畳んで、
// 末尾スラッシュは（ルート以外）落とす。SD の外へは出さない。
// 途中で切り詰めると別のパスになってしまうため、収まらないときは失敗にする。
static bool ftpdResolve(char *out, u32 outSize, const char *base, const char *arg)
{
    char tmp[FTPD_PATH_MAX * 2];
    u32 n = 0;

    if (arg == NULL || arg[0] == 0)
        arg = ".";

    if (arg[0] == '/')
    {
        tmp[n++] = '/';
    }
    else
    {
        for (u32 i = 0; base[i] != 0; ++i)
        {
            if (n + 1 >= sizeof(tmp))
                return false;
            tmp[n++] = base[i];
        }
        if (n == 0 || tmp[n - 1] != '/')
        {
            if (n + 1 >= sizeof(tmp))
                return false;
            tmp[n++] = '/';
        }
    }

    for (u32 i = 0; arg[i] != 0; ++i)
    {
        if (n + 1 >= sizeof(tmp))
            return false;
        tmp[n++] = arg[i];
    }
    tmp[n] = 0;

    // 成分ごとに畳む
    u32 outLen = 0;
    out[outLen++] = '/';

    u32 i = 0;
    while (tmp[i] != 0)
    {
        while (tmp[i] == '/')
            ++i;
        if (tmp[i] == 0)
            break;

        u32 start = i;
        while (tmp[i] != 0 && tmp[i] != '/')
            ++i;
        u32 segLen = i - start;

        if (segLen == 1 && tmp[start] == '.')
            continue;

        if (segLen == 2 && tmp[start] == '.' && tmp[start + 1] == '.')
        {
            while (outLen > 1 && out[outLen - 1] != '/')
                --outLen;
            if (outLen > 1)
                --outLen;
            if (outLen == 0)
                outLen = 1;
            continue;
        }

        if (outLen > 1)
        {
            if (outLen + 1 >= outSize)
                return false;
            out[outLen++] = '/';
        }

        if (outLen + segLen >= outSize)
            return false;

        for (u32 k = 0; k < segLen; ++k)
            out[outLen++] = tmp[start + k];
    }

    out[outLen] = 0;
    return true;
}

static Result ftpdOpenArchive(FS_Archive *archive)
{
    return FSUSER_OpenArchive(archive, ARCHIVE_SDMC, fsMakePath(PATH_EMPTY, ""));
}

static bool ftpdStat(const char *path, bool *isDir, u64 *size)
{
    FS_Archive archive = 0;

    *isDir = false;
    *size = 0;

    if (ftpdIsRoot(path))
    {
        *isDir = true;
        return true;
    }

    if (R_FAILED(ftpdOpenArchive(&archive)))
        return false;

    Handle h = 0;
    bool found = false;
    FS_Path fsPath = IFile_MakeUtf8Path(g_state->pathW, path);

    if (R_SUCCEEDED(FSUSER_OpenDirectory(&h, archive, fsPath)))
    {
        FSDIR_Close(h);
        *isDir = true;
        found = true;
    }
    else
    {
        IFile file;
        if (R_SUCCEEDED(IFile_Open(&file, ARCHIVE_SDMC, fsMakePath(PATH_EMPTY, ""), fsPath, FS_OPEN_READ)))
        {
            IFile_GetSize(&file, size);
            IFile_Close(&file);
            found = true;
        }
    }

    FSUSER_CloseArchive(archive);
    return found;
}

static bool ftpdIsLeap(u32 year)
{
    return year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
}

static void ftpdCivil(u64 ms, u32 *t)
{
    static const u8 monthDays[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
    u64 secs = ms / 1000;
    u32 days = (u32)(secs / 86400);
    u32 rem = (u32)(secs % 86400);
    u32 year = 2000, month = 0;

    for (;;)
    {
        u32 n = ftpdIsLeap(year) ? 366 : 365;
        if (days < n)
            break;
        days -= n;
        ++year;
    }

    for (;;)
    {
        u32 n = monthDays[month] + ((month == 1 && ftpdIsLeap(year)) ? 1 : 0);
        if (days < n)
            break;
        days -= n;
        ++month;
    }

    t[0] = year;
    t[1] = month;
    t[2] = days + 1;
    t[3] = rem / 3600;
    t[4] = (rem / 60) % 60;
    t[5] = rem % 60;
}

static void ftpdFormatListTime(char *out, u64 ms)
{
    static const char *const months[12] =
    {
        "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
    };
    u64 now = osGetTime() - FTPD_MS_1900_2000;
    u32 t[6];

    ftpdCivil(ms, t);

    if (ms <= now && now - ms < FTPD_RECENT_MS)
        sprintf(out, "%s %2lu %02lu:%02lu", months[t[1]], t[2], t[3], t[4]);
    else
        sprintf(out, "%s %2lu  %4lu", months[t[1]], t[2], t[0]);
}

static bool ftpdGetTime(FS_Archive archive, u32 units, u64 *ms)
{
    return R_SUCCEEDED(FSUSER_ControlArchive(archive, ARCHIVE_ACTION_GET_TIMESTAMP, g_state->pathW, (units + 1) * 2,
                                             ms, sizeof(*ms))) && *ms < FTPD_MAX_MS;
}

static bool ftpdEntryTime(FS_Archive archive, u32 baseUnits, const u16 *name, u64 *ms)
{
    u32 n = baseUnits;

    if (n > 1)
        g_state->pathW[n++] = '/';

    for (u32 i = 0; name[i] != 0; ++i)
    {
        if (n + 1 >= IFILE_PATH_UNITS)
            return false;
        g_state->pathW[n++] = name[i];
    }
    g_state->pathW[n] = 0;

    return ftpdGetTime(archive, n, ms);
}

// ---------------------------------------------------------------- data conn

static void ftpdCloseData(FtpdSession *s)
{
    if (s->data >= 0)
    {
        socClose(s->data);
        s->data = -1;
    }
    if (s->pasv >= 0)
    {
        socClose(s->pasv);
        s->pasv = -1;
    }
}

// minisoc には getsockname が無いため、PASV のポートは自前で決めて bind する。
static u16 g_pasvPort = FTPD_PORT + 1;

static bool ftpdOpenPasvFixed(FtpdSession *s)
{
    struct sockaddr_in saddr;

    ftpdCloseData(s);

    for (u32 attempt = 0; attempt < 16; ++attempt)
    {
        g_pasvPort++;
        if (g_pasvPort < FTPD_PORT + 1 || g_pasvPort > FTPD_PORT + 32)
            g_pasvPort = FTPD_PORT + 1;

        s->pasv = socSocket(AF_INET, SOCK_STREAM, 0);
        if (s->pasv < 0)
            return false;

        memset(&saddr, 0, sizeof(saddr));
        saddr.sin_family = AF_INET;
        saddr.sin_port = htons(g_pasvPort);
        saddr.sin_addr.s_addr = gethostid();

        if (socBind(s->pasv, (struct sockaddr *)&saddr, sizeof(saddr)) == 0 &&
            socListen(s->pasv, 1) == 0)
            return true;

        socClose(s->pasv);
        s->pasv = -1;
    }

    return false;
}

static void ftpdStopTransfer(FtpdSession *s)
{
    if (s->state == FTPD_RETR || s->state == FTPD_STOR)
    {
        if (s->state == FTPD_STOR)
            FSFILE_Flush(s->file.handle);
        IFile_Close(&s->file);
        ++g_transfers;
    }
    else if (s->state != FTPD_IDLE)
    {
        FSDIR_Close(s->dir);
        FSUSER_CloseArchive(s->archive);
    }

    s->state = FTPD_IDLE;
    ftpdCloseData(s);
}

static void ftpdEndTransfer(FtpdSession *s, const char *reply)
{
    ftpdStopTransfer(s);
    ftpdReply(s->ctrl, reply);
}

// データ接続がつながらなかったときの終了。STOR でこちらが作っただけの
// 空のファイルは残さない（既存のファイルには、つながるまで手を付けていない）。
static void ftpdFailDataConn(FtpdSession *s)
{
    bool removeFile = s->state == FTPD_STOR && s->created;
    FS_Archive archive = 0;

    ftpdEndTransfer(s, "425 Cannot open data connection.\r\n");

    if (removeFile && R_SUCCEEDED(ftpdOpenArchive(&archive)))
    {
        FSUSER_DeleteFile(archive, IFile_MakeUtf8Path(g_state->pathW, s->xferPath));
        FSUSER_CloseArchive(archive);
    }
}

static void ftpdBeginTransfer(FtpdSession *s, u32 state, const char *reply)
{
    s->state = state;
    s->rest = 0;
    s->startTick = svcGetSystemTick();
    s->startPos = s->file.pos;
    s->deadline = s->startTick + (u64)FTPD_DATA_TIMEOUT * SYSCLOCK_ARM11;
    FontJp_CopyUtf8(s->shown, sizeof(s->shown), ftpdBaseName(s->xferPath));

    ftpdReply(s->ctrl, reply);

    if (s->pasv < 0 && s->data < 0)
        ftpdFailDataConn(s);
}

static void ftpdStepList(FtpdSession *s)
{
    FS_Path base = IFile_MakeUtf8Path(g_state->pathW, s->xferPath);
    bool haveBase = base.type == PATH_UTF16;
    u32 baseUnits = haveBase ? base.size / 2 - 1 : 0;
    u32 len = 0;
    bool more = true;

    while (len + FTPD_LIST_MAX <= sizeof(g_state->xfer))
    {
        FS_DirectoryEntry entry;
        u32 n = 0;

        if (R_FAILED(FSDIR_Read(s->dir, &n, 1, &entry)) || n == 0)
        {
            more = false;
            break;
        }

        char *line = (char *)g_state->xfer + len;
        u32 prefix = 0;

        if (s->state == FTPD_LIST)
        {
            bool isDir = (entry.attributes & 1) != 0;
            char date[16];
            u64 ms = 0;

            if (haveBase && ftpdEntryTime(s->archive, baseUnits, entry.name, &ms))
                ftpdFormatListTime(date, ms);
            else
                strcpy(date, "Jan  1  2000");

            prefix = (u32)sprintf(line, "%crw-rw-rw- 1 3ds 3ds %llu %s ",
                                  isDir ? 'd' : '-', isDir ? 0ULL : entry.fileSize, date);
        }

        int units = utf16_to_utf8((u8 *)line + prefix, entry.name, FTPD_LIST_MAX - prefix - 2);
        if (units < 0 || (u32)units > FTPD_LIST_MAX - prefix - 2)
            continue;

        line[prefix + units] = '\r';
        line[prefix + units + 1] = '\n';
        len += prefix + units + 2;
    }

    if (len != 0 && !ftpdSend(s->data, g_state->xfer, len))
        ftpdEndTransfer(s, "426 Transfer aborted.\r\n");
    else if (!more)
        ftpdEndTransfer(s, "226 Directory send OK.\r\n");
}

static void ftpdStepRetr(FtpdSession *s)
{
    u64 read = 0;

    if (s->file.pos < s->size)
    {
        if (R_FAILED(IFile_Read(&s->file, &read, g_state->xfer, sizeof(g_state->xfer))) || read == 0 ||
            !ftpdSend(s->data, g_state->xfer, (u32)read))
        {
            ftpdEndTransfer(s, "426 Transfer aborted.\r\n");
            return;
        }
    }

    if (s->file.pos >= s->size)
        ftpdEndTransfer(s, "226 Transfer complete.\r\n");
}

static void ftpdStepStor(FtpdSession *s)
{
    u32 len = 0;
    u64 written = 0;
    ssize_t n;

    do
    {
        n = soc_recv(s->data, g_state->xfer + len, sizeof(g_state->xfer) - len, 0);
        if (n > 0)
            len += (u32)n;
    }
    while (n > 0 && len < sizeof(g_state->xfer) && ftpdReadable(s->data));

    if (len != 0 && R_FAILED(IFile_Write(&s->file, &written, g_state->xfer, len, 0)))
        ftpdEndTransfer(s, "426 Transfer aborted.\r\n");
    else if (n == 0)
        ftpdEndTransfer(s, "226 Transfer complete.\r\n");
    else if (n < 0)
        ftpdEndTransfer(s, "426 Transfer aborted.\r\n");
}

static void ftpdStep(FtpdSession *s, int fd, int revents)
{
    if (s->state == FTPD_IDLE)
        return;

    if (fd == s->pasv)
    {
        struct sockaddr_in caddr;
        socklen_t clen = sizeof(caddr);

        s->data = (revents & POLLIN) ? socAccept(s->pasv, (struct sockaddr *)&caddr, &clen) : -1;

        socClose(s->pasv);
        s->pasv = -1;

        if (s->data < 0)
            ftpdFailDataConn(s);
        else if (s->state == FTPD_STOR && s->truncateAt != FTPD_NO_TRUNCATE &&
                 R_FAILED(FSFILE_SetSize(s->file.handle, s->truncateAt)))
            ftpdEndTransfer(s, "451 Cannot truncate file.\r\n");
    }
    else if (fd == s->data)
    {
        if (s->state == FTPD_RETR)
            ftpdStepRetr(s);
        else if (s->state == FTPD_STOR)
            ftpdStepStor(s);
        else
            ftpdStepList(s);
    }
}

// ---------------------------------------------------------------- commands

static void ftpdCmdList(FtpdSession *s, const char *arg, bool namesOnly)
{
    while (*arg == '-')
    {
        while (*arg != 0 && *arg != ' ')
            ++arg;
        while (*arg == ' ')
            ++arg;
    }

    if (!ftpdResolve(s->xferPath, sizeof(s->xferPath), s->cwd, arg))
    {
        ftpdReply(s->ctrl, "550 Path too long.\r\n");
        return;
    }

    if (R_FAILED(ftpdOpenArchive(&s->archive)))
    {
        ftpdReply(s->ctrl, "550 Cannot open SD.\r\n");
        return;
    }

    if (R_FAILED(FSUSER_OpenDirectory(&s->dir, s->archive, IFile_MakeUtf8Path(g_state->pathW, s->xferPath))))
    {
        FSUSER_CloseArchive(s->archive);
        ftpdReply(s->ctrl, "550 No such directory.\r\n");
        return;
    }

    ftpdBeginTransfer(s, namesOnly ? FTPD_NLST : FTPD_LIST, "150 Here comes the directory listing.\r\n");
}

static void ftpdCmdRetr(FtpdSession *s, const char *arg)
{
    if (!ftpdResolve(s->xferPath, sizeof(s->xferPath), s->cwd, arg))
    {
        s->rest = 0;
        ftpdReply(s->ctrl, "550 Path too long.\r\n");
        return;
    }

    if (R_FAILED(IFile_Open(&s->file, ARCHIVE_SDMC, fsMakePath(PATH_EMPTY, ""),
                            IFile_MakeUtf8Path(g_state->pathW, s->xferPath), FS_OPEN_READ)))
    {
        s->rest = 0;
        ftpdReply(s->ctrl, "550 Failed to open file.\r\n");
        return;
    }

    s->size = 0;
    IFile_GetSize(&s->file, &s->size);
    s->file.pos = s->rest < s->size ? s->rest : s->size;

    ftpdBeginTransfer(s, FTPD_RETR, "150 Opening data connection.\r\n");
}

static void ftpdCmdStor(FtpdSession *s, const char *arg, bool append)
{
    bool isDir = false;
    bool exists;
    u64 size = 0;

    if (!ftpdResolve(s->xferPath, sizeof(s->xferPath), s->cwd, arg))
    {
        s->rest = 0;
        ftpdReply(s->ctrl, "550 Path too long.\r\n");
        return;
    }

    // 既存のファイルを切り詰めるのはデータ接続がつながってから（ftpdStep）。
    // 先に PASV が無ければ、ファイルには触らずに断る。
    if (s->pasv < 0 && s->data < 0)
    {
        s->rest = 0;
        ftpdReply(s->ctrl, "425 Use PASV first.\r\n");
        return;
    }

    exists = ftpdStat(s->xferPath, &isDir, &size);

    if (!append && s->rest > size)
    {
        s->rest = 0;
        ftpdReply(s->ctrl, "554 Restart offset is past the end of the file.\r\n");
        return;
    }

    if (R_FAILED(IFile_Open(&s->file, ARCHIVE_SDMC, fsMakePath(PATH_EMPTY, ""),
                            IFile_MakeUtf8Path(g_state->pathW, s->xferPath), FS_OPEN_CREATE | FS_OPEN_WRITE)))
    {
        s->rest = 0;
        ftpdReply(s->ctrl, "550 Cannot create file.\r\n");
        return;
    }

    s->created = !exists;
    s->size = 0;
    s->file.pos = append ? size : s->rest;
    s->truncateAt = append ? FTPD_NO_TRUNCATE : s->file.pos;

    ftpdBeginTransfer(s, FTPD_STOR, "150 Ready to receive data.\r\n");
}

static void ftpdCmdDele(FtpdSession *s, const char *arg)
{
    FS_Archive archive = 0;

    if (!ftpdResolve(g_state->scratch, sizeof(g_state->scratch), s->cwd, arg) ||
        R_FAILED(ftpdOpenArchive(&archive)))
    {
        ftpdReply(s->ctrl, "550 Failed.\r\n");
        return;
    }

    Result res = FSUSER_DeleteFile(archive, IFile_MakeUtf8Path(g_state->pathW, g_state->scratch));
    FSUSER_CloseArchive(archive);

    ftpdReply(s->ctrl, R_SUCCEEDED(res) ? "250 File deleted.\r\n" : "550 Delete failed.\r\n");
}

static void ftpdCmdMkd(FtpdSession *s, const char *arg)
{
    FS_Archive archive = 0;

    if (!ftpdResolve(g_state->scratch, sizeof(g_state->scratch), s->cwd, arg) ||
        R_FAILED(ftpdOpenArchive(&archive)))
    {
        ftpdReply(s->ctrl, "550 Failed.\r\n");
        return;
    }

    Result res = FSUSER_CreateDirectory(archive, IFile_MakeUtf8Path(g_state->pathW, g_state->scratch), 0);
    FSUSER_CloseArchive(archive);

    if (R_SUCCEEDED(res))
        ftpdReply(s->ctrl, "257 \"%s\" created.\r\n", g_state->scratch);
    else
        ftpdReply(s->ctrl, "550 Create failed.\r\n");
}

static void ftpdCmdRmd(FtpdSession *s, const char *arg)
{
    FS_Archive archive = 0;

    if (!ftpdResolve(g_state->scratch, sizeof(g_state->scratch), s->cwd, arg) || ftpdIsRoot(g_state->scratch) ||
        R_FAILED(ftpdOpenArchive(&archive)))
    {
        ftpdReply(s->ctrl, "550 Failed.\r\n");
        return;
    }

    Result res = FSUSER_DeleteDirectoryRecursively(archive, IFile_MakeUtf8Path(g_state->pathW, g_state->scratch));
    FSUSER_CloseArchive(archive);

    ftpdReply(s->ctrl, R_SUCCEEDED(res) ? "250 Directory removed.\r\n" : "550 Remove failed.\r\n");
}

static void ftpdCmdRnto(FtpdSession *s, const char *arg)
{
    FS_Archive archive = 0;
    bool isDir = false;
    u64 size = 0;

    if (s->renameFrom[0] == 0)
    {
        ftpdReply(s->ctrl, "503 RNFR required first.\r\n");
        return;
    }

    if (!ftpdResolve(g_state->scratch, sizeof(g_state->scratch), s->cwd, arg) ||
        R_FAILED(ftpdOpenArchive(&archive)))
    {
        ftpdReply(s->ctrl, "550 Failed.\r\n");
        return;
    }

    ftpdStat(s->renameFrom, &isDir, &size);

    FS_Path from = IFile_MakeUtf8Path(g_state->pathW, s->renameFrom);
    FS_Path to = IFile_MakeUtf8Path(g_state->pathW2, g_state->scratch);
    Result res;

    if (isDir)
        res = FSUSER_RenameDirectory(archive, from, archive, to);
    else
        res = FSUSER_RenameFile(archive, from, archive, to);

    FSUSER_CloseArchive(archive);
    s->renameFrom[0] = 0;

    ftpdReply(s->ctrl, R_SUCCEEDED(res) ? "250 Rename successful.\r\n" : "550 Rename failed.\r\n");
}

static void ftpdCmdMdtm(FtpdSession *s, const char *arg)
{
    FS_Archive archive = 0;
    u64 ms = 0;
    bool ok = false;

    if (ftpdResolve(g_state->scratch, sizeof(g_state->scratch), s->cwd, arg) && R_SUCCEEDED(ftpdOpenArchive(&archive)))
    {
        FS_Path path = IFile_MakeUtf8Path(g_state->pathW, g_state->scratch);

        ok = path.type == PATH_UTF16 && ftpdGetTime(archive, path.size / 2 - 1, &ms);
        FSUSER_CloseArchive(archive);
    }

    if (ok)
    {
        u32 t[6];

        ftpdCivil(ms, t);
        ftpdReply(s->ctrl, "213 %04lu%02lu%02lu%02lu%02lu%02lu\r\n", t[0], t[1] + 1, t[2], t[3], t[4], t[5]);
    }
    else
    {
        ftpdReply(s->ctrl, "550 Could not get file time.\r\n");
    }
}

// ---------------------------------------------------------------- dispatch

static bool ftpdMatch(const char *cmd, const char *name)
{
    u32 i = 0;

    while (name[i] != 0)
    {
        char c = cmd[i];
        if (c >= 'a' && c <= 'z')
            c = (char)(c - 'a' + 'A');
        if (c != name[i])
            return false;
        ++i;
    }

    return cmd[i] == 0 || cmd[i] == ' ';
}

static const char *ftpdArg(const char *cmd)
{
    const char *p = cmd;

    while (*p != 0 && *p != ' ')
        ++p;
    while (*p == ' ')
        ++p;

    return p;
}

// true を返すと接続を閉じる
static bool ftpdHandle(FtpdSession *s, const char *cmd)
{
    const char *arg = ftpdArg(cmd);
    int ctrl = s->ctrl;

    if (ftpdMatch(cmd, "USER"))          ftpdReply(ctrl, "230 Logged in.\r\n");
    else if (ftpdMatch(cmd, "PASS"))     ftpdReply(ctrl, "230 Logged in.\r\n");
    else if (ftpdMatch(cmd, "SYST"))     ftpdReply(ctrl, "215 UNIX Type: L8\r\n");
    else if (ftpdMatch(cmd, "NOOP"))     ftpdReply(ctrl, "200 OK.\r\n");
    else if (ftpdMatch(cmd, "TYPE"))     ftpdReply(ctrl, "200 Type set.\r\n");
    else if (ftpdMatch(cmd, "MODE"))     ftpdReply(ctrl, "200 Mode set.\r\n");
    else if (ftpdMatch(cmd, "STRU"))     ftpdReply(ctrl, "200 Structure set.\r\n");
    else if (ftpdMatch(cmd, "OPTS"))     ftpdReply(ctrl, "200 OK.\r\n");
    else if (ftpdMatch(cmd, "FEAT"))     ftpdReply(ctrl, "211-Features:\r\n MDTM\r\n REST STREAM\r\n SIZE\r\n UTF8\r\n211 End\r\n");
    else if (ftpdMatch(cmd, "PORT"))     ftpdReply(ctrl, "502 Use passive mode.\r\n");
    else if (ftpdMatch(cmd, "EPRT"))     ftpdReply(ctrl, "502 Use passive mode.\r\n");
    else if (ftpdMatch(cmd, "EPSV"))     ftpdReply(ctrl, "502 Use PASV.\r\n");
    else if (ftpdMatch(cmd, "QUIT"))
    {
        ftpdReply(ctrl, "221 Goodbye.\r\n");
        return true;
    }
    else if (ftpdMatch(cmd, "ABOR"))
    {
        if (s->state != FTPD_IDLE)
            ftpdEndTransfer(s, "426 Transfer aborted.\r\n");
        ftpdReply(ctrl, "226 Abort successful.\r\n");
    }
    else if (ftpdMatch(cmd, "PWD") || ftpdMatch(cmd, "XPWD"))
    {
        ftpdReply(ctrl, "257 \"%s\" is the current directory.\r\n", s->cwd);
    }
    else if (ftpdMatch(cmd, "CWD") || ftpdMatch(cmd, "XCWD"))
    {
        bool isDir = false;
        u64 size = 0;

        if (ftpdResolve(g_state->scratch, sizeof(g_state->scratch), s->cwd, arg) &&
            ftpdStat(g_state->scratch, &isDir, &size) && isDir)
        {
            strncpy(s->cwd, g_state->scratch, sizeof(s->cwd) - 1);
            s->cwd[sizeof(s->cwd) - 1] = 0;
            ftpdReply(ctrl, "250 Directory changed.\r\n");
        }
        else
        {
            ftpdReply(ctrl, "550 No such directory.\r\n");
        }
    }
    else if (ftpdMatch(cmd, "CDUP") || ftpdMatch(cmd, "XCUP"))
    {
        if (ftpdResolve(g_state->scratch, sizeof(g_state->scratch), s->cwd, ".."))
        {
            strncpy(s->cwd, g_state->scratch, sizeof(s->cwd) - 1);
            s->cwd[sizeof(s->cwd) - 1] = 0;
        }
        ftpdReply(ctrl, "250 Directory changed.\r\n");
    }
    else if (ftpdMatch(cmd, "PASV"))
    {
        if (!ftpdOpenPasvFixed(s))
        {
            ftpdReply(ctrl, "425 Cannot open passive socket.\r\n");
        }
        else
        {
            u32 ip = gethostid();
            u32 p = g_pasvPort;

            ftpdReply(ctrl, "227 Entering Passive Mode (%lu,%lu,%lu,%lu,%lu,%lu).\r\n",
                      ip & 0xFF, (ip >> 8) & 0xFF, (ip >> 16) & 0xFF, (ip >> 24) & 0xFF,
                      p >> 8, p & 0xFF);
        }
    }
    else if (ftpdMatch(cmd, "REST"))
    {
        const char *p = arg;
        u64 offset = 0;

        while (*p >= '0' && *p <= '9')
            offset = offset * 10 + (u64)(*p++ - '0');

        if (p == arg || *p != 0)
        {
            ftpdReply(ctrl, "501 Invalid offset.\r\n");
        }
        else
        {
            s->rest = offset;
            ftpdReply(ctrl, "350 Restarting at %llu.\r\n", offset);
        }
    }
    else if (ftpdMatch(cmd, "LIST"))     ftpdCmdList(s, arg, false);
    else if (ftpdMatch(cmd, "NLST"))     ftpdCmdList(s, arg, true);
    else if (ftpdMatch(cmd, "RETR"))     ftpdCmdRetr(s, arg);
    else if (ftpdMatch(cmd, "STOR"))     ftpdCmdStor(s, arg, false);
    else if (ftpdMatch(cmd, "APPE"))     ftpdCmdStor(s, arg, true);
    else if (ftpdMatch(cmd, "DELE"))     ftpdCmdDele(s, arg);
    else if (ftpdMatch(cmd, "MKD") || ftpdMatch(cmd, "XMKD"))  ftpdCmdMkd(s, arg);
    else if (ftpdMatch(cmd, "RMD") || ftpdMatch(cmd, "XRMD"))  ftpdCmdRmd(s, arg);
    else if (ftpdMatch(cmd, "RNFR"))
    {
        bool isDir = false;
        u64 size = 0;

        if (ftpdResolve(g_state->scratch, sizeof(g_state->scratch), s->cwd, arg) &&
            ftpdStat(g_state->scratch, &isDir, &size))
        {
            strncpy(s->renameFrom, g_state->scratch, sizeof(s->renameFrom) - 1);
            s->renameFrom[sizeof(s->renameFrom) - 1] = 0;
            ftpdReply(ctrl, "350 Ready for RNTO.\r\n");
        }
        else
        {
            ftpdReply(ctrl, "550 No such file.\r\n");
        }
    }
    else if (ftpdMatch(cmd, "RNTO"))     ftpdCmdRnto(s, arg);
    else if (ftpdMatch(cmd, "MDTM"))     ftpdCmdMdtm(s, arg);
    else if (ftpdMatch(cmd, "SIZE"))
    {
        bool isDir = false;
        u64 size = 0;

        if (ftpdResolve(g_state->scratch, sizeof(g_state->scratch), s->cwd, arg) &&
            ftpdStat(g_state->scratch, &isDir, &size) && !isDir)
            ftpdReply(ctrl, "213 %llu\r\n", size);
        else
            ftpdReply(ctrl, "550 Not a regular file.\r\n");
    }
    else
    {
        ftpdReply(ctrl, "502 Command not implemented.\r\n");
    }

    return false;
}

// ---------------------------------------------------------------- session

static bool ftpdProcess(FtpdSession *s)
{
    for (;;)
    {
        u32 end = 0;

        while (end < s->cmdLen && s->cmd[end] != '\n')
            ++end;

        if (end == s->cmdLen)
        {
            if (s->cmdLen + 1 >= FTPD_CMD_MAX)
            {
                s->cmdLen = 0;
                ftpdReply(s->ctrl, "500 Command line too long.\r\n");
            }
            return false;
        }

        u32 start = 0, len = end;

        while (start < len && ((u8)s->cmd[start] == 0xFF || (u8)s->cmd[start] == 0xF4 || (u8)s->cmd[start] == 0xF2))
            ++start;
        if (len > start && s->cmd[len - 1] == '\r')
            --len;

        memcpy(g_state->line, s->cmd + start, len - start);
        g_state->line[len - start] = 0;

        if (s->state != FTPD_IDLE && !ftpdMatch(g_state->line, "ABOR"))
            return false;

        s->cmdLen -= end + 1;
        for (u32 i = 0; i < s->cmdLen; ++i)
            s->cmd[i] = s->cmd[end + 1 + i];

        if (g_state->line[0] != 0 && ftpdHandle(s, g_state->line))
            return true;
    }
}

static void ftpdCloseSession(FtpdSession *s)
{
    ftpdStopTransfer(s);
    socClose(s->ctrl);
    s->ctrl = -1;
    s->cmdLen = 0;
}

static bool ftpdReceive(FtpdSession *s)
{
    ssize_t n = soc_recv(s->ctrl, s->cmd + s->cmdLen, FTPD_CMD_MAX - 1 - s->cmdLen, 0);

    if (n > 0)
    {
        s->cmdLen += (u32)n;
        if (!ftpdProcess(s))
            return true;
    }

    ftpdCloseSession(s);
    return false;
}

static void ftpdAccept(void)
{
    struct sockaddr_in caddr;
    socklen_t clen = sizeof(caddr);

    int sock = socAccept(g_listenSock, (struct sockaddr *)&caddr, &clen);
    if (sock < 0)
        return;

    for (u32 i = 0; i < FTPD_MAX_SESSIONS; ++i)
    {
        FtpdSession *s = &g_state->sessions[i];

        if (s->ctrl >= 0)
            continue;

        s->ip = caddr.sin_addr.s_addr;
        s->cwd[0] = '/';
        s->cwd[1] = 0;
        s->renameFrom[0] = 0;
        s->cmdLen = 0;
        s->rest = 0;
        s->ctrl = sock;

        ftpdReply(sock, "220 Luma3DS ftpd ready.\r\n");
        return;
    }

    ftpdReply(sock, "421 Too many connections.\r\n");
    socClose(sock);
}

static bool ftpdJoin(s64 timeout)
{
    if (ftpdThread.handle == 0)
        return true;

    if (svcWaitSynchronization(ftpdThread.handle, timeout) != 0)
        return false;

    MyThread_Join(&ftpdThread, 0);
    return true;
}

MyThread *ftpdCreateThread(void)
{
    u32 tmp;

    if (g_state == NULL)
    {
        if (R_FAILED(svcControlMemoryEx(&tmp, FTPD_STATE_ADDR, 0, FTPD_STATE_SIZE,
                                        MEMOP_ALLOC | MEMOP_REGION_SYSTEM, MEMPERM_READWRITE, true)))
            return NULL;

        g_state = (FtpdState *)FTPD_STATE_ADDR;
    }

    memset(g_state->sessions, 0, sizeof(g_state->sessions));

    for (u32 i = 0; i < FTPD_MAX_SESSIONS; ++i)
    {
        g_state->sessions[i].ctrl = -1;
        g_state->sessions[i].pasv = -1;
        g_state->sessions[i].data = -1;
        g_state->sessions[i].state = FTPD_IDLE;
    }

    ftpdEnabled = true;

    if (R_FAILED(MyThread_Create(&ftpdThread, ftpdThreadMain, g_state->stack,
                                 sizeof(g_state->stack), 0x20, CORE_SYSTEM)))
        svcBreak(USERBREAK_PANIC);

    return &ftpdThread;
}

void ftpdThreadMain(void)
{
    if (R_FAILED(miniSocInit()))
    {
        ftpdEnabled = false;
        return;
    }

    g_listenSock = socSocket(AF_INET, SOCK_STREAM, 0);

    u32 tries = 15;
    while (g_listenSock < 0 && --tries > 0)
    {
        svcSleepThread(100 * 1000 * 1000LL);
        g_listenSock = socSocket(AF_INET, SOCK_STREAM, 0);
    }

    if (g_listenSock < 0 || tries == 0)
    {
        miniSocExit();
        ftpdEnabled = false;
        return;
    }

    struct sockaddr_in saddr;
    memset(&saddr, 0, sizeof(saddr));
    saddr.sin_family = AF_INET;
    saddr.sin_port = htons(FTPD_PORT);
    saddr.sin_addr.s_addr = gethostid();

    if (socBind(g_listenSock, (struct sockaddr *)&saddr, sizeof(saddr)) != 0 ||
        socListen(g_listenSock, FTPD_MAX_SESSIONS) != 0)
    {
        socClose(g_listenSock);
        g_listenSock = -1;
        miniSocExit();
        ftpdEnabled = false;
        return;
    }

    while (ftpdEnabled)
    {
        struct pollfd pfds[1 + 2 * FTPD_MAX_SESSIONS];
        s32 ctrlIdx[FTPD_MAX_SESSIONS];
        s32 xferIdx[FTPD_MAX_SESSIONS];
        nfds_t n = 0;

        pfds[n].fd = g_listenSock;
        pfds[n].events = POLLIN;
        pfds[n++].revents = 0;

        for (u32 i = 0; i < FTPD_MAX_SESSIONS; ++i)
        {
            FtpdSession *s = &g_state->sessions[i];
            int fd = s->data >= 0 ? s->data : s->pasv;

            ctrlIdx[i] = -1;
            xferIdx[i] = -1;

            if (s->ctrl < 0)
                continue;

            if (s->cmdLen + 1 < FTPD_CMD_MAX)
            {
                ctrlIdx[i] = (s32)n;
                pfds[n].fd = s->ctrl;
                pfds[n].events = POLLIN;
                pfds[n++].revents = 0;
            }

            if (s->state != FTPD_IDLE && fd >= 0)
            {
                xferIdx[i] = (s32)n;
                pfds[n].fd = fd;
                pfds[n].events = (s->data >= 0 && s->state != FTPD_STOR) ? POLLOUT : POLLIN;
                pfds[n++].revents = 0;
            }
        }

        if (socPoll(pfds, n, 100) < 0)
        {
            svcSleepThread(100 * 1000 * 1000LL);
            continue;
        }

        if (pfds[0].revents & POLLIN)
            ftpdAccept();

        for (u32 i = 0; i < FTPD_MAX_SESSIONS; ++i)
        {
            FtpdSession *s = &g_state->sessions[i];
            bool ctrlReady = ctrlIdx[i] >= 0 && pfds[ctrlIdx[i]].revents != 0;

            if (ctrlReady && !ftpdReceive(s))
                continue;

            if (!ctrlReady && xferIdx[i] >= 0 && pfds[xferIdx[i]].revents != 0)
                ftpdStep(s, pfds[xferIdx[i]].fd, pfds[xferIdx[i]].revents);

            if (s->ctrl >= 0 && s->state != FTPD_IDLE && s->data < 0 && svcGetSystemTick() > s->deadline)
                ftpdFailDataConn(s);

            if (s->ctrl >= 0 && s->state == FTPD_IDLE && ftpdProcess(s))
                ftpdCloseSession(s);
        }
    }

    for (u32 i = 0; i < FTPD_MAX_SESSIONS; ++i)
    {
        if (g_state->sessions[i].ctrl >= 0)
            ftpdCloseSession(&g_state->sessions[i]);
    }

    if (g_listenSock >= 0)
    {
        socClose(g_listenSock);
        g_listenSock = -1;
    }

    miniSocExit();
}

bool Ftpd_Disable(s64 timeout)
{
    ftpdEnabled = false;

    return ftpdJoin(timeout);
}

void Ftpd_ReleaseMemory(void)
{
    u32 tmp;

    if (g_state == NULL || ftpdEnabled || !ftpdJoin(0))
        return;

    svcControlMemory(&tmp, FTPD_STATE_ADDR, 0, FTPD_STATE_SIZE, MEMOP_FREE, MEMPERM_DONTCARE);
    g_state = NULL;
}

// ---------------------------------------------------------------- menu

void RosalinaMenu_Ftpd(void)
{
    static const char *const ops[] = { TR("待機", "idle"), TR("一覧", "list"), TR("一覧", "list"), TR("送信", "send"), TR("受信", "recv") };
    bool noMemory = false;

    Draw_Lock();
    Draw_ClearFramebuffer();
    Draw_FlushFramebuffer();
    Draw_Unlock();

    do
    {
        Ftpd_ReleaseMemory();

        Draw_Lock();
        Draw_ClearFramebuffer();
        Draw_DrawString(10, 4, COLOR_TITLE, TR("FTP サーバー", "FTP server"));

        u32 y = 4 + 3 * SPACING_Y;

        if (ftpdEnabled)
        {
            u32 ip = gethostid();
            u32 connected = 0;

            for (u32 i = 0; i < FTPD_MAX_SESSIONS; ++i)
                if (g_state->sessions[i].ctrl >= 0)
                    ++connected;

            y = Draw_DrawString(10, y, COLOR_LIME, Lang(STR_RUNNING));
            y = Draw_DrawFormattedString(10, y, COLOR_WHITE, "ftp://%lu.%lu.%lu.%lu:%lu\n\n",
                                         ip & 0xFF, (ip >> 8) & 0xFF, (ip >> 16) & 0xFF,
                                         (ip >> 24) & 0xFF, (u32)FTPD_PORT);
            y = Draw_DrawString(10, y, COLOR_WHITE, Lang(STR_ANY_USERNAME_AND_PASSWORD));
            y = Draw_DrawString(10, y, COLOR_WHITE, Lang(STR_PASSIVE_MODE_ONLY));
            y = Draw_DrawFormattedString(10, y, COLOR_WHITE, Lang(STR_SESSIONS), connected);
            y = Draw_DrawFormattedString(10, y, COLOR_WHITE, Lang(STR_TRANSFERS), g_transfers);
            y = Draw_DrawString(10, y, COLOR_WHITE, Lang(STR_KEEPS_RUNNING_WITH_THE_MENU_CLOSED));

            for (u32 i = 0; i < FTPD_MAX_SESSIONS; ++i)
            {
                const FtpdSession *s = &g_state->sessions[i];
                u32 state = s->state;
                u32 peer = s->ip;
                u64 pos = s->file.pos;
                u64 elapsed = svcGetSystemTick() - s->startTick;
                char line[DRAW_TEXT_SIZE(FTPD_LINE_COLS)];
                u32 n;

                if (s->ctrl < 0)
                    continue;

                n = sprintf(line, "%lu.%lu.%lu.%lu  ", peer & 0xFF, (peer >> 8) & 0xFF,
                            (peer >> 16) & 0xFF, (peer >> 24) & 0xFF);

                if (state == FTPD_RETR && s->size != 0)
                    n += sprintf(line + n, TR("送信 %lu%% ", "send %lu%% "), (u32)(pos * 100 / s->size));
                else if (state == FTPD_RETR || state == FTPD_STOR)
                    n += sprintf(line + n, "%s %luK ", ops[state], (u32)(pos >> 10));
                else
                    n += sprintf(line + n, "%s ", ops[state]);

                if ((state == FTPD_RETR || state == FTPD_STOR) && elapsed >= SYSCLOCK_ARM11 && pos >= s->startPos)
                    n += sprintf(line + n, "%luK/s ", (u32)((pos - s->startPos) * SYSCLOCK_ARM11 / elapsed >> 10));

                // n はバイト数。状態の名前は日本語なので、残りの幅は桁数で数える
                u32 cols = Draw_TextColumns(line);
                if (state != FTPD_IDLE && cols < FTPD_LINE_COLS)
                    Draw_ClipText(line + n, sizeof(line) - n, s->shown,
                                  FTPD_LINE_COLS - cols < FTPD_NAME_COLS ? FTPD_LINE_COLS - cols : FTPD_NAME_COLS);

                y = Draw_DrawFormattedString(10, y, COLOR_WHITE, "%s\n", line);
            }

            Draw_DrawString(10, 225, COLOR_WHITE, Lang(STR_A_STOP_B_BACK));
        }
        else
        {
            y = Draw_DrawString(10, y, COLOR_WHITE, Lang(STR_STOPPED));
            if (noMemory)
                y = Draw_DrawString(10, y, COLOR_RED, Lang(STR_NOT_ENOUGH_MEMORY));
            y = Draw_DrawString(10, y, COLOR_WHITE, Lang(STR_SERVES_THE_SD_CARD_OVER_FTP_SO_YOU));
            y = Draw_DrawString(10, y, COLOR_WHITE, Lang(STR_CAN_MANAGE_FILES_FROM_A_PC));
            y = Draw_DrawString(10, y, COLOR_WHITE, Lang(STR_WI_FI_MUST_BE_CONNECTED_FIRST));

            Draw_DrawString(10, 225, COLOR_WHITE, Lang(STR_A_START_B_BACK));
        }

        Draw_FlushFramebuffer();
        Draw_Unlock();

        u32 k = waitInputWithTimeout(1000);

        if (terminationRequest || (k & BUTTON_B))
            return;

        if (k & BUTTON_A)
        {
            if (ftpdEnabled)
            {
                Ftpd_Disable(5 * 1000 * 1000 * 1000LL);
            }
            else if (ftpdJoin(0))
            {
                noMemory = ftpdCreateThread() == NULL;
                if (!noMemory)
                    svcSleepThread(500 * 1000 * 1000LL);
            }
        }
    }
    while (!terminationRequest);
}

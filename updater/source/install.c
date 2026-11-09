/*
*   This file is part of a modified version of Luma3DS (Luma3DS_New updater)
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
*/

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include "install.h"
#include "local.h"

#ifdef __3DS__
#include <3ds.h>
#endif

#define INSTALL_PROGRESS_STEP   (32 << 10)  // 進み具合はこれだけ進むごとに知らせる（画面を描くのが遅いため）

static unsigned char g_copyBuf[0x10000];

typedef struct DownloadCtx
{
    FILE                    *file;
    uint64_t                done, total, shown;
    const InstallProgress   *progress;
    const char              *label;
    bool                    writeFailed;
    bool                    cancelled;
} DownloadCtx;

static void Install_Report(const InstallProgress *progress, const char *label, uint64_t done, uint64_t total)
{
    if (progress != NULL && progress->update != NULL)
        progress->update(progress->ctx, label, done, total);
}

static bool Download_Begin(void *ctx, int64_t length)
{
    DownloadCtx *d = (DownloadCtx *)ctx;

    d->total = length > 0 ? (uint64_t)length : 0;
    Install_Report(d->progress, d->label, 0, d->total);
    return true;
}

static bool Download_Write(void *ctx, const void *data, size_t len)
{
    DownloadCtx *d = (DownloadCtx *)ctx;

    if (fwrite(data, 1, len, d->file) != len)
    {
        d->writeFailed = true;
        return false;
    }
    d->done += len;

    if (d->progress != NULL && d->progress->cancelled != NULL && d->progress->cancelled(d->progress->ctx))
    {
        d->cancelled = true;
        return false;
    }
    if (d->done - d->shown >= INSTALL_PROGRESS_STEP || d->done == d->total)
    {
        d->shown = d->done;
        Install_Report(d->progress, d->label, d->done, d->total);
    }
    return true;
}

InstallResult Install_Download(const ReleaseAsset *asset, const char *path,
                               const InstallProgress *progress, int *detail, int *detail2)
{
    DownloadCtx d;
    HttpsSink   sink = { Download_Begin, Download_Write, &d };
    HttpsResult res;
    bool        closed;

    *detail = *detail2 = 0;
    memset(&d, 0, sizeof(d));
    d.progress = progress;
    d.label = asset->name;

    if (!Local_MakeParentDirs(path) || (d.file = fopen(path, "wb")) == NULL)
        return INSTALL_ERR_WRITE;

    res = Https_Get(asset->url, NULL, &sink, detail2);
    closed = fclose(d.file) == 0;

    if (res != HTTPS_OK || !closed)
    {
        remove(path);
        if (d.cancelled)
            return INSTALL_ERR_CANCELLED;
        if (d.writeFailed || res == HTTPS_OK)
            return INSTALL_ERR_WRITE;
        *detail = (int)res;
        return INSTALL_ERR_NET;
    }

    // 中身は TLS で守られている。途中で切れたものを使わないよう、大きさだけ確かめる
    if (asset->size != 0 && d.done != asset->size)
    {
        remove(path);
        return INSTALL_ERR_FORMAT;
    }
    return INSTALL_OK;
}

bool Install_HasMagic(const char *path, const char *magic)
{
    char head[4];
    FILE *f = fopen(path, "rb");
    bool ok;

    if (f == NULL)
        return false;
    ok = fread(head, 1, sizeof(head), f) == sizeof(head) && memcmp(head, magic, sizeof(head)) == 0;
    fclose(f);
    return ok;
}

static bool Install_CopyFile(const char *src, const char *dst)
{
    FILE *in = fopen(src, "rb"), *out;
    size_t n;
    bool ok = true;

    if (in == NULL)
        return false;
    out = fopen(dst, "wb");
    if (out == NULL)
    {
        fclose(in);
        return false;
    }

    while (ok && (n = fread(g_copyBuf, 1, sizeof(g_copyBuf), in)) > 0)
        ok = fwrite(g_copyBuf, 1, n, out) == n;

    ok = ok && !ferror(in);
    fclose(in);
    return fclose(out) == 0 && ok;
}

InstallResult Install_ReplaceFile(const char *staged, const char *target, const char *backupName)
{
    char        newPath[256], backup[256];
    FILE        *f;
    bool        exists;

    if (snprintf(newPath, sizeof(newPath), "%s.new", target) >= (int)sizeof(newPath) ||
        snprintf(backup, sizeof(backup), UPDATE_BACKUP_DIR "/%s", backupName) >= (int)sizeof(backup))
        return INSTALL_ERR_WRITE;

    // 新しいものを隣に書いて確かめる（ここで失敗しても元のファイルはそのまま）
    if (!Local_MakeParentDirs(target) || !Install_CopyFile(staged, newPath) || !Local_SameFile(newPath, staged))
    {
        remove(newPath);
        return INSTALL_ERR_WRITE;
    }

    f = fopen(target, "rb");
    exists = f != NULL;
    if (f != NULL)
        fclose(f);

    if (exists)
    {
        remove(backup);
        if (!Local_MakeParentDirs(backup) || !Install_CopyFile(target, backup) || remove(target) != 0)
        {
            remove(newPath);
            return INSTALL_ERR_WRITE;
        }
    }

    // FAT では上書きの rename ができないので、元を消してから名前を変える。
    // 失敗したら、残しておいた元のファイルを戻す
    if (rename(newPath, target) != 0)
    {
        if (exists)
            Install_CopyFile(backup, target);
        remove(newPath);
        return INSTALL_ERR_WRITE;
    }
    return INSTALL_OK;
}

InstallResult Install_Cia(const char *path, const InstallProgress *progress, int *detail)
{
#ifdef __3DS__
    FILE        *f;
    Handle      cia = 0;
    Result      res;
    uint64_t    offset = 0, total;
    size_t      n;

    *detail = 0;
    f = fopen(path, "rb");
    if (f == NULL)
        return INSTALL_ERR_WRITE;
    fseek(f, 0, SEEK_END);
    total = (uint64_t)ftell(f);
    fseek(f, 0, SEEK_SET);

    res = amInit();
    if (R_SUCCEEDED(res))
    {
        res = AM_StartCiaInstall(MEDIATYPE_SD, &cia);
        while (R_SUCCEEDED(res) && (n = fread(g_copyBuf, 1, sizeof(g_copyBuf), f)) > 0)
        {
            u32 written = 0;
            res = FSFILE_Write(cia, &written, offset, g_copyBuf, (u32)n, 0);
            if (R_SUCCEEDED(res) && written != n)
                res = -1;
            offset += n;
            Install_Report(progress, "Luma3DS_New_Updater.cia", offset, total);
        }

        if (cia != 0)
        {
            if (R_SUCCEEDED(res) && offset == total)
                res = AM_FinishCiaInstall(cia);
            else
                AM_CancelCIAInstall(cia);
        }
        amExit();
    }

    fclose(f);
    *detail = (int)res;
    return R_SUCCEEDED(res) && offset == total ? INSTALL_OK : INSTALL_ERR_CIA;
#else
    (void)path;
    (void)progress;
    *detail = 0;
    return INSTALL_ERR_CIA;
#endif
}

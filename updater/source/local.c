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
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "local.h"

#define BUNDLE_FIRM_NEW BUNDLE_ROOT "/new.firm"
#define BUNDLE_FIRM_91  BUNDLE_ROOT "/9.1.firm"

// v1.4 より前は、新しい方の Luma を版の番号のフォルダに置いていた
#define LEGACY_DIR      SD_ROOT "/3ds/luma/13.4"
#define LEGACY_FIRM     LEGACY_DIR "/boot.firm"

static const char *const g_paths[PART_COUNT] =
{
    SD_ROOT "/boot.firm",
    SD_ROOT "/3ds/luma/new/boot.firm",
    SD_ROOT "/3ds/luma/9.1/boot.firm",
};

const char *Part_Path(Part part)
{
    return g_paths[part];
}

const char *Part_Bundle(Part part, const LocalState *st)
{
    if (part == PART_ROOT_FIRM)
        return strcmp(st->rootLuma, "9.1") == 0 ? BUNDLE_FIRM_91 : BUNDLE_FIRM_NEW;
    return part == PART_FIRM_91 ? BUNDLE_FIRM_91 : BUNDLE_FIRM_NEW;
}

bool Local_SameFile(const char *a, const char *b)
{
    static unsigned char bufA[0x8000], bufB[0x8000];
    struct stat sa, sb;
    FILE *fa, *fb;
    size_t n;
    bool same = true;

    if (stat(a, &sa) != 0 || stat(b, &sb) != 0 || sa.st_size != sb.st_size)
        return false;
    fa = fopen(a, "rb");
    fb = fopen(b, "rb");
    if (fa == NULL || fb == NULL)
    {
        if (fa != NULL)
            fclose(fa);
        if (fb != NULL)
            fclose(fb);
        return false;
    }

    while (same && (n = fread(bufA, 1, sizeof(bufA), fa)) > 0)
        same = fread(bufB, 1, n, fb) == n && memcmp(bufA, bufB, n) == 0;

    same = same && !ferror(fa) && !ferror(fb);
    fclose(fa);
    fclose(fb);
    return same;
}

static bool Local_Exists(const char *path)
{
    struct stat sb;
    return stat(path, &sb) == 0 && S_ISREG(sb.st_mode);
}

bool Local_MakeParentDirs(const char *path)
{
    char dir[256];
    size_t n = strlen(path);

    if (n >= sizeof(dir))
        return false;
    memcpy(dir, path, n + 1);

    // sdmc:/a/b/c.txt → sdmc:/a、sdmc:/a/b の順に作る（最初の / は飛ばす）
    for (char *p = strchr(dir, '/'); p != NULL && (p = strchr(p + 1, '/')) != NULL;)
    {
        *p = 0;
        if (mkdir(dir, 0777) != 0 && errno != EEXIST)
        {
            struct stat sb;
            if (stat(dir, &sb) != 0 || !S_ISDIR(sb.st_mode))
                return false;
        }
        *p = '/';
    }
    return true;
}

// 小さいテキストのファイルを読む（無ければ空）
static void Local_ReadText(const char *path, char *buf, size_t size)
{
    FILE *f = fopen(path, "rb");
    size_t n = 0;

    if (f != NULL)
    {
        n = fread(buf, 1, size - 1, f);
        fclose(f);
    }
    buf[n] = 0;
}

// "name=value" の行から value を取る
static const char *Local_Value(const char *text, const char *name, char *out, size_t size)
{
    size_t len = strlen(name);

    for (const char *line = text; *line != 0;)
    {
        const char *eol = strchr(line, '\n');
        if (eol == NULL)
            eol = line + strlen(line);

        if (strncmp(line, name, len) == 0 && line[len] == '=')
        {
            size_t n = (size_t)(eol - line) - len - 1;
            if (n > 0 && line[len + n] == '\r')
                --n;
            if (n >= size)
                n = size - 1;
            memcpy(out, line + len + 1, n);
            out[n] = 0;
            return out;
        }
        line = *eol != 0 ? eol + 1 : eol;
    }
    out[0] = 0;
    return NULL;
}

static void Local_ReadRunning(LocalState *st)
{
    char text[64], *nl;

    Local_ReadText(UPDATE_RUNNING_PATH, text, sizeof(text));

    nl = strchr(text, '\n');
    if (nl == NULL)
        return;
    *nl = 0;
    snprintf(st->runningTag, sizeof(st->runningTag), "%.*s", (int)sizeof(st->runningTag) - 1, text);

    char *luma = nl + 1;
    nl = strchr(luma, '\n');
    if (nl != NULL)
        *nl = 0;
    snprintf(st->runningLuma, sizeof(st->runningLuma), "%.*s", (int)sizeof(st->runningLuma) - 1, luma);
}

static void Local_ReadCheck(LocalState *st)
{
    char text[256], value[32];

    Local_ReadText(UPDATE_CHECK_PATH, text, sizeof(text));
    if (Local_Value(text, "result", st->checkResult, sizeof(st->checkResult)) == NULL)
        return;

    st->checkKnown = true;
    if (Local_Value(text, "time", value, sizeof(value)) != NULL)
        st->checkTime = strtoull(value, NULL, 10);
    if (Local_Value(text, "code", value, sizeof(value)) != NULL)
        st->checkCode = atoi(value);
    Local_Value(text, "latest", st->checkLatest, sizeof(st->checkLatest));
}

void Local_Read(LocalState *st)
{
    FILE *f;

    memset(st, 0, sizeof(*st));

    for (int i = 0; i < PART_COUNT; ++i)
        st->exists[i] = Local_Exists(g_paths[i]);

    Local_ReadRunning(st);
    Local_ReadCheck(st);

    // SD:/boot.firm がどちらの版か。このアプリの boot.firm、/3ds/luma/<版>/boot.firm（版を切り替えたときに
    // 写したもの。v1.4 より前の 13.4 も見る）、Rosalina が書いた running.txt の順に見る。
    // どれでもなければ（公式の Luma など）新しい方にする
    const char *root = g_paths[PART_ROOT_FIRM];
    if (Local_SameFile(root, BUNDLE_FIRM_NEW))
        st->rootLuma = "new";
    else if (Local_SameFile(root, BUNDLE_FIRM_91))
        st->rootLuma = "9.1";
    else if (Local_SameFile(root, g_paths[PART_FIRM_NEW]) || Local_SameFile(root, LEGACY_FIRM))
        st->rootLuma = "new";
    else if (Local_SameFile(root, g_paths[PART_FIRM_91]))
        st->rootLuma = "9.1";
    else if (strcmp(st->runningLuma, "9.1") == 0)
        st->rootLuma = "9.1";
    else
        st->rootLuma = "new";

    for (int i = 0; i < PART_COUNT; ++i)
        st->same[i] = st->exists[i] && Local_SameFile(g_paths[i], Part_Bundle((Part)i, st));

    f = fopen(UPDATE_OFF_PATH, "rb");
    st->autoCheck = f == NULL;
    if (f != NULL)
        fclose(f);
}

void Local_RemoveLegacy(void)
{
    remove(LEGACY_FIRM);
    rmdir(LEGACY_DIR);      // ほかのファイルがあれば消えない
}

bool Local_NeedsInstall(const LocalState *st)
{
    for (int i = 0; i < PART_COUNT; ++i)
        if (!st->same[i])
            return true;
    return false;
}

bool Local_SetAutoCheck(bool on)
{
    FILE *f;

    if (on)
        return remove(UPDATE_OFF_PATH) == 0 || errno == ENOENT;

    if (!Local_MakeParentDirs(UPDATE_OFF_PATH) || (f = fopen(UPDATE_OFF_PATH, "wb")) == NULL)
        return false;
    fclose(f);
    return true;
}

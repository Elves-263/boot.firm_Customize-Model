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

#include <stdio.h>
#include "lang.h"
#include "local.h"

#ifdef __3DS__
#include <3ds.h>
#endif

#define LANG_CFG_PATH SD_ROOT "/luma/lang.cfg"

bool g_langEn;

void Lang_Init(void)
{
    FILE *f = fopen(LANG_CFG_PATH, "rb");
    int c = EOF;

    if (f != NULL)
    {
        c = fgetc(f);
        fclose(f);
    }
    if (c == '0' || c == '1')
    {
        g_langEn = c == '0';
        return;
    }

#ifdef __3DS__
    u8 language = CFG_LANGUAGE_JP;
    bool known = false;
    if (R_SUCCEEDED(cfguInit()))
    {
        known = R_SUCCEEDED(CFGU_GetSystemLanguage(&language));
        cfguExit();
    }
    g_langEn = known && language != CFG_LANGUAGE_JP;

    // 起動時の Luma の設定画面も同じ言語になるよう、lang.cfg に書いておく（Rosalina と同じ）
    f = known ? fopen(LANG_CFG_PATH, "wb") : NULL;
    if (f != NULL)
    {
        fputc(g_langEn ? '0' : '1', f);
        fclose(f);
    }
#else
    g_langEn = false;
#endif
}

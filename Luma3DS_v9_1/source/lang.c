/*
*   This file is part of a modified version of Luma3DS (Luma3DS_New)
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

#include "lang.h"
#include "fs.h"

#define LANG_CFG_PATH "0:/luma/lang.cfg"     // 0: は SD カード

bool langEnglish = false;

void initLanguage(void)
{
    char buf[4] = { 0 };

    if(fileRead(buf, LANG_CFG_PATH, sizeof(buf)) > 0) langEnglish = buf[0] == '0';
}

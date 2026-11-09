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

#include "text.h"

size_t Text_Decode(const char *s, uint32_t *code)
{
    const uint8_t *p = (const uint8_t *)s;
    uint32_t c = p[0], min;
    size_t n;

    if (c < 0x80)       { *code = c; return 1; }
    else if (c < 0xC2)  { *code = c; return 1; }
    else if (c < 0xE0)  { n = 2; c &= 0x1F; min = 0x80; }
    else if (c < 0xF0)  { n = 3; c &= 0x0F; min = 0x800; }
    else if (c < 0xF5)  { n = 4; c &= 0x07; min = 0x10000; }
    else                { *code = c; return 1; }

    for (size_t i = 1; i < n; ++i)
    {
        if ((p[i] & 0xC0) != 0x80)
        {
            *code = p[0];
            return 1;
        }
        c = c << 6 | (p[i] & 0x3F);
    }

    if (c < min || c > 0x10FFFF || (c >= 0xD800 && c <= 0xDFFF))
    {
        *code = p[0];
        return 1;
    }
    *code = c;
    return n;
}

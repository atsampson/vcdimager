/*
    $Id$

    Copyright (C) 2000 Herbert Valerio Riedel <hvr@gnu.org>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#ifndef __VCD_UTIL_H__
#define __VCD_UTIL_H__

#include <stdlib.h>
#include <libvcd/vcd_types.h>

#undef  MAX
#define MAX(a, b)  (((a) > (b)) ? (a) : (b))

#undef  MIN
#define MIN(a, b)  (((a) < (b)) ? (a) : (b))

#undef  IN
#define IN(x, low, high) ((x) >= (low) && (x) <= (high))

#undef  CLAMP
#define CLAMP(x, low, high)  (((x) > (high)) ? (high) : (((x) < (low)) ? (low) : (x)))

unsigned
_vcd_len2blocks (unsigned len, int blocksize);

size_t
_vcd_strlenv(char **str_array);

char *
_vcd_strjoin (char *strv[], unsigned count, const char delim[]);

char **
_vcd_strsplit(const char str[], char delim);

void
_vcd_strfreev(char **strv);

void *
_vcd_malloc (size_t size);

void *
_vcd_memdup (const void *mem, size_t count);

char *
_vcd_strdup_upper (const char str[]);

char *
_vcd_strncpy_pad(char dst[], const char src[], size_t len);

#endif /* __VCD_UTIL_H__ */


/* 
 * Local variables:
 *  c-file-style: "gnu"
 *  tab-width: 8
 *  indent-tabs-mode: nil
 * End:
 */

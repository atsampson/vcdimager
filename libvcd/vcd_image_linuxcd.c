/*
    $Id$

    Copyright (C) 2001 Herbert Valerio Riedel <hvr@gnu.org>

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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <libvcd/vcd_assert.h>
#include <libvcd/vcd_bytesex.h>
#include <libvcd/vcd_cd_sector.h>
#include <libvcd/vcd_image_linuxcd.h>
#include <libvcd/vcd_iso9660.h>
#include <libvcd/vcd_logging.h>
#include <libvcd/vcd_util.h>

#if defined(__linux__)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <linux/cdrom.h>
#include <sys/stat.h>
#include <sys/ioctl.h>

/* reader */

typedef struct {
  FILE *fd;
} _img_linuxcd_src_t;

static void
_source_free (void *user_data)
{
  _img_linuxcd_src_t *_obj = user_data;

  if (_obj->fd)
    fclose (_obj->fd);

  free (_obj);
}

static int
_read_mode2_sector (void *user_data, void *data, uint32_t lsn, bool form2)
{
  _img_linuxcd_src_t *_obj = user_data;

  if (form2)
    {
      char buf[M2RAW_SIZE] = { 0, };
      struct cdrom_msf *msf = (struct cdrom_msf *) &buf;
      msf_t _msf;
      
      lba_to_msf (lsn + 150, &_msf);
      msf->cdmsf_min0 = from_bcd8(_msf.m);
      msf->cdmsf_sec0 = from_bcd8(_msf.s);
      msf->cdmsf_frame0 = from_bcd8(_msf.f);

      /* if (!msf->cdmsf_frame0) */
      vcd_debug ("reading %2.2d:%2.2d:%2.2d",
		 msf->cdmsf_min0, msf->cdmsf_sec0, msf->cdmsf_frame0);
      
      if (ioctl (fileno (_obj->fd), CDROMREADMODE2, &buf) == -1)
        {
          perror ("ioctl()");
          return 1;
          /* exit (EXIT_FAILURE); */
        }

      memcpy (data, buf, M2RAW_SIZE);
    }
  else
    {
      char buf[ISO_BLOCKSIZE] = { 0, };

      if (fseek (_obj->fd, lsn * ISO_BLOCKSIZE, SEEK_SET))
        {
          perror ("fseek()");
          clearerr (_obj->fd);
          return 1;
        }

      if (fread (buf, ISO_BLOCKSIZE, 1, _obj->fd) != 1 && ferror (_obj->fd))
        {
          perror ("fread()");
          clearerr (_obj->fd);
          return 1;
        }

      memcpy (data, buf, ISO_BLOCKSIZE);
    }

  return 0;
}

static uint32_t 
_stat_size (void *user_data)
{
  _img_linuxcd_src_t *_obj = user_data;

  struct cdrom_tocentry tocent;
  uint32_t size;

  tocent.cdte_track = CDROM_LEADOUT;
  tocent.cdte_format = CDROM_LBA;
  if (ioctl (fileno (_obj->fd), CDROMREADTOCENTRY, &tocent) == -1)
    {
      perror ("ioctl(CDROMREADTOCENTRY)");
      exit (EXIT_FAILURE);
    }
  size = tocent.cdte_addr.lba;

  return size;
}

#endif /* defined(__linux__) */

VcdImageSource *
vcd_image_source_new_linuxcd (const char device_filename[])
{
#if defined(__linux__)
  _img_linuxcd_src_t *_data;

  vcd_image_source_funcs _funcs = {
    read_mode2_sector: _read_mode2_sector,
    stat_size: _stat_size,
    free: _source_free
  };

  if (!device_filename)
    return NULL;

  _data = _vcd_malloc (sizeof (_img_linuxcd_src_t));
  _data->fd = fopen (device_filename, "rb");

  if (!_data->fd)
    {
      free (_data);
      perror ("fopen()");
      return NULL;
    }

  return vcd_image_source_new (_data, &_funcs);
#else 
  vcd_error ("linux cd image source only supported under linux");
  return NULL;
#endif
}


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
#include <libvcd/vcd_image_cd.h>
#include <libvcd/vcd_iso9660.h>
#include <libvcd/vcd_logging.h>
#include <libvcd/vcd_util.h>

static const char _rcsid[] = "$Id$";

#if defined(__bsdi__)

#include </sys/dev/scsi/scsi.h>
#include </sys/dev/scsi/scsi_ioctl.h>

#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/types.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

/* reader */

typedef struct {
  int fd;

  enum {
    _AM_NONE,
    _AM_READ_CD,
    _AM_READ_10
  } access_mode;

  char *device;
  
  bool init;
} _img_bsdicd_src_t;

static int
_scsi_cmd (int fd, struct scsi_user_cdb *sucp, const char *tag)
{
  u_char *cp;

  if  (ioctl (fd, SCSIRAWCDB, sucp))
    {
      vcd_error ("%s: ioctl (SCSIRAWCDB): %s", tag, strerror (errno));
      return 1;
    }
  if  (sucp->suc_sus.sus_status)
    {
      cp = sucp->suc_sus.sus_sense;
      vcd_info("%s: cmd: %x sus_status %d ", tag, sucp->suc_cdb[0],
	       sucp->suc_sus.sus_status);
      vcd_info(" sense: %x %x %x %x %x %x %x %x", cp[0], cp[1], cp[2],
	       cp[3], cp[4], cp[5], cp[6], cp[7]);
      return 1;
    }
  return 0;
}

static void
_source_init (_img_bsdicd_src_t *_obj)
{
  if (_obj->init)
    return;

  _obj->fd = open (_obj->device, O_RDONLY, 0);
  _obj->access_mode = _AM_READ_CD;

  if (_obj->fd < 0)
    {
      vcd_error ("open (%s): %s", _obj->device, strerror (errno));
      return;
    }

  _obj->init = true;
}


static void
_source_free (void *user_data)
{
  _img_bsdicd_src_t *_obj = user_data;

  free (_obj->device);

  if (_obj->fd)
    close (_obj->fd);

  free (_obj);
}

static int 
_set_bsize (int fd, unsigned bsize)
{
  struct
  {
    uint8_t reserved1;
    uint8_t medium;
    uint8_t reserved2;
    uint8_t block_desc_length;
    uint8_t density;
    uint8_t number_of_blocks_hi;
    uint8_t number_of_blocks_med;
    uint8_t number_of_blocks_lo;
    uint8_t reserved3;
    uint8_t block_length_hi;
    uint8_t block_length_med;
    uint8_t block_length_lo;
  } mh;

  struct scsi_user_cdb suc; 

  memset (&mh, 0, sizeof (mh));
  memset (&suc, 0, sizeof (struct scsi_user_cdb));
  
  suc.suc_cdb[0] = 0x15;
  suc.suc_cdb[1] = 1 << 4;
  suc.suc_cdb[4] = 12;
  suc.suc_cdblen = 6;;

  suc.suc_data = (u_char *)&mh;
  suc.suc_datalen = sizeof (mh);

  suc.suc_timeout = 500;
  suc.suc_flags = SUC_WRITE;

  mh.block_desc_length = 0x08;
  mh.block_length_hi = (bsize >> 16) & 0xff;
  mh.block_length_med = (bsize >> 8) & 0xff;
  mh.block_length_lo = (bsize >> 0) & 0xff;

  return _scsi_cmd (fd, &suc, "_set_bsize");
}

static int
_read_mode2 (int fd, void *buf, uint32_t lba, unsigned nblocks, 
	     bool _workaround)
{
  struct scsi_user_cdb suc; 
  int retval = 0;

  memset (&suc, 0, sizeof (struct scsi_user_cdb));

  suc.suc_cdb[0] = (_workaround 
		    ? 0x28 /* CMD_READ_10 */
		    : 0xbe /* CMD_READ_CD */);

  if (!_workaround)
    suc.suc_cdb[1] = 0; /* sector size mode2 */
  
  suc.suc_cdb[2] = (lba >> 24) & 0xff;
  suc.suc_cdb[3] = (lba >> 16) & 0xff;
  suc.suc_cdb[4] = (lba >> 8) & 0xff;
  suc.suc_cdb[5] = (lba >> 0) & 0xff;

  suc.suc_cdb[6] = (nblocks >> 16) & 0xff;
  suc.suc_cdb[7] = (nblocks >> 8) & 0xff;
  suc.suc_cdb[8] = (nblocks >> 0) & 0xff;

  if (!_workaround)
    suc.suc_cdb[9] = 0x58; /* 2336 mode2 mixed form */

  suc.suc_cdblen = _workaround ? 10 : 12;

  suc.suc_data = buf;
  suc.suc_datalen = 2336 * nblocks;

  suc.suc_timeout = 500;
  suc.suc_flags = SUC_READ;

  if (_workaround)
    {
      if ((retval = _set_bsize (fd, 2336)))
	goto out;

      if ((retval = _scsi_cmd(fd, &suc, "_read_mode2_workaround")))
	{
	  _set_bsize (fd, 2048);
	  goto out;
	}
      retval = _set_bsize (fd, 2048);
    }
  else
    retval = _scsi_cmd(fd, &suc, "_read_mode2");

 out:
  return retval;
}

static int
_read_mode2_sector (void *user_data, void *data, uint32_t lsn, bool form2)
{
  _img_bsdicd_src_t *_obj = user_data;

  _source_init (_obj);

  if (form2)
    {
    retry:
      switch (_obj->access_mode)
	{
	case _AM_NONE:
	  vcd_error ("no way to read mode2");
	  return 1;
	  break;

	case _AM_READ_CD:
	case _AM_READ_10:
	  if (_read_mode2 (_obj->fd, data, lsn, 1, 
			   (_obj->access_mode == _AM_READ_10)))
	    {
	      if (_obj->access_mode == _AM_READ_CD)
		{
		  vcd_info ("READ_CD failed; switching to READ_10 mode...");
		  _obj->access_mode = _AM_READ_10;
		  goto retry;
		}
	      else
		{
		  vcd_info ("READ_10 failed; no way to read mode2 left.");
		  _obj->access_mode = _AM_NONE;
		  goto retry;
		}
	      return 1;
	    }
	  break;
	}
    }
  else
    {
      char buf[M2RAW_SECTOR_SIZE] = { 0, };
      int retval;

      if ((retval = _read_mode2_sector (_obj, buf, lsn, true)))
	return retval;

      memcpy (data, buf + 8, ISO_BLOCKSIZE);
    }

  return 0;
}

static const u_char scsi_cdblen[8] = {6, 10, 10, 12, 12, 12, 10, 10};

static uint32_t 
_stat_size (void *user_data)
{
  _img_bsdicd_src_t *_obj = user_data;

  struct scsi_user_cdb suc; 
  uint8_t buf[12] = { 0, };

  uint32_t retval;

  _source_init(_obj);

  memset (&suc, 0, sizeof (struct scsi_user_cdb));

  suc.suc_cdb[0] = 0x43; /* CMD_READ_TOC_PMA_ATIP */
  suc.suc_cdb[1] = 0; /* lba; msf: 0x2 */
  suc.suc_cdb[6] = 0xaa; /* CDROM_LEADOUT */
  suc.suc_cdb[8] = 12; /* ? */
  suc.suc_cdblen = 10;

  suc.suc_data = buf;
  suc.suc_datalen = sizeof (buf);

  suc.suc_timeout = 500;
  suc.suc_flags = SUC_READ;

  if (_scsi_cmd(_obj->fd, &suc, "_stat_size"))
    return 0;

  {
    int i;

    retval = 0;
    for (i = 8; i < 12; i++)
      {
        retval <<= 8;
        retval += buf[i];
      }
  }

  return retval;
}

/*!
  Set the key "arg" to "value" in source device.
*/
static int
_source_set_arg (void *user_data, const char key[], const char value[])
{
  _img_bsdicd_src_t *_obj = user_data;
  
  if (!strcmp (key, "device"))
    {
      if (!value)
	return -2;

      free (_obj->device);
      
      _obj->device = strdup (value);
    }
  else 
    return -1;

  return 0;
}

#endif /* defined(__bsdi__) */

VcdImageSource *
vcd_image_source_new_bsdicd (void)
{
#if defined(__bsdi__)
  _img_bsdicd_src_t *_data;

  vcd_image_source_funcs _funcs = {
    .read_mode2_sector = _read_mode2_sector,
    .stat_size         = _stat_size,
    .free              = _source_free,
    .set_arg           = _source_set_arg
  };

  _data = _vcd_malloc (sizeof (_img_bsdicd_src_t));
  _data->device = strdup ("/dev/cdrom");

  return vcd_image_source_new (_data, &_funcs);
#else 
# error bsdi cd image source only supported under BSDI
  vcd_assert_not_reached ();
  return NULL;
#endif
}



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

#ifndef __VCD_DICT_H__
#define __VCD_DICT_H__

#include <libvcd/vcd_types.h>
#include <libvcd/vcd_obj.h>

struct _dict_t
{
  char *key;
  uint32_t sector;
  uint32_t length;
  void *buf;
  uint8_t flags;
};

static void
_dict_insert (VcdObj *obj, const char key[], uint32_t sector, uint32_t length, 
              uint8_t end_flags)
{
  struct _dict_t *_new_node;

  assert (key != NULL);
  assert (length > 0);

  if ((sector =_vcd_salloc (obj->iso_bitmap, sector, length)) == SECTOR_NIL)
    assert (0);

  _new_node = _vcd_malloc (sizeof (struct _dict_t));

  _new_node->key = strdup (key);
  _new_node->sector = sector;
  _new_node->length = length;
  _new_node->buf = _vcd_malloc (length * ISO_BLOCKSIZE);
  _new_node->flags = end_flags;

  _vcd_list_prepend (obj->buffer_dict_list, _new_node);
}

static 
int _dict_key_cmp (struct _dict_t *a, char *b)
{
  assert (a != NULL);
  assert (b != NULL);

  return !strcmp (a->key, b);
}

static 
int _dict_sector_cmp (struct _dict_t *a, uint32_t *b)
{
  assert (a != NULL);
  assert (b != NULL);

  return (a->sector <= *b && (*b - a->sector) < a->length);
}

static const struct _dict_t *
_dict_get_bykey (VcdObj *obj, const char key[])
{
  VcdListNode *node;

  assert (obj != NULL);
  assert (key != NULL);

  node = _vcd_list_find (obj->buffer_dict_list,
                         (_vcd_list_iterfunc) _dict_key_cmp,
                         (char *) key);
  
  if (node)
    return _vcd_list_node_data (node);

  return NULL;
}

static const struct _dict_t *
_dict_get_bysector (VcdObj *obj, uint32_t sector)
{
  VcdListNode *node;

  assert (obj != NULL);
  assert (sector != SECTOR_NIL);

  node = _vcd_list_find (obj->buffer_dict_list, 
                         (_vcd_list_iterfunc) _dict_sector_cmp, 
                         &sector);

  if (node)
    return _vcd_list_node_data (node);

  return NULL;
}

static uint8_t
_dict_get_sector_flags (VcdObj *obj, uint32_t sector)
{
  const struct _dict_t *p;

  assert (sector != SECTOR_NIL);

  p = _dict_get_bysector (obj, sector);

  if (p)
    return (((sector - p->sector)+1 == p->length)
            ? p->flags : 0);

  return 0;
}

static void *
_dict_get_sector (VcdObj *obj, uint32_t sector)
{
  const struct _dict_t *p;

  assert (sector != SECTOR_NIL);

  p = _dict_get_bysector (obj, sector);

  if (p)
    return ((char *)p->buf) + ((sector - p->sector) * ISO_BLOCKSIZE);

  return NULL;
}

static void
_dict_clean (VcdObj *obj)
{
  VcdListNode *node;

  while ((node = _vcd_list_begin (obj->buffer_dict_list)))
    {
      struct _dict_t *p = _vcd_list_node_data (node);

      free (p->key);
      free (p->buf);

      _vcd_list_node_free (node, true);
    }
}

#endif /* __VCD_DICT_H__ */

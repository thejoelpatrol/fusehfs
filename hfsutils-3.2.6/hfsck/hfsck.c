/*
 * hfsck - tool for checking and repairing the integrity of HFS volumes
 * Copyright (C) 1996-1998 Robert Leslie
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * $Id: hfsck.c,v 1.7 1998/04/11 08:27:08 rob Exp $
 */

# include <stdio.h>

# include "hfsck.h"

# include "ck_mdb.h"
# include "ck_volume.h"
# include "ck_btree.h"
#include <fcntl.h>
#include <unistd.h>

int hfsck(hfsvol *vol)
{
    int log = open("/tmp/hfsck.log", O_WRONLY | O_APPEND | O_CREAT);
    dprintf(log, "func hfsck invoked\n");
    close(log);
  return ck_mdb(vol) || ck_volume(vol) ||
    ck_btree(&vol->ext) || ck_btree(&vol->cat);
}

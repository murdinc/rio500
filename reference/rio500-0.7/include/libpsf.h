/*  ----------------------------------------------------------------------

    Copyright (C) 2000  Cesar Miquel  (miquel@df.uba.ar)

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
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

    ---------------------------------------------------------------------- */


#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>

typedef struct
{
  unsigned short magic;
  unsigned char mode;
  unsigned char height;
} psf_header;

typedef struct
{
  int width, height;
  unsigned char *data;
  FILE *fp;
} psf_font;

psf_font      *psf_load_font (char *filename);
unsigned char *psf_get_char (psf_font *f, char c);
int            psf_write_string (psf_font *f, char *s, unsigned char *d);
void           psf_delete_font (psf_font *f);

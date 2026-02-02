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


#include "libpsf.h"

psf_font *
psf_load_font (char *filename)
{
	FILE *fp;
	psf_header hdr;
	psf_font *font;

	fp = fopen (filename, "r");
	if (fp == NULL )
		return NULL;

	/* Read header anc check its a psf file */
	fread (&hdr, sizeof (psf_header), 1, fp);
	if (hdr.magic != 0x3604 && hdr.magic != 0x0436)
		return NULL;
	
	font = malloc (sizeof (psf_font));
	if (font == NULL)
		return NULL;

	font->width = 8;
	font->height = hdr.height;
	font->fp = fp;

	font->data = malloc (256 * font->height);

	// Read data
	if (font->data)
		fread (font->data, 256 * font->height, 1, fp);
	
	return font;
}

unsigned char *
psf_get_char (psf_font *f, char c)
{
	unsigned int offset = ((unsigned int)(unsigned char)c) * f->height;
	return (f->data + offset);
}

int
psf_write_string (psf_font *f, char *string, unsigned char *dst)
{
  unsigned char *bits, *p;
  int i, j, skip, height;

  height = f->height;
  skip = (16 - height) >> 1;

  p = dst;
  for (i = 0; i < strlen (string); i++)
  {
    memset (p, 0, 16);
    bits = psf_get_char (f, string[i]);
    for (j = 0; j < height; j++)
      p[skip + j] = bits[j];
    p += 16;
  }
  return strlen (string); 
}

void
psf_delete_font (psf_font *f)
{
  if (f->data)
    free (f->data);
  fclose (f->fp);
}

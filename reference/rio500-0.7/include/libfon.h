/*  ----------------------------------------------------------------------

    Copyright (C) 2000  Bruce Tenison (btenison@dibbs.net)
    In the beginning this file was designed to load .psf font files
    and was created by Cesar Miquel (miquel@df.uba.ar)

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

#include <stdio.h>
#include <stdlib.h>

#if defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
#include <sys/param.h>
#else
#include <stdint.h>
#endif

#include "config.h"

#define MZ_HEADER_MAGIC 0x5a4d
#define NE_HEADER_MAGIC 0x454e

typedef struct {
   uint16_t magic;
   uint16_t dummy[29];          /* MZ Header data that we want to skip */
   uint16_t ne_location;        /* Location of the NE Header Offset */
} MZ_Header;

typedef struct {
   uint16_t magic;
   uint16_t dummy1[15];
   uint16_t nonres_name_length;         /* Why isn't this the length of the string? */
   uint16_t dummy2[5];
   uint32_t nonres_name_start;
} NE_Header;

struct Font_header {
   uint16_t	Windows_version;
   uint16_t 	File_sizeL;			/* This looks like the total size */
   uint16_t 	File_sizeH;			/* for this font */
   uint8_t 	Copyright[60];
   uint16_t 	Type_of_font_file;		/* GDI Use.  We won't need this */
   uint16_t 	Nominal_point_size;		/* Hmmm. According to .fnt specs, this
						is correct.  Doesn't seem to be, though */
   uint16_t 	Vertical_resolution;		/* Windows 3.0 files have something weird here */
   uint16_t 	Horizontal_resolution;
   uint16_t 	dummy1;
   uint16_t 	dummy2;
   uint16_t 	dummy3;
   uint16_t 	dummy4;
   uint16_t 	dummy5;
   uint16_t 	dummy6;
   uint16_t 	Point_size;
   uint16_t 	Bytes_per_char_cell;
   uint8_t  	Thirty;				/* What is this?  Always 30? */
   uint8_t 	Avg_point_sizeL;		/* Alignment problems?  How do I fix this?!?!? */
   uint8_t 	Avg_point_sizeH;
   uint8_t 	Max_point_sizeL;			
   uint8_t 	Max_point_sizeH;			
   uint8_t  	Initial_char_code;
   uint8_t 	Last_char_code;			/* This doesn't look right to me.  Every font file
						I've seen has 0xff stored here. */
   uint8_t 	Default_char_code;
   uint8_t 	Break_char_code;		/* Again, this doesn't look right.  All files have
						0x00 stored here */
   uint8_t 	Total_number_of_chars;
   uint16_t 	dummy7;
   uint16_t 	dummy8;
   uint8_t  	dummy9;
   uint8_t 	Offset_to_next_fontL;		/* OK, how do I do this?  These two are */
   uint8_t 	Offset_to_next_fontH;		/* at an odd offset in mem, and the x86
						seems to want to move the mem pointer
						by one for me... */
   uint8_t	dummy10;
   uint8_t	dummy11;
   uint8_t	dummy12;
   uint8_t	dummy13;
   uint8_t	dummy14;
   uint8_t	dummy15;
   uint8_t	dummy16;
   uint8_t	dummy17;
   uint8_t	dummy18;
   uint8_t	dummy19;
   uint8_t	dummy20;
};

struct Win_charmap
{
   int length;
   long First_bitmap_offset;
   unsigned char *data;
};

struct Win_bitmap
{
   int length;
   unsigned char *data;
};

struct fon_font
{
  int font_number;
  int number_of_fonts;
  struct Font_header *Header;
  struct Win_charmap *Charmap;
  struct Win_bitmap *Bitmap;
};

struct fon_font * fon_load_font (char *filename);
unsigned char *fon_get_char (struct fon_font *f, char c);
int            fon_write_string (struct fon_font *f, char *s, unsigned char *d);
void           fon_delete_font (struct fon_font *f);
void           fon_skip_zeros (FILE *f);
void           fon_skip_nonzeros (FILE *f);

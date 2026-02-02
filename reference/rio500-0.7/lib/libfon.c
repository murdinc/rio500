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

#include "libfon.h"

#ifdef WORDS_BIGENDIAN
#include <byteswap.h>
#endif

struct fon_font *
fon_load_font (char *filename)
{
	FILE *fp;
	MZ_Header MZ_hdr;
	NE_Header NE_hdr;
	
	int i;

#ifdef DEBUG_MASSIVE
	int x;
#endif

	unsigned int number_of_fonts=0;
	long header_location=0, bitmap_start_location=0;
	long charmap_start_location=0;
	long end_location=0, bitmap_start_offset=0;
	struct fon_font *font=NULL;

#ifdef DEBUG_MASSIVE
	printf("Debugging set\n");

	#ifdef WORDS_BIGENDIAN
	printf ("WORDS_BIGENDIAN defined.\n");
	#endif

#endif
	fp = fopen (filename, "r");
	if (fp == NULL )
		return NULL;

	/* Read header and check if its a fon file */
	fread (&MZ_hdr, sizeof (MZ_Header), 1, fp);

#ifdef WORDS_BIGENDIAN
	MZ_hdr.magic = bswap_16(MZ_hdr.magic);
        MZ_hdr.dummy[29] = bswap_16(MZ_hdr.dummy[29]);
	//MZ_hdr.ne_location = bswap_16(MZ_hdr.ne_location); 
#endif

/* ne_location is a 16bit quantity,l_e.  It appears fseek below wants 
   the location in l_e so it isn't being swapped . -- Keith */
        
	if (MZ_hdr.magic != MZ_HEADER_MAGIC) {
		printf("Invalid MZ Header Magic! %x\n", MZ_hdr.magic);
		return NULL;
	}

	/* So far, so good.  It's a MZ exe, at least*/
	/* Grab the ne_location and seek!*/
	(void)fseek(fp, MZ_hdr.ne_location, SEEK_SET);

	/* Read in the NE Header and check */
	fread (&NE_hdr, sizeof (NE_Header), 1, fp);
	
	#ifdef WORDS_BIGENDIAN
	NE_hdr.magic = bswap_16(NE_hdr.magic);
	NE_hdr.dummy1[15] = bswap_16(NE_hdr.dummy1[15]);
	
	//NE_hdr.nonres_name_length = bswap_16(NE_hdr.nonres_name_length);
	/* fseek seems to like l_e quantities */
	
	/* Ok . . I'm an idiot, nonres_name_start was 32bit, can use bswap */
	
	NE_hdr.nonres_name_start=bswap_32(NE_hdr.nonres_name_start);
	#endif 
	
	if (NE_hdr.magic != NE_HEADER_MAGIC ) {
		printf("Invalid NE Header Magic! %x\n", NE_hdr.magic);
		return NULL;
	}
	
	/* Good, it's a NE type executable. */
  	/* Seek the Nonresident names table. */
	(void)fseek(fp, NE_hdr.nonres_name_start, SEEK_SET); 

	/* First byte of the nonres table is the length of the
	text.  We are skipping the nonres_name*/
	(void)fseek(fp, NE_hdr.nonres_name_length, SEEK_CUR);

	/* It looks as if the header is now padded with zeros 
	I initially thought that this was to make the font header 
	start at a multiple of 0x10 boundry, but looking at some
	font files, this isn't the case some of the time.  Just skip
	all zeros.*/
	(void)fon_skip_zeros(fp);

	/* Now, the file pointer SHOULD point to the first
	font header.  Preceeding it are a couple of bytes 
	that tell us how many fonts are in the file */
	fread(&number_of_fonts, 2, 1, fp);
	
	#ifdef WORDS_BIGENDIAN
	number_of_fonts = bswap_32(number_of_fonts);
	#endif 
	/* font info stored in 32bit 32bit 32bit l_e format for win fonts */
	/* need to swap to use on ppc */

	/* Here are a couple of bytes that I have no clue about.
	skipping them */
	fseek(fp, 2, SEEK_CUR);

	/* Finally, we're at the main font header location */
	header_location = ftell(fp);
	/* Allocate Memory */
	/* Bruce, I took the allocations out of the ifs to make it easier to print out
	   addresses when I was debugging . . hope you don't mind . . we could put them
	   back as they were now that things are running . .  keith */
	font = malloc(sizeof(struct fon_font));
	if (font==NULL)
	{
                printf("Could not allocate memory for font headers!\n");
                return NULL;
        }
	font->Header = calloc(number_of_fonts,sizeof(struct Font_header));
	if (font->Header==NULL)
        {
		printf("Falling in font->header\n");
		printf("Could not allocate memory for font headers!\n");
		free(font);
		return NULL;

        }
	font->Charmap=calloc(number_of_fonts,sizeof(struct Win_charmap));
	if (font->Charmap==NULL)
	{
		printf("Could not allocate memory for charmaps!\n");
		free(font->Header);
		free(font);
		return NULL;
	}
	font->Bitmap = calloc(number_of_fonts,sizeof(struct Win_bitmap));
	if (font->Bitmap==NULL)
	{
		printf("Could not allocate memory for bitmaps!\n");
		free(font->Charmap);
		free(font->Header);
		free(font);
		return NULL;
	}

	font->number_of_fonts = number_of_fonts;
	/* Now, we should have plenty of space allocated to read in all of the Headers */
	/* Read them into memory */

	for (i=0; i<font->number_of_fonts; i++) {
		(void)fread(&font->Header[i], sizeof( struct Font_header ), 1, fp);
	/* Skip the fontname (since it is a variable size, with no discernable
	entry anywhere to describe that size... */
		(void)fon_skip_nonzeros(fp);
		(void)fseek(fp, 3, SEEK_CUR);
	
	/* Do any sort of byte swapping necessary for big endian after reading data */

	#ifdef WORDS_BIGENDIAN
	font->Header[i].Windows_version = bswap_16(font->Header[i].Windows_version);
	font->Header[i].Bytes_per_char_cell = bswap_16(font->Header[i].Bytes_per_char_cell);
	font->Header[i].File_sizeH = bswap_16(font->Header[i].File_sizeH);
	font->Header[i].File_sizeL = bswap_16(font->Header[i].File_sizeL);
	font->Header[i].Nominal_point_size = bswap_16(font->Header[i].Nominal_point_size);
	font->Header[i].Vertical_resolution = bswap_16(font->Header[i].Vertical_resolution);
	font->Header[i].Horizontal_resolution = bswap_16(font->Header[i].Horizontal_resolution);
	#endif

	}
#ifdef DEBUG_MASSIVE
	for (i=0; i<font->number_of_fonts; i++) {
		printf("Debug: Windows_version[%d] = %x\n", i, font->Header[i].Windows_version);
		printf("Debug: File_size[%d] = %x\n", i, font->Header[i].File_sizeH*65536+font->Header[i].File_sizeL);
		printf("Debug: Copyright[%d] = %s\n", i, font->Header[i].Copyright);
		printf("Debug: Type_of_font_file[%d] = %x\n", i, font->Header[i].Type_of_font_file);
		printf("Debug: Nominal_point_size[%d] = %x\n", i, font->Header[i].Nominal_point_size);
		printf("Debug: Vertical_resolution[%d] = %x\n", i, font->Header[i].Vertical_resolution);
		printf("Debug: Horizontal_resolution[%d] = %x\n", i, font->Header[i].Horizontal_resolution);
		printf("Debug: Point_size[%d] = %x\n", i, font->Header[i].Point_size);
		printf("Debug: Bytes_per_char_cell[%d] = %x\n", i, font->Header[i].Bytes_per_char_cell);
		printf("Debug: Thirty[%d] = %x\n", i, font->Header[i].Thirty);
		printf("Debug: Avg_point_size[%d] = %x\n", i, font->Header[i].Avg_point_sizeH*256+font->Header[i].Avg_point_sizeL);
		printf("Debug: Max_point_size[%d] = %x\n", i, font->Header[i].Max_point_sizeH*256+font->Header[i].Max_point_sizeL);
		printf("Debug: Initial_char_code[%d] = %x\n", i, font->Header[i].Initial_char_code);
		printf("Debug: Last_char_code[%d] = %x\n", i, font->Header[i].Last_char_code);
		printf("Debug: Default_char_code[%d] = %x\n", i, font->Header[i].Default_char_code);
		printf("Debug: Break_char_code[%d] = %x\n", i, font->Header[i].Break_char_code);
		printf("Debug: Total_number_of_chars[%d] = %x\n", i, font->Header[i].Total_number_of_chars);
		printf("Debug: Offset_to_next_font[%d] = %x\n", i, font->Header[i].Offset_to_next_fontH*256+font->Header[i].Offset_to_next_fontL);
		printf("\n");
	}
#endif

	for (i=0; i<font->number_of_fonts; i++) {
		(void)fon_skip_zeros(fp);

	/* Back up 1...  Need that previous zero... */
		(void)fseek(fp, -1, SEEK_CUR);
		header_location = ftell(fp);

		/* Now, we are at a repeat of a header.  Since it is an exact duplicate,
		skip it and begin reading the pertinant data into memory */
		(void)fseek(fp, 0x71, SEEK_CUR);

		/* At this point, the file pointer points to a word that provides us an offset
		from the header_location to the actual bitmap data start location */
		(void)fread(&bitmap_start_offset, 2, 1, fp);

		#ifdef WORDS_BIGENDIAN
		bitmap_start_offset = bswap_32(bitmap_start_offset);
		#endif
		
		/* b_s_off comes out of .fon in l_e, need to convert */

		font->Charmap[i].First_bitmap_offset = bitmap_start_offset;

		bitmap_start_location = header_location + bitmap_start_offset;
		end_location = header_location + (font->Header[i].Offset_to_next_fontH*256+font->Header[i].Offset_to_next_fontL);

		
		if (font->Header[i].Windows_version == 0x200)
			(void)fseek(fp, 0x3, SEEK_CUR);
		else
			(void)fseek(fp, 0x21, SEEK_CUR);

		charmap_start_location = ftell(fp);

#ifdef DEBUG_MASSIVE
	printf("header_location %lx\n", header_location);
	printf("charmap_start_loccation %lx\n", charmap_start_location);
	printf("bitmap_start_offset %lx\n", bitmap_start_offset);
	printf("bitmap_start_location %lx\n", bitmap_start_location);
	printf("end_location %lx\n", end_location);
#endif

		/* OK, now we need to read the character maps into memory.  This requires some logic
		on our part, since there are two types of character maps.  One type is Windows 2.0
		format and the other is Windows 3.0 format. */
		if (font->Header[i].Windows_version == 0x200) {
		/* Windows 2.00 format */
		/* Now, load in the character table and the bitmap data into structs */
			font->Charmap[i].length = bitmap_start_location - charmap_start_location;
			font->Charmap[i].data = malloc(font->Charmap[i].length);
			(void)fread(font->Charmap[i].data, font->Charmap[i].length, 1, fp);
#ifdef DEBUG_MASSIVE
			for (x=0; x<font->Charmap[i].length; x++) {
				printf("%.2x ", font->Charmap[i].data[x]);
				if (x%16 == 15)
					printf("\n");
			}
			printf("\n");
#endif
		/* OK. Charmap is in memory, now. Read in Bitmap */
			font->Bitmap[i].length = end_location - bitmap_start_location;
			font->Bitmap[i].data = malloc(font->Bitmap[i].length);
			(void)fread(font->Bitmap[i].data, font->Bitmap[i].length, 1, fp);
		/* OK.  Bitmap data is in memory (Finally!) */
#ifdef DEBUG_MASSIVE
			for (x=0; x<font->Bitmap[i].length; x++) {
				printf("%.2x ", font->Bitmap[i].data[x]);
				if (x%16 == 15)
					printf("\n");
			}
			printf("\n");
#endif
		} else {
		/* Must be Windows 3.00 format */
			printf("Windows 3.0 Not currently supported\n");
			free(font->Bitmap);
			free(font->Charmap);
			free(font->Header);
			free(font);
			return NULL;
		}

		/* Lastly, let's read the bitmap data into memory */

		/* Seek next font structure */
		(void)fseek(fp, end_location+1, SEEK_SET);
	/* Skip the fontname (since it is a variable size, with no discernable
	entry anywhere to describe that size... */
		(void)fon_skip_nonzeros(fp);
		(void)fseek(fp, 3, SEEK_CUR);
	}

	fclose(fp);
	return font;
}

void
fon_skip_zeros( FILE * fp )
{
	uint8_t data=0x00;	
	long file_location = 0;
	while ( data == 0x00 )
		(void)fread(&data, 1, 1, fp);
	file_location = ftell(fp);
	(void)fseek(fp, file_location-1, SEEK_SET);
}

void
fon_skip_nonzeros( FILE *fp )
{
	uint8_t data=0x01;
	long file_location = 0;
        while ( data != 0x00 )
                (void)fread(&data, 1, 1, fp);
	file_location = ftell(fp);
	(void)fseek(fp, file_location-1, SEEK_SET);
}

unsigned char *
fon_get_char (struct fon_font *f, char c)
{
	long code_offset=0, bitmap_offset=0;
	/* OK, we have a char code.  Now, find the difference between this
	code and the start code in the Header */
	code_offset = c - f->Header[f->font_number].Initial_char_code;

	/* Now, we have a char offset, take into account the Win 2.0 and 3.0
	font differences, and find the offset into the Charmap */
	if (f->Header[f->font_number].Windows_version == 0x200) {
		code_offset = code_offset * 4 + 2;
		bitmap_offset = f->Charmap[f->font_number].data[code_offset+1]*256+f->Charmap[f->font_number].data[code_offset];
		bitmap_offset -= f->Charmap[f->font_number].First_bitmap_offset;
	} else {
		printf("Windows 3.0 files not supported (yet)\n");
		return NULL;
	}

	return (f->Bitmap[f->font_number].data + bitmap_offset);
}

int
fon_get_char_width (struct fon_font *f, char c)
{
	long code_offset;
	int width;

	/* OK, we have a char code.  Now, find the difference between this
	code and the start code in the Header */
	code_offset = c - f->Header[f->font_number].Initial_char_code;

	/* Now, we have a char offset, take into account the Win 2.0 and 3.0
	font differences, and find the offset into the Charmap */
	if (f->Header[f->font_number].Windows_version == 0x200) {
		code_offset = code_offset * 4;
		width = (int)f->Charmap[f->font_number].data[code_offset];
	} else {
		printf("Windows 3.0 files not supported (yet)\n");
		return 8;
	}

	if (width < 0 || width > 8)
	  return (8);
        else
	  return (width);
}

int
fon_write_string (struct fon_font *f, char *string, unsigned char *dst)
{
  unsigned char *bits, *p;
  unsigned short twobytes;
  int i, j, skip, height;
  int x, offset, xmod, w;

  height = f->Header[f->font_number].Bytes_per_char_cell;
  skip = (16 - height) >> 1;

  p = dst;
  memset (p, 0, 1536);
  x = 0;
  for (i = 0; i < strlen (string); i++)
  {
    offset = x / 8;
    xmod   = x % 8;
    p      = &dst[16 * offset];
    bits   = fon_get_char (f, string[i]);
    w      = fon_get_char_width (f, string[i]);
    for (j = 0; j < height; j++)
    {
      twobytes = (0x00 | bits[j]) << (16 - w - xmod);
      p[skip + j] |= (bits[j] >> xmod) & 0xff;
      if (xmod + w > 8)
        p[skip + j + 16] |= bits[j] << (w - (xmod + w - 8));
      /*p[skip+j] = bits[j];*/
    }

    x += w;
    /* Verify string isn't too long and truncate if it is */
    /* The name cannot exceed 96x8 pixels since 96 chars * 16 lines = 1536 */
    if ( (x / 8) > 95 )
      break;
  }
  return i;
}

void
fon_delete_font (struct fon_font *f)
{
  if (f->Header)
    free (f->Header);
  if (f->Charmap)
    free (f->Charmap);
  if (f->Bitmap)
    free (f->Bitmap);
  if (f)
    free (f);
}

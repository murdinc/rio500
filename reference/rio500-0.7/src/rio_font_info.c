/*  ----------------------------------------------------------------------

    Copyright (C) 2000  Bruce Tenison (btenison@dibbs.net)

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

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#if defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
#include <sys/param.h>
#else
#include <stdint.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>
#include "libfon.h"
#include "getopt.h"
void get_some_switches (int argc, char *argv[]);

static char infile[512];

int main( int argc, char *argv[] )
{
   FILE *fpin;
   struct fon_font *font;
   int i;

/*
** determine infiles 
*/
  get_some_switches(argc,argv);
  
   if ( argc != optind )
   {
      strcpy( infile,  argv[optind] );
   }
   else
   {
      puts( "\nUsage: rio_font_info file" );
      exit(0);
   }

   if ( ( fpin = fopen( infile, "rb" ) ) == 0 )
   {
      printf( "font_info: could not open '%s' for reading\n", infile );
      exit( 1 );
   }

   font = fon_load_font(infile);

   printf("Number of fonts in file = %d\n", font->number_of_fonts);
   for (i=0; i<font->number_of_fonts; i++) {
	printf("Windows_version[%d] = %x\n", i, font->Header[i].Windows_version);
	printf("File_size?[%d] = %x\n", i, font->Header[i].File_sizeH*65536+font->Header[i].File_sizeL);
	printf("Copyright[%d] = %s\n", i, font->Header[i].Copyright);
	printf("Type_of_font_file?[%d] = %x\n", i, font->Header[i].Type_of_font_file);
	printf("Nominal_point_size[%d] = %x\n", i, font->Header[i].Nominal_point_size);
	printf("Vertical_resolution[%d] = %x\n", i, font->Header[i].Vertical_resolution);
	printf("Horizontal_resolution[%d] = %x\n", i, font->Header[i].Horizontal_resolution);
	printf("Point_size[%d] = %x\n", i, font->Header[i].Point_size);
	printf("Bytes_per_char_cell[%d] = %x\n", i, font->Header[i].Bytes_per_char_cell);
	printf("Thirty?[%d] = %x\n", i, font->Header[i].Thirty);
	printf("Avg_point_size[%d] = %x\n", i, font->Header[i].Avg_point_sizeH*256+font->Header[i].Avg_point_sizeL);
	printf("Max_point_size[%d] = %x\n", i, font->Header[i].Max_point_sizeH*256+font->Header[i].Max_point_sizeL);
	printf("Initial_char_code[%d] = %x\n", i, font->Header[i].Initial_char_code);
	printf("Last_char_code?[%d] = %x\n", i, font->Header[i].Last_char_code);
	printf("Default_char_code?[%d] = %x\n", i, font->Header[i].Default_char_code);
	printf("Break_char_code?[%d] = %x\n", i, font->Header[i].Break_char_code);
	printf("Total_number_of_chars[%d] = %x\n", i, font->Header[i].Total_number_of_chars);
	printf("Offset_to_next_font[%d] = %x\n", i, font->Header[i].Offset_to_next_fontH*256+font->Header[i].Offset_to_next_fontL);
	printf("\n");
    }


   font->font_number = 0;

   fon_delete_font(font);
   return 0;
}

static char const shortopts[] = "hv";

static struct option const longopts[] =
{
  {"version", no_argument, NULL, 'v'},
  {"help", no_argument, NULL, 'h'},
  {NULL, no_argument, NULL, 0}
};

static char const *const option_help[] =
{
"",
"Miscellaneous options:",
"",
"  -v  --version     Output version info.",
"  -h  --help        Output this help.",
"",
"Report bugs to <rio500-devel@lists.sourceforge.net>.",
0
};


/* Process switches and filenames.  */

void
get_some_switches (int argc, char *argv[])
{
    register int optc;
    char const * const *p;

    if (optind == argc)
        return;
    while ((optc = getopt_long (argc, argv, shortopts, longopts, (int *) 0))
           != -1) {
         switch (optc) {
            case 'v':
                printf("\nrio_font_info -- version %s\n",VERSION);
                exit(0);
                break;
            case 'h':
                puts("\nUsage: rio_font_info file");
                for (p=option_help;  *p ;  p++)
                  fprintf (stderr, "%s\n", *p);
                exit(0);
                break;
            default:
                puts("\nUsage: rio_font_info file");
                for (p=option_help;  *p ;  p++)
                  fprintf (stderr, "%s\n", *p);
                exit(0);
                break;

         }
    }
}

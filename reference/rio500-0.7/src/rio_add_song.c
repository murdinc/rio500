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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <ctype.h>
#include <time.h>
#include <errno.h>
#include "getopt.h"

#include "librio500.h"

#ifndef TRUE

#define TRUE 				1
#define FALSE 				0

#endif /* TRUE */

#define STR_RIFF (((((('R' << 8) | 'I') << 8) | 'F') << 8) | 'F')
#define STR_WAVE (((((('W' << 8) | 'A') << 8) | 'V') << 8) | 'E')
#define STR_MPEG (((((('M' << 8) | 'P') << 8) | 'E') << 8) | 'G')
#define STR_fmt  (((((('f' << 8) | 'm') << 8) | 't') << 8) | ' ')
#define STR_fact (((((('f' << 8) | 'a') << 8) | 'c') << 8) | 't')
#define STR_data (((((('d' << 8) | 'a') << 8) | 't') << 8) | 'a')

void  usage (char *progname);
#ifndef WITH_USBDEVFS
int   write_song (int rio_dev, char *filename, int card_number);
#else
int   write_song (struct usbdevice *rio_dev, char *filename, int card_number);
#endif
int   file_size (char *filename);
char *strip_path (char *f);
#ifdef USE_ID3_TAGS
void
get_some_switches (int argc, char *argv[], int *font_number, int *folder_num, char *display_format, int *card_number, int *card_auto);
GList *add_song_to_list (GList *songs, char *filename, int offset, char *font_name, int font_number, char *display_format);
#else
void
get_some_switches (int argc, char *argv[], int *font_number, int *folder_num, int *card_number, int *card_auto);
GList *add_song_to_list (GList *songs, char *filename, int offset, char *font_name, int font_number);
#endif
static unsigned long get_frame_header(FILE *fp);
static int is_frame_header(unsigned long fh);
static int mp3_read_long(unsigned long *fhp, FILE *fp);
static int mp3_read_byte(unsigned long *fhp, FILE *fp);

char *font_name = DEFAULT_FONT_PATH;
char *temp_name;

/* Support for displaying id3 tag information
 *   There codes are very experimental, will safely be changed... */
#ifdef USE_ID3_TAGS
/* borrowed from XMMS's code */
typedef struct
{
       char tag[3]; /* always "TAG": defines ID3v1 tag 128 bytes before EOF */
       char title[30];
       char artist[30];
       char album[30];
       char year[4];
       char comment[30];
       unsigned char genre;
} id3v1tag;

#define GENRE_MAX 0x94      /* XMMS 1.0.1 (Input/mpg123/mpg123.h */
const char *mpg123_id3_genres[GENRE_MAX] =
{
       "Blues", "Classic Rock", "Country", "Dance", "Disco", "Funk",
       "Grunge", "Hip-Hop", "Jazz", "Metal", "New Age", "Oldies", "Other",
       "Pop", "R&B", "Rap", "Reggae", "Rock", "Techno", "Industrial",
       "Alternative", "Ska", "Death Metal", "Pranks", "Soundtrack",
       "Euro-Techno", "Ambient", "Trip-Hop", "Vocal", "Jazz+Funk", "Fusion",
       "Trance", "Classical", "Instrumental", "Acid", "House", "Game",
       "Sound Clip", "Gospel", "Noise", "Alt", "Bass", "Soul", "Punk", "Space",
       "Meditative", "Instrumental Pop", "Instrumental Rock", "Ethnic",
       "Gothic", "Darkwave", "Techno-Industrial", "Electronic", "Pop-Folk",
       "Eurodance", "Dream", "Southern Rock", "Comedy", "Cult", "Gangsta Rap",
       "Top 40", "Christian Rap", "Pop/Funk", "Jungle", "Native American",
       "Cabaret", "New Wave", "Psychedelic", "Rave", "Showtunes", "Trailer",
       "Lo-Fi", "Tribal", "Acid Punk", "Acid Jazz", "Polka", "Retro", "Musical",
       "Rock & Roll", "Hard Rock", "Folk", "Folk/Rock", "National Folk", "Swing",
       "Fast-Fusion", "Bebob", "Latin", "Revival", "Celtic", "Bluegrass",
       "Avantgarde", "Gothic Rock", "Progressive Rock", "Psychedelic Rock",
       "Symphonic Rock", "Slow Rock", "Big Band", "Chorus", "Easy Listening",
       "Acoustic", "Humour", "Speech", "Chanson", "Opera", "Chamber Music",
       "Sonata", "Symphony", "Booty Bass", "Primus", "Porn Groove", "Satire",
       "Slow Jam", "Club", "Tango", "Samba", "Folklore", "Ballad", "Power Ballad",
       "Rhythmic Soul", "Freestyle", "Duet", "Punk Rock", "Drum Solo", "A Cappella",
       "Euro-House", "Dance Hall", "Goa", "Drum & Bass", "Club-House", "Hardcore",
       "Terror", "Indie", "BritPop", "Negerpunk", "Polsk Punk", "Beat",
       "Christian Gangsta Rap", "Heavy Metal", "Black Metal", "Crossover",
       "Contemporary Christian", "Christian Rock", "Merengue", "Salsa",
       "Thrash Metal", "Anime", "JPop", "Synthpop"
};
       
char     *get_display_string(char *display_format, char *filename);
id3v1tag *get_id3info_v1(char *filename);
char     *strip_tailspace(char *string);
       
#define  DEFAULT_DISPLAY_FORMAT     "%7.%9"
#define  DISPLAY_FORMAT_LEN         256
#define  DISPLAY_STRING_LEN           DISPLAY_FORMAT_LEN
#endif

void
usage (char *progname)
{
  printf ("\nusage: %s [OPTIONS] <file1.mp3> . . . <fileN.mp3>\n", progname);
  printf ("\n [OPTIONS]  Try --help for more information");
  printf ("\n <fileN.mp3> is the name of the file whose content");
  printf ("\n we want to upload.  Adding multiple songs is now");
  printf ("\n supported.");
  printf("\n\n");
return;
}

int
main(int argc, char *argv[])
{
  int               retries, song_location, new_size;
  int               folder_block_offset, song_block_offset;
  int               folder_num, font_number, card_number, card_auto;
  int 		    mem_left,filesize,card_changed;
  GList            *folders, *songs;
  mem_status       *mem;
  folder_entry     *f_entry;
  char             *filename;
#ifdef USE_ID3_TAGS
  char display_format[DISPLAY_FORMAT_LEN] = DEFAULT_DISPLAY_FORMAT;
#endif

#ifndef WITH_USBDEVFS
  int              rio_dev;
#else
  struct usbdevice   *rio_dev;

  rio_dev = malloc(sizeof(struct usbdevice));
#endif

  /* set defaults, can be overridden with switches*/
  /* Tomoaki . . set the default value for display here (%7,%9) */

  card_number = 0;
  card_auto = 0;
  folder_num = 0;
  font_number = 0;
  mem_left = 0;
  folders = NULL;

#ifdef USE_ID3_TAGS
  get_some_switches(argc,argv,&font_number,&folder_num,
                    display_format,&card_number,&card_auto);

#else
  get_some_switches(argc,argv,&font_number,&folder_num,&card_number,&card_auto);
#endif

  /* quick fix (by Tomoaki) */
  if (optind == argc) {
      printf ("\nAt least, one mp3 file must be specified! \n\n");
      exit (-1);
  }
  
  if (strcmp(font_name,DEFAULT_FONT_PATH) == 0 )
  {
        new_size = strlen(DEFAULT_FONT_PATH)+strlen(DEFAULT_FON_FONT)+1;
        temp_name=(char *)malloc(new_size);
        strcpy(temp_name,font_name);
        strcat(temp_name,DEFAULT_FON_FONT);
        font_name=temp_name;
  }

  /* Open connection to rio */
#ifndef WITH_USBDEVFS
  rio_dev = open (DEFAULT_DEV_PATH, O_RDWR);
  if (rio_dev < 0)
    {
      printf ("\nVerify that the rio module is loadad and your Rio is \n");
      printf ("connected and powered up.\n\n");
      exit (-1);
    }

   /* Init communication with rio */
   init_communication (rio_dev);
#else
   if(!(rio_dev = init_communication())) {
     printf("init_communication() failed!\n");
     free(rio_dev);
     return -1;
   }
#endif

   
  while (optind < argc)  /* loop through filenames and add */
  {
        filename = argv[optind++];

 /* Make sure there's enough space left */
   /* Sometimes quert_mem_left returns 0 but there really is space in
      the device. So... try 3 times and make sure that it really is
      returning 0. */
   retries = 3;
   while (retries-- > 0)
   {
     mem_left = query_mem_left (rio_dev,card_number);
     send_command(rio_dev,0x42,0,0);
     if (mem_left > 0) break;
   }
     filesize = file_size(filename);
     if (filesize > mem_left && card_auto && card_number==0) {
        printf("\nNot enough room in internal memory\n");
	printf("Autofill has been set\n");
	printf("Trying external smartmedia card\n");
	printf("Setting folder number = 0 on external card\n");
	folder_num = 0;	
	card_number++;
	card_changed = 1;
	mem_left = query_mem_left(rio_dev,card_number);
     }
   if (filesize > mem_left)
   {
      printf ("Not enough space left in rio for %s.\n",filename);
      goto try_next;
      finish_communication (rio_dev);
      exit (0);
   }
   
   mem_left -= filesize;

   /* Read folder & song block */
   folders = read_folder_entries (rio_dev,card_number);

   if (g_list_length(folders) == 0) {
	if (card_number == 1) {
        printf("At least one folder must be created on the smartmedia card before adding songs\n");
	}
	else {
	printf("At least one folder must be created in internal memory before adding songs\n");
        }
	finish_communication (rio_dev);
	exit(0);
   }	

   if ( folder_num > g_list_length (folders)-1 )
     folder_num = 0;
   songs   = read_song_entries ( rio_dev, folders, folder_num,card_number); 
   /* Write the song to the Rio */
   song_location = write_song (rio_dev, filename,card_number);
   
   /* Add an entry to the song block */
#ifdef USE_ID3_TAGS
   songs = add_song_to_list ( songs, filename, song_location, font_name, font_number, display_format);
#else
   songs = add_song_to_list ( songs, filename, song_location, font_name, font_number);
#endif
   if (songs == NULL)
   {
     fprintf (stderr, 
        "Couldn't upload file. No more song entries or incorrect filename.\n");
     finish_communication (rio_dev);
#ifndef WITH_USBDEVFS
     close (rio_dev);
#endif
     exit (0);
   }

   /* Write song block to the correct folder */
   write_song_entries (rio_dev, folder_num, songs,card_number );
   send_command (rio_dev, 0x42, 0, 0);
   send_command (rio_dev, 0x42, 0, 0);

   song_block_offset = send_command (rio_dev, 0x43, 0x0, 0x0);

#ifdef DEBUG
   fprintf (stderr, "Song block written to 0x%04x\n", song_block_offset);
#endif

   /* Now write the folder block again */
   f_entry = (folder_entry *) ((GList *) g_list_nth (folders, folder_num))->data; 
   f_entry->offset = song_block_offset;
   f_entry->fst_free_entry_off += 0x800;

/*
  Keith: it's no longer needed to swap data in folder and song entries
  because now the functions which read and write these entries do the
  byte swapping so the structures are endian correct (i hope).
*/
   write_folder_entries ( rio_dev, folders,card_number );
   send_command (rio_dev, 0x42, 0, 0);
   send_command (rio_dev, 0x42, 0, 0);
   folder_block_offset = send_command (rio_dev, 0x43, 0x0, 0x0);

#ifdef DEBUG
   fprintf (stderr, "Folder block written to 0x%04x\n", folder_block_offset);
#endif

   /* Tell Rio where the root folder block is. */
   send_folder_location (rio_dev, folder_block_offset, folder_num,card_number);

   /* Not really sure what this does */
   send_command (rio_dev, 0x58, 0x0, card_number);

try_next:
   } /* end of add file loop */

   /* Close device */
   finish_communication (rio_dev);
#ifndef WITH_USBDEVFS
   close (rio_dev);
#endif
   exit (0);
}

#ifdef USE_ID3_TAGS

GList *
add_song_to_list (GList *songs, char *filename, int offset, char *font_name, int font_number, char *display_format)

#else

GList *
add_song_to_list (GList *songs, char *filename, int offset, char *font_name, int font_number)
#endif
{
  char smiley[] = {0x00, 0x00, 0x00, 0x00, 
                   0x3e, 0x41, 0x94, 0x80,
                   0xa2, 0x9d, 0x41, 0x3e,
		   0x00, 0x00, 0x00, 0x00, 
		   0x00, 0x00, 0x00, 0x00, 
                   0x00, 0x00, 0x80, 0x80,
                   0x80, 0x80, 0x00, 0x00,
		   0x00, 0x00, 0x00, 0x00}; 

  FILE                *fp;
  rio_bitmap_data     *bitmap;
  GList               *new_entry;
  int                  size;
  char                *striped_name;
  song_entry          *entry;
#ifdef USE_ID3_TAGS
  char                *display_string;
#endif
  
  fp = fopen (filename, "r");
  if (fp == NULL)
    return NULL;

  size = file_size (filename);

  /* Fill file info */
  entry = (song_entry *) calloc (sizeof (song_entry), 1);
  entry->offset = (WORD)  offset;
  entry->length = (DWORD) size;
  entry->dunno3 = (WORD) 0x0020;
  entry->mp3sig = (DWORD) get_frame_header(fp);
  entry->time   = (DWORD) time (NULL);
  fclose (fp);

#ifdef USE_ID3_TAGS
  if ((striped_name = get_display_string (display_format, filename)) == NULL)
    striped_name = strip_path (filename);
  
#else
  striped_name = strip_path (filename);

#endif

  bitmap = bitmap_data_new (striped_name, font_name, font_number);
  if (bitmap)
  {
    memcpy (&entry->bitmap, bitmap, sizeof (rio_bitmap_data));
  } else {
    entry->bitmap.num_blocks = 2;
    memcpy (entry->bitmap.bitmap, smiley, 0x20);
  }
  sprintf (entry->name1, "%s", striped_name);
  sprintf (entry->name2, "%s", striped_name);

  new_entry = g_list_append (songs, entry);

#ifdef USE_ID3_TAGS
  /* free ( display_string ); */
#endif
  
  return g_list_first (new_entry);
}

static unsigned long
get_frame_header(FILE *fp)
{
	unsigned long fh = 0;
	int i;

	if (!mp3_read_long(&fh, fp)) 
		return 0;
	if (fh == STR_RIFF) {
riff:		if (!mp3_read_long(&fh, fp)) 
			return 0;
		if (!mp3_read_long(&fh, fp)) 
			return 0;
		if (fh != STR_MPEG) {
			if (fh != STR_WAVE) {
				goto search;
			}
			if (!mp3_read_long(&fh, fp)) 
				return 0;
			if (fh != STR_fmt) {
				goto search;
			}
			if (!mp3_read_long(&fh, fp)) 
				return 0;
			if (fh > 4096) {
				goto search;
			}
			for (i = fh; i--; ) 
				if (getc(fp) == EOF) 
					return 0;
		}
		if (!mp3_read_long(&fh, fp)) 
			return 0;
		if (fh == STR_fact) {
			if (!mp3_read_long(&fh, fp)) 
				return 0;
			if (fh > 4096) {
				goto search;
			}
			for (i = fh; i--; ) 
				if (getc(fp) == EOF) 
					return 0;
			if (!mp3_read_long(&fh, fp)) 
				return 0;
		}
		if (fh != STR_data) {
			goto search;
		}
		if (!mp3_read_long(&fh, fp)) 
			return 0;
		if (!mp3_read_long(&fh, fp)) 
			return 0;
	}

search:	i = 65536;
	while (!is_frame_header(fh)) {
		if (fh == STR_RIFF) 
			goto riff;
		if (i-- == 0 || !mp3_read_byte(&fh, fp)) 
			return 0;
	}
	return GUINT32_SWAP_LE_BE(fh);
}

static int
is_frame_header(unsigned long fh)
{
	return
		(fh & 0xffe00000) == 0xffe00000 &&
		((fh >> 17) & 3) != 0 &&
		((fh >> 12) & 0xf) != 0xf &&
		((fh >> 10) & 0x3) != 0x3 &&
		(fh & 0xffff0000) != 0xfffe0000;
}

static int
mp3_read_long(unsigned long *fhp, FILE *fp)
{
	return
		mp3_read_byte(fhp, fp) && mp3_read_byte(fhp, fp) &&
		mp3_read_byte(fhp, fp) && mp3_read_byte(fhp, fp);
}

static int
mp3_read_byte(unsigned long *fhp, FILE *fp)
{
	int n = getc(fp);

	if (n == EOF) return FALSE;
	*fhp = (*fhp << 8) | n;
	return TRUE;
}

#ifdef USE_ID3_TAGS
#define GET_ID3TAG( _TAGNAME )                                            \
  _TAGNAME = (char *) calloc( sizeof(id3tag_v1->_TAGNAME) +1, 1);         \
  strncpy(_TAGNAME,  id3tag_v1->_TAGNAME,  sizeof(id3tag_v1->_TAGNAME));  \
  _TAGNAME[sizeof(id3tag_v1->_TAGNAME)] = '\0';                           \
  strip_tailspace(_TAGNAME);

/*
  get_display_string():
  interpret display_format, and return the pointer to result string
*/
char *
get_display_string(char *display_format, char *filename)
{
  id3v1tag  *id3tag_v1;
  char      *display_string;
  char      tmp_buf[DISPLAY_STRING_LEN];
  char      *filename_buf, *body, *ext, *path;
  char      *tmp_ptr;
  char      *title, *artist, *album, *year, *comment;
  char      *ptr;
  int       count;
  char 	    track[4]; 
  if ((id3tag_v1 = get_id3info_v1 (filename)) == NULL)
    return NULL;

  /* get a body,extension and path from a filename */
  path = NULL;
  ext  = NULL;
  filename_buf = (char *) strdup( filename );
  body = strip_path( filename_buf );
  if (body != filename_buf) {
    /* path was found */
    *(body - 1) = '\0';
    path = filename_buf;
  }
  /* search a position of last '.' and replace with '\0' */
  tmp_ptr = body + strlen( body ) - 1;
  while (tmp_ptr >= body) {
    if (*tmp_ptr == '.') {
      *tmp_ptr = '\0';
      ext  = tmp_ptr+1;
      break;
    }
    tmp_ptr--;
  }
/*
printf("body=%s\next=%s\npath=%s\nfilename=%s\n", body,ext, path,filename);
*/

  /* get each tag string from the ID3TAG cluster */
  GET_ID3TAG(artist);
  GET_ID3TAG(title);
  GET_ID3TAG(album);
  GET_ID3TAG(year);
  GET_ID3TAG(comment);

/* id3v1.1: if comment[28] is 0, comment[29] has track number */
   if(id3tag_v1->comment[28]=='\0')
	sprintf(track, "%d", id3tag_v1->comment[29]);
   else
	track[0]=0;
/*
printf("artist: [%s]\ntitle:  [%s]\nalbum:  [%s]\nyear:   [%s]\ncomment:[%s]\ngenre:[%s]\n",+ artist, title, album, year, comment, mpg123_id3_genres[id3tag_v1->genre]);
printf("%s\n", display_format);
*/

  /* start analyzing display_format */
  ptr   = display_format;
  count = 1;
  display_string = (char *) calloc (DISPLAY_STRING_LEN, 1);
  while ( (*ptr != '\0') && (count < DISPLAY_FORMAT_LEN) ) {
    if (*ptr == '%') {
      ptr++; count++;
      switch (*ptr) {
        case '0':				       /* %0: v1.1 Track # */
	 safe_strcat(display_string, track, DISPLAY_STRING_LEN);
	 break;

        case '1':                                      /* %1: ID3 Artist */
         safe_strcat(display_string, artist, DISPLAY_STRING_LEN);
         break;

        case '2':                                      /* %2: ID3 Title */
         safe_strcat(display_string, title, DISPLAY_STRING_LEN);
         break;

        case '3':                                      /* %3: ID3 Album */
         safe_strcat(display_string, album, DISPLAY_STRING_LEN);
         break;

        case '4':                                      /* %4: ID3 Year */
         safe_strcat(display_string, year, DISPLAY_STRING_LEN);
         break;

        case '5':                                      /* %5: ID3 Comment */
         safe_strcat(display_string, comment, DISPLAY_STRING_LEN);
         break;

        case '6':                                      /* %6: ID3 Genre */
         if (id3tag_v1->genre < GENRE_MAX) {
           safe_strcat(display_string, mpg123_id3_genres[id3tag_v1->genre],
                       DISPLAY_STRING_LEN);
         }
         break;

        case '7':                                      /* %7: File Name */
         safe_strcat(display_string, body, DISPLAY_STRING_LEN);
         break;

        case '8':                                      /* %8: File Path */
         safe_strcat(display_string, path, DISPLAY_STRING_LEN);
         break;

        case '9':                                      /* %9: File Extension */
         safe_strcat(display_string, ext, DISPLAY_STRING_LEN);
         break;

        case '%':                                      /* %%: Convert to '%' */
         safe_strcat(display_string, "%", DISPLAY_STRING_LEN);
         break;

        default:                                       /* No Action */
      }

    } else {
      /* Normal character */
      sprintf(tmp_buf, "%c", *ptr);
      safe_strcat(display_string, tmp_buf, DISPLAY_STRING_LEN);
    }

    ptr++; count++;
  }


/* printf("string = [%s]\n", display_string); */


  return display_string;
}

/*
  get_id3info_v1:
  get ID3 tag info(v1) from MP3 file, and return the pointer to the structure
*/

id3v1tag *
get_id3info_v1(char *filename)
{
  FILE     *fp;
  id3v1tag *id3tag;

  /* too safe? */
  if ((fp = fopen (filename, "r")) == NULL)
    return NULL;

  id3tag = (id3v1tag *) calloc (sizeof (id3v1tag), 1);
  if (id3tag == NULL)
    return NULL;

  if ((fseek (fp, -1 * sizeof (id3v1tag), SEEK_END) == 0) &&
      (fread (id3tag, 1, sizeof (id3v1tag), fp)
       == sizeof (id3v1tag)) &&
      (strncmp (id3tag->tag, "TAG", 3) == 0)) {
    fclose (fp);
    return id3tag;   /* got id3 tag infomation (raw, doesn't have \0) */
  }

  fclose (fp);
  return NULL;
}
#endif
  
int
#ifndef WITH_USBDEVFS
write_song (int rio_dev, char *filename, int card)
#else
write_song (struct usbdevice *rio_dev, char *filename, int card)
#endif
{
  int input_file;
  int  i, j, size;
  int  total, count, num_blocks, remainder, blocks_left;
  BYTE *block, *p;
  int num_chunks, song_location;

  block = (char *)malloc (0x80000);

  i = 0;
  input_file = open (filename, O_RDONLY);
  if (input_file == -1)
    return -1;

  size = file_size (filename);

  num_blocks = size / 0x10000;
  remainder  = size % 0x10000;

  printf ("Transfering file: %s  ", filename);
  fflush (stdout);

  /* First send num_chunks, blocks at a time */
  num_chunks = 0x10;
  total = 0;
  blocks_left = num_blocks - num_chunks;
 
  send_command (rio_dev, 0x4f, 0xffff, card);
  while (blocks_left > 0)
  {
    send_command (rio_dev, 0x46, num_chunks, 0);
    for (j=0;j<num_chunks / 2;j++)
    {
      count = read (input_file, block, 0x20000);
      total += count;
      if (count != 0x20000)
        printf ("[Short read!]");
      bulk_write (rio_dev, block, 0x20000);
      printf (".");
      fflush (stdout);
    }
    blocks_left -= num_chunks; 
    // Send a 0x42.
    send_command (rio_dev, 0x42, 0, 0);
    send_command (rio_dev, 0x42, 0, 0);
  }

  /* Send remaining blocks */
  blocks_left += num_chunks;
  //send_command (rio_dev, 0x4f, 0xffff, card);
  send_command (rio_dev, 0x46, blocks_left, 0);
  while (blocks_left > 0)
  {
    count = read (input_file, block, 0x10000);
    total += count;
    if (count != 0x10000)
      printf ("[Short read!]");
    bulk_write (rio_dev, block, 0x10000);
    printf (".");
    fflush (stdout);
    blocks_left--;
    send_command (rio_dev, 0x42, 0, 0);
    send_command (rio_dev, 0x42, 0, 0);
  }

  /* Send last block */
  count = read (input_file, block, remainder);
  j = remainder;
  total += count;
  p = block;
  while (j > 0)
  {
    if (j > 0x4000)
    {
      //send_command (rio_dev, 0x4f, 0xffff, card);
      send_command (rio_dev, 0x46, 00, 0x4000);
      bulk_write (rio_dev, p, 0x4000);
    } else {
      //send_command (rio_dev, 0x4f, 0xffff, card);
      send_command (rio_dev, 0x46, 00, j);
      bulk_write (rio_dev, p, j);
    }
    j -= 0x4000;
    p += 0x4000;
    send_command (rio_dev, 0x42, 0, 0);
    send_command (rio_dev, 0x42, 0, 0);
  }

  send_command (rio_dev, 0x42, 0, 0);
  send_command (rio_dev, 0x42, 0, 0);
  song_location = send_command (rio_dev, 0x43, 0, 0);
  printf (" (done. Transfered %d bytes.)\n", total);
  fflush (stdout);

#ifdef DEBUG
  fprintf (stderr, "Wrote song to offset 0x%04x\n", song_location);
#endif

  return song_location;
}

char *
strip_path (char *f)
{
  int i = strlen (f);

  while (i>0)
  {
    if (f[i-1] == '/')
      return &f[i];
    i--;
  }
  
  return f;
}
#ifdef USE_ID3_TAGS
 char *
 strip_tailspace (char *f)
 {
   int i = strlen (f);

   while ((i>0) && (f[i-1] == ' '))
     i--;

   f[i] = '\0';

   return f;
 }
#endif

int 
file_size (char *filename)
{
  struct stat file_stat;
  int  size;

  if (stat(filename,&file_stat) == -1)
    return 0;

  size = file_stat.st_size;

  return size;
}

#ifdef USE_ID3_TAGS
static char const shortopts[] = "d:xaF:f:n:hv";
#else
static char const shortopts[] = "xaF:f:n:hv"; 
#endif
static struct option const longopts[] =
{
#ifdef USE_ID3_TAGS
  {"display", required_argument, NULL, 'd'},
#endif
  {"external", no_argument, NULL, 'x'},
  {"autofill", no_argument, NULL, 'a'},
  {"folder", required_argument, NULL, 'F'},
  {"fontname", required_argument, NULL, 'f'},
  {"fontnumber", required_argument, NULL, 'n'},
  {"version", no_argument, NULL, 'v'},
  {"help", no_argument, NULL, 'h'},
  {NULL, no_argument, NULL, 0}
};

static char const *const option_help[] =
{
"Input options:",
"",
#ifdef USE_ID3_TAGS
"  -d %x.%y  --display %x.%y    set Rio500 display's FORMAT like xmms's title.",
"                               acceptable special characters are below:",
"",
"                               %1=ID3 Artist  %2=ID3 Title    %3=ID3 Album",
"                               %4=ID3 Year    %5=ID3 Comment  %6=ID3 Genre",
"                               %7=File Name   %8=File Path    %9=File Extension",
"                               %0=Track Number (id3v1.1)",
"",
"                               default FORMAT is %7.%9",
"",
#endif
"  -x        --external         Write to external memory card",
"  -a        --autofill         Use external memory card if internal full",
"  -F x      --folder x         Transfer song(s) into folder of index=x",
"  -f name   --fontname name    Set the fontname to be used on the Rio display.",
"  -n x      --fontnumber x     Set the fontnumber within the given ",
"                               .fon file set with -f",
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

#ifdef USE_ID3_TAGS
void
get_some_switches (int argc, char *argv[], int *font_number, int *folder_num, char *display_format, int *card_number, int *card_auto)
#else

void
get_some_switches (int argc, char *argv[], int *font_number, int *folder_num, int *card_number, int *card_auto)
#endif
{
    register int optc;
    char const * const *p; 
    FILE *fptemp=0;
    int newsize = 0;

    if (optind == argc)
        return;
    while ((optc = getopt_long (argc, argv, shortopts, longopts, (int *) 0))
           != -1) {
	 switch (optc) {
#ifdef USE_ID3_TAGS
            case 'd':
		/* Tomoaki - process display switch here */                
                if (strlen(optarg) >= DISPLAY_FORMAT_LEN) {
                   fprintf(stderr,"\nID3 format length is too long!\n");
                   usage(argv[0]);
                   exit(-1);
                   break;
                }
                strcpy (display_format, optarg);
		break;
#endif

	    case 'x':
		*card_number = 1;
		break;
	  
	    case 'a':
		*card_number = 0;
		*card_auto = 1;
		break;

	    case 'F':
		/* Sanity check --foldernumber digit */
		if(!isdigit(*optarg)) {
		   fprintf(stderr,"\nFolder number must be numeric!\n");
		   usage(argv[0]);
		   exit(-1);
		   break;
		}
		*folder_num=atoi(optarg);
		break;
		
            case 'f':
		newsize = strlen(font_name)+strlen(optarg)+1;
		temp_name=(char *)malloc(newsize);
		strcpy(temp_name,font_name);
		strcat(temp_name,optarg);
		font_name=temp_name;
                if ( ( fptemp = fopen(font_name, "rb" ) ) == 0 )
   		{
                 fprintf(stderr,"\n%s is an invalid fontpath/fontname\n",font_name);
      	       	 exit(-1);
   		 break;
                }
		else
                {
		 fclose(fptemp);
		 break;
		}

            case 'n':
                /* Sanity check --fontnumber digit */
		if(!isdigit(*optarg)) {
		   fprintf(stderr,"\nFont number must be numeric!\n");
		   usage(argv[0]);
		   exit(-1);
		   break;
		}
		*font_number = atoi(optarg);
		break;
            case 'v':
                printf("\nrio_add_song -- version %s\n",VERSION);
		exit(0);
		break;
            case 'h':
                usage(argv[0]);
		for (p=option_help;  *p ;  p++)
    	          fprintf (stderr, "%s\n", *p);
		exit(0);
		break;
            default:
                usage (argv[0]);
        }
    }

    /* Processing any filename args happens in main code.  */
 
}



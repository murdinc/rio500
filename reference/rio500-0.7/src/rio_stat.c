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
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#include "librio500.h"
#include "getopt.h"
void get_some_switches (int argc, char *argv[], int * ter);
void usage (char *progname);

void
usage (char *progname)
{
  printf ("\nusage: %s [OPTIONS] \n", progname);
  printf ("\n");
  printf ("\n Shows Rio statistics, such as:\n");
  printf ("\n   Memory free");
  printf ("\n   Folder Names");
  printf ("\n   Stored songs");
  printf ("\n   Song sizes\n");
  printf("\n");
return;
}

#define FOLDER_BLOCK_SIZE           0x4000

int terse = 0;
#ifndef WITH_USBDEVFS
void show_songs (int rio_dev, GList *folders, int num_folder, int card);
#else
void show_songs (struct usbdevice *rio_dev, GList *folders, int num_folder, int card);
#endif

int
main (int argc, char *argv[])
{
  int         folder_num=0;
  unsigned long memfree, memtotal,revision;
  GList      *folders, *item;
  mem_status *mem;
  folder_entry *entry;
  int card,card_count;
#ifndef WITH_USBDEVFS
  int rio_dev;
#else
  struct usbdevice *rio_dev;

  rio_dev = malloc(sizeof(struct usbdevice));
#endif

  get_some_switches(argc,argv,&terse);

  /* Open connection to rio */
#ifndef WITH_USBDEVFS
  rio_dev = open (DEFAULT_DEV_PATH, O_RDWR);
  if (rio_dev < 0)
    {
      perror("Could not open " DEFAULT_DEV_PATH);
      fprintf(stderr,"\nVerify that the rio module is loadad and "
               "your Rio is\nconnected and powered up.\n\n");
      exit (-1);
    }

   /* Init communication with rio */
   init_communication (rio_dev);
#else
   if(!(rio_dev = init_communication())) {
     printf("init_communication() failed!\n");
     free (rio_dev);
     return -1;
   }
#endif
   revision = query_firmware_rev(rio_dev);
   printf ("Your Rio500 has firmware revision %d.%02x\n", revision >> 8,(revision & 0xff));

   card_count = (int) query_card_count(rio_dev);

   finish_communication (rio_dev);

#ifndef WITH_USBDEVFS
   close (rio_dev);
#endif
   
   for (card=0; card<card_count; card++) {

#ifndef WITH_USBDEVFS
  rio_dev = open (DEFAULT_DEV_PATH, O_RDWR);
  if (rio_dev < 0)
    {
      perror("Could not open " DEFAULT_DEV_PATH);
      fprintf(stderr,"\nVerify that the rio module is loadad and "
               "your Rio is\nconnected and powered up.\n\n");
      exit (-1);
    }

   /* Init communication with rio */
   init_communication (rio_dev);
#else
   if(!(rio_dev = init_communication())) {
     printf("init_communication() failed!\n");
     free (rio_dev);
     return -1;
   }
#endif

   /* Check how much memory we have */
   memfree = query_mem_left(rio_dev,card);
   mem = get_mem_status(rio_dev,card);
   memtotal = mem->num_blocks * mem->block_size;
   
   if (terse)
     printf ("\nMemory: Free Bytes\n%ld\n", memfree);
   else
     {
       printf ("Card %d reports %ld Mb free (%ld bytes) out of %ld Mb (%ld bytes).\n", 
	       card,(memfree/(1024*1024)), memfree,(memtotal/(1024*1024)), memtotal );
     }
   if (!terse)
     {
       printf ( "Command 0x57 returned:\n");
       printf ( " first_free_block = 0x%08x\n sl = 0x%08x\n", 
		mem->first_free_block, mem->num_unused_blocks);

       /* Read folder list */
       printf ( "-------------------------------------------------------------\n");
       printf ( "  N   offset  num songs       Folder Name\n");
       printf ( "-------------------------------------------------------------\n");
     }
   folder_num = 0;
   folders = read_folder_entries (rio_dev, card);
   for (item = folders; item; item = item->next)
   {
     entry = (folder_entry *)item->data;
     if (entry)
     {
       if (terse)
	 printf ( "\nFolder: # Songs Name\n%02d %02d %s\n", 
		  folder_num,
		  (entry->fst_free_entry_off / 0x800),
		  entry->name1 );
       else
	 printf ( "(%2d)  0x%04x  (%2d items)      %s\n", 
		  folder_num,
		  entry->offset, 
		  (entry->fst_free_entry_off / 0x800),
		  entry->name1 );

       show_songs (rio_dev, folders, folder_num++, card);
     }
   }

   finish_communication (rio_dev);

#ifndef WITH_USBDEVFS
   close (rio_dev);
#endif
   }
   exit (0);
}


void
#ifndef WITH_USBDEVFS
show_songs (int rio_dev, GList *folders, int num_folder, int card)
#else
show_songs (struct usbdevice *rio_dev, GList *folders, int num_folder, int card)
#endif
{   
  int         song_num;
  GList      *songs, *item;
  song_entry *entry;


  if (terse)
    printf ("\nSongs: # Bytes Name\n");
  else
    {
      printf ("\n");
      printf ( "   (num) offset      size         song name\n");
    }

   songs = read_song_entries (rio_dev, folders, num_folder, card);
   song_num = 0;
   for (item = songs; item; item = item->next)
   {
     entry = (song_entry *)item->data;
     if (entry)
     {
       if (terse)
	 printf ( "%02d %8ld %s\n", 
		  song_num++, 
		  entry->length,
		  entry->name1 );
       else
	 printf ( "    (%2d) 0x%04x  (%8ld bytes) %s\n", 
		  song_num++, 
		  entry->offset, 
		  entry->length,
		  entry->name1 );
     }
   }
   printf ("\n\n");

}

static char const shortopts[] = "thv";

static struct option const longopts[] =
{
  {"terse", no_argument, NULL, 't'},
  {"version", no_argument, NULL, 'v'},
  {"help", no_argument, NULL, 'h'},
  {NULL, no_argument, NULL, 0}
};

static char const *const option_help[] =
{
"Input options:",
"",
"  -t        --terse        Terse rio_stat output",
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
get_some_switches (int argc, char *argv[], int * ters)
{
    register int optc;
    char const * const *p;

    if (optind == argc)
        return;
    while ((optc = getopt_long (argc, argv, shortopts, longopts, (int *) 0))
           != -1) {
         switch (optc) {
            case 't':
                *ters = 1;
                break;
            case 'v':
                printf("\nrio_stat -- version %s\n",VERSION);
                exit(0);
                break;
            case 'h':
                usage(argv[0]);
                for (p=option_help;  *p ;  p++)
                  fprintf (stderr, "%s\n", *p);
                exit(0);
                break;
            default:
                usage(argv[0]);
                for (p=option_help;  *p ;  p++)
                  fprintf (stderr, "%s\n", *p);
                exit(0);
                break;

         }
    }


}


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
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#include "librio500.h"


#define FOLDER_BLOCK_SIZE           0x4000



#ifndef TRUE

#define TRUE 				1
#define FALSE 				0

#endif /* TRUE */

void usage (char *progname);
void signal_handler (int signal);
#ifndef WITH_USBDEVFS
void read_file (int rio_dev, unsigned long size, char *filename, int card);
#else
void read_file (struct usbdevice *rio_dev, unsigned long size, char *filename, int card);
#endif


void
usage (char *progname)
{
  printf ("\nusage: %s <song_num> <folder_num> <card_num>\n", progname);
  printf ("\n <file> is the name of the file whose content");
  printf ("\n we want to upload.");
  printf ("\n <folder_num> is the index to the folder where it will be put.\n");
  printf (" <card_num> is the index of the memory card to use\n");
  printf (" card_num=0=internal memory, card_num=1=external memory card\n");
  return;
}

main(int argc, char *argv[])
{
  int               old_offset, i, length;
  int               folder_block_offset, song_block_offset;
  int               folder_num, song_num, font_number;
  GList            *folders, *songs;
  folder_entry     *folder;
  song_entry       *song;
  folder_entry     *f_entry;
  int		   card;

#ifndef WITH_USBDEVFS
  int               rio_dev;
#else
  struct usbdevice *rio_dev;

  rio_dev = malloc(sizeof(struct usbdevice));
#endif

  /* Setup signal handler */
  signal (SIGINT , signal_handler);
  signal (SIGQUIT, signal_handler);
  signal (SIGHUP , signal_handler);
  signal (SIGSEGV, signal_handler);
  signal (SIGTERM, signal_handler);

  if (argc < 2 || argc > 4)
  {
    usage (argv[0]);
    exit (-1);
  }

  /* set defaults */
  folder_num = 0;
  card = 0;
  song_num = atoi (argv[1]);

  if (argc == 3)
    folder_num = atoi (argv[2]);

  if (argc == 4)
  {
    folder_num = atoi (argv[2]);
    card = atoi (argv[3]);
  }
  /* Open connection to rio */
#ifndef WITH_USBDEVFS
  rio_dev = open (DEFAULT_DEV_PATH, O_RDWR);
  if (rio_dev < 0)
    {
      perror ("\nVerify that your Rio is connected and powered up.\n\n");
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

   /* Get the first free block and amont of free memory in Rio */
   /* This doesn't seem to be necesary */
   /* send_command (rio_dev, 0x50, 0, 0); */
   /* send_command (rio_dev, 0x51, 1, 0); */
   /* mem = get_mem_status (rio_dev); */

   /* Read folder & song block */
   folders = read_folder_entries (rio_dev,card);
   if ( folder_num > g_list_length (folders)-1 )
     folder_num = 0;
   songs   = read_song_entries ( rio_dev, folders, folder_num,card ); 

   if (song_num > g_list_length (songs)-1)
   {
     printf ("Incorrect song_num parameter!\n");
     goto end;
   }

   /* Trick the rio into reading song data */
   f_entry = (folder_entry *) g_list_nth_data (folders, 0);
   song    = (song_entry *) g_list_nth_data (songs, song_num);
 
   if (f_entry == NULL || song == NULL)
   {
     printf ("Incorrect song_num or folder_num parameter!\n");
     goto end;
   }


   old_offset      = f_entry->offset;
   f_entry->offset = song->offset;

   write_folder_entries ( rio_dev, folders,card );
   send_command (rio_dev, 0x42, 0, 0);
   send_command (rio_dev, 0x42, 0, 0);
   folder_block_offset = send_command (rio_dev, 0x43, 0x0, 0x0);

   /* Tell Rio where the root folder block is. */
   send_folder_location (rio_dev, folder_block_offset, folder_num,card);

   /* Not really sure what this does */
   send_command (rio_dev, 0x58, 0x0, card);

   /* Now read the song */
   read_file (rio_dev, song->length, song->name1,card);

   /* Restore folder */
   f_entry->offset = old_offset;
   write_folder_entries ( rio_dev, folders,card );
   send_command (rio_dev, 0x42, 0, 0);
   send_command (rio_dev, 0x42, 0, 0);
   folder_block_offset = send_command (rio_dev, 0x43, 0x0, 0x0);

   /* Tell Rio where the root folder block is. */
   send_folder_location (rio_dev, folder_block_offset, folder_num,card);

   /* Not really sure what this does */
   send_command (rio_dev, 0x58, 0x0, card);

end:

   /* Close device */
   finish_communication (rio_dev);
#ifndef WITH_USBDEVFS
   close (rio_dev);
#endif

   exit (0);
}

void
#ifndef WITH_USBDEVFS
read_file (int rio_dev, unsigned long size, char *filename, int card)
#else
read_file (struct usbdevice *rio_dev, unsigned long size, char *filename, int card)
#endif
{
  int output_file;
  struct stat file_stat;
  int  i, j, this_read, block_length;
  int  total, count, num_blocks, remainder, blocks_left;
  BYTE *block, *p, address[2];
  unsigned short offset;
  int num_chunks, song_location;

  block = (char *)malloc (0x80000);

  i = 0;
  output_file = open (filename, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
  if (output_file == -1)
    return;

  printf ("Reading file: %s  ", filename);
  fflush (stdout);

  /* Read 0x4000 bytes first */
  this_read = (size > 0x4000) ? 0x4000 : size;
  send_command (rio_dev, 0x4e, 0xff, card);
  send_command (rio_dev, 0x45, 0x0, this_read);

  count = bulk_read (rio_dev, block, this_read);
  if (count > 0)
    write (output_file, block, count);

  size -= this_read;

  if (size == 0)
    return;

  num_blocks = size / 0x10000;
  remainder  = size % 0x10000;

  /* First read num_chunks, blocks at a time */
  num_chunks = 0x10;
  total = this_read;
  blocks_left = num_blocks - num_chunks;
 
  while (blocks_left > 0)
  {
    send_command (rio_dev, 0x45, num_chunks, 0x0);
    for (j = 0; j < num_chunks / 2; j++)
    {
      count = bulk_read (rio_dev, block, 0x20000);
      total += count;
      if (count != 0x20000)
        printf ("[Short read!]");
      write (output_file, block, count);
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
  send_command (rio_dev, 0x45, blocks_left, 0x0);
  while (blocks_left > 0)
  {
    count = bulk_read (rio_dev, block, 0x10000);
    total += count;
    if (count != 0x10000)
      printf ("[Short read!]");
    write (output_file, block, count);
    printf (".");
    fflush (stdout);
    blocks_left--;
    send_command (rio_dev, 0x42, 0, 0);
    send_command (rio_dev, 0x42, 0, 0);
  }

  /* Read last block */
  while (remainder > 0)
  {
    this_read = (remainder > 0x4000) ? 0x4000 : remainder;
    send_command (rio_dev, 0x45, 0x0, this_read);
    count = bulk_read (rio_dev, block, this_read);
    if (count > 0)
      write (output_file, block, count);
    total += count;
    remainder -= this_read;
  }

  printf (" (done. Transfered %d bytes.)\n", total);
  fflush (stdout);

  return;
}

void signal_handler (int signal)
{
  switch (signal)
  {
    case 1:
    case 2:
      printf ("Cannot interrupt transfer! Please wait for transfer to complete ...\n");
      break;
    default:
      printf ("Signal [%d] trapped! Ignoring ... \n", signal);
  }
}

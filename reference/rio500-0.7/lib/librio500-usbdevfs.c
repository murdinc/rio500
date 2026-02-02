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


#define _GNU_SOURCE
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/poll.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>

#if TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif

#include "librio500.h"
#include "usbdrv.h"
#include "libpsf.h"
#include "libfon.h"

#ifdef WORDS_BIGENDIAN
#include <byteswap.h>
#endif

static unsigned verboselevel = 0;

struct usbdevice *init_communication ()
{
  int ret;
  int intf = 0;
  struct usbdevice *rio_dev;

  rio_dev = usb_open(USB_VENDOR_DIAMOND, USB_PRODUCT_DIAMOND_RIO500USB, 5000);

  if (!rio_dev) {
    printf("usb_init returned failure\n");
    return NULL;
  }

  if(usb_claiminterface(rio_dev, intf)) {
	printf("usb_claiminterface returned an error!\n");
  }

  send_command (rio_dev, START_USB_COMM, 0x00, 0x00);
  return rio_dev;
}

void
finish_communication (struct usbdevice *rio_dev)
{
  int intf = 0;

  send_command (rio_dev, 0x42, 0x00, 0x00);
  send_command (rio_dev, END_USB_COMM, 0x00, 0x00);
  send_command (rio_dev, 0x42, 0x00, 0x00);

  if(usb_releaseinterface(rio_dev, intf)) {
	printf("usb_releaseinterface returned an error!\n");
  }
  
  usb_close(rio_dev);
}

void
send_folder_location (struct usbdevice *rio_dev, int offset, int folder_num, int card)
{
  folder_location location;

  memset (&location, 0, sizeof (folder_location));
  location.offset = (WORD) offset;
  location.bytes = (WORD) 0x4000;
  location.folder_num = (WORD) folder_num;


/* this struct written from cpu to rio, for big_endian platforms, 
   need to byteswap to get to le */

#ifdef WORDS_BIGENDIAN
  location.offset     = bswap_16(location.offset);
  location.bytes      = bswap_16(location.bytes);
  location.folder_num = bswap_16(location.folder_num);
#endif

  rio_ctl_msg (rio_dev, RIO_DIR_OUT, 0x56, 0, 0, 
               sizeof(folder_location), (void*)&location);
}

void
format_flash (struct usbdevice *rio_dev, int card)
{
  send_command (rio_dev, RIO_FORMAT_DEVICE, 0x2185, card);
  sleep (1); /* wait for flash memory to update */
}

mem_status *
get_mem_status (struct usbdevice *rio_dev, int card)
{
  static mem_status status;

  memset (&status, 0, sizeof (mem_status));

/* set card from which to get memory status */
  send_command (rio_dev, 0x51, 1, card);

  rio_ctl_msg (rio_dev, RIO_DIR_IN, 0x57, 0, 0, sizeof(status), (void*)&status);

/* this struct "filled" by the rio.  Need to switch to big_endian for ppc 
   to read correctly */

#ifdef WORDS_BIGENDIAN
  status.dunno1 = bswap_16(status.dunno1);
  status.block_size = bswap_16(status.block_size);
  status.num_blocks = bswap_16(status.num_blocks);
  status.first_free_block = bswap_16(status.first_free_block);
  status.num_unused_blocks = bswap_16(status.num_unused_blocks);
  status.dunno2 = bswap_32(status.dunno2);
  status.dunno3 = bswap_32(status.dunno3);
#endif

  return &status;
}

unsigned long
query_card_count (struct usbdevice *rio_dev)
{
  return ( ( (send_command (rio_dev, 0x42, 0, 0) & 0x40000000) >> 30) + 1);
}


unsigned long
query_mem_left (struct usbdevice *rio_dev, int card)
{
  unsigned long mem_left;
  send_command (rio_dev, 0x42, 0, 0);
  mem_left = send_command (rio_dev, 0x50, 0, card);
  /* wait a bit */
  send_command (rio_dev, 0x42, 0, 0);

  return mem_left;
}

unsigned long
query_firmware_rev (struct usbdevice *rio_dev)
{
  return send_command (rio_dev, 0x40, 0, 0) & 0xffff;
}

unsigned long
get_num_folder_blocks (struct usbdevice *rio_dev, int address, int card)
{
  unsigned long read_status=0;

  read_status = send_command (rio_dev, 0x59, address, card);
  /* if command fails end comm gracefully and return -1 */
  if (read_status == 0)
  {
       //finish_communication(rio_dev);
       send_command (rio_dev, 0x42, 0x00, 0x00);
       send_command (rio_dev, END_USB_COMM, 0x00, 0x00);
       send_command (rio_dev, 0x42, 0x00, 0x00);

       return -1; 
  }

  return read_status;
}

GList *
read_folder_entries (struct usbdevice *rio_dev, int card)
{
  BYTE           *folder_block, *pb;
  GList          *entry_list = NULL;
  folder_entry   *entry, *copy;
  int            total_read, folder_count=0;
  unsigned long  com_status;
  unsigned long  folder_block_count;
  unsigned long  total_folder_block_size;

  /* Determine number of folder blocks */
  folder_block_count = get_num_folder_blocks (rio_dev, 0xff00, card);
  if (folder_block_count == -1)
    return NULL;

  total_folder_block_size = FOLDER_BLOCK_SIZE * folder_block_count;

   /* Assign space for folder block */
  folder_block = (BYTE *) malloc (total_folder_block_size);
  if (folder_block == NULL)
     return NULL;
 
   /* Read folder list */
  com_status = send_read_command (rio_dev, 0xff00, folder_block_count, card);
  if (com_status == -1) {
	free (folder_block);
        return NULL;
  }
 
  total_read = bulk_read ( rio_dev, folder_block, total_folder_block_size);
  if (total_read != total_folder_block_size) {
     free (folder_block);
     return NULL;
  }
 
  /* Make a list of entries: one for each folder. */
   pb = folder_block;
   entry = (folder_entry *)pb;
  while (entry->offset != 0xffff && folder_count<8*folder_block_count)
  {
    copy = calloc (sizeof (folder_entry), 1);
    memcpy (copy, entry, sizeof (folder_entry));
    #ifdef WORDS_BIGENDIAN
      bswap_folder_entry (copy);
    #endif
    entry_list = g_list_append (entry_list, copy);
    folder_count++;
    pb += sizeof (folder_entry);
    entry = (folder_entry *)pb;
  }

  free (folder_block);
  return g_list_first (entry_list);
}


GList *
read_song_entries (struct usbdevice *rio_dev, GList *folder_entries, int folder_num, int card)
{
  folder_entry *folder;
  song_entry *song, *song_copy;
  BYTE *song_block, *ps;
  GList *item, *song_list = NULL;
  unsigned long com_status;
  int    total_read, address, num_blocks;
  int    size, count;

  item = g_list_nth (folder_entries, folder_num);
  if (item == NULL)
    return NULL;
  folder = (folder_entry*) item->data;
  if (folder == NULL)
    return NULL;

  /* Calculate how many blocks this folder uses up */
  num_blocks = folder->fst_free_entry_off / 0x4000;
  count = 8 * num_blocks;
  if ( (folder->fst_free_entry_off % 0x4000) > 0 )
    num_blocks++;
  count += (folder->fst_free_entry_off % 0x4000) / 0x800;

  if (num_blocks == 0)
    return NULL;

  /* Read numblocks */
  address = folder_num;
  address <<= 8;
  address |= 0x00ff;
  address &= 0xffff;

  size = num_blocks * FOLDER_BLOCK_SIZE;
  song_block = (BYTE *) malloc ( size+1 );
  if (song_block == NULL)
    return NULL;

  /* Read folder list */
  com_status = send_read_command (rio_dev, address, num_blocks, card);
  if (com_status == -1) {
        free(song_block);
        return NULL;
   }

total_read = rio_usb_bulk (rio_dev, 0x81, song_block, size);
  if ( total_read != size ) {
    free (song_block);
    return (NULL);
  }

  /* Make list */
  ps = song_block;
  song = (song_entry *)ps;
  while (count > 0 && song->offset != 0xffff)
  {
    song_copy = calloc (1, sizeof (song_entry));
    memcpy (song_copy, song, sizeof (song_entry) );
    #ifdef WORDS_BIGENDIAN
      bswap_song_entry (song_copy);
    #endif
    song_list = g_list_append (song_list, song_copy);
    ps += sizeof (song_entry);
    song = (song_entry *)ps;
    count--;
  }
  
  free (song_block);

  if (song_list)
    return g_list_first(song_list);
  else 
    return NULL;
}

void
write_folder_entries (struct usbdevice *rio_dev, GList *folder_list, int card)
{
  int          num_blocks, list_len;
  int          count;
  BYTE         *block, *p;
  GList        *item;
  folder_entry *entry;

  block = new_empty_block ();

  /* If there are no entries just send a blank block */
  if (folder_list == NULL)
  {
    send_write_command (rio_dev, 0xff00, 1, card);
    rio_usb_bulk (rio_dev, 2, block, 0x4000);
    free (block);
    return;
  }

  list_len = g_list_length (folder_list);

  num_blocks = list_len >> 3; /* len / 8. There are 8 entries per block */
  if (list_len & 0x7)
    num_blocks++;

  send_write_command (rio_dev, 0xff00, num_blocks, card);
  p = block;
  count = 0;
  for (item = g_list_first (folder_list); item; item = item->next)
  {
    /* copy up to 8 entries */
    entry = (folder_entry*) p;
    memcpy (p, item->data, sizeof (folder_entry));
    #ifdef WORDS_BIGENDIAN
      bswap_folder_entry ((folder_entry*)p);
    #endif 
    p += sizeof (folder_entry);
    count++;
    if (count == 8)
    {
      /* Write the block */
      rio_usb_bulk (rio_dev, 2, block, 0x4000);
      count = 0;
      clear_block (block);
      p = block;
    }
  }

  /* Write the last block if it was not full */
  if (count != 0) 
    rio_usb_bulk (rio_dev, 2, block, 0x4000);

  free (block);
  return;
}

void
write_song_entries (struct usbdevice *rio_dev, int folder_num, GList *song_list, int card)
{
  int          num_blocks, list_len;
  int          count, address;
  BYTE         *block, *p;
  GList        *item;
  song_entry *entry;

  /* Define address */
  address = folder_num;
  address <<= 8;
  address |= 0x00ff;
  address &= 0xffff;

  block = new_empty_block ();

  /* If there are no entries just send a black block */
  if (song_list == NULL)
  {
    send_write_command (rio_dev, address, 1, card);
    rio_usb_bulk (rio_dev, 2, block, 0x4000);
    free(block);
    return;
  }

  list_len = g_list_length (song_list);

  num_blocks = list_len >> 3; /* len / 8. There are 8 entries per block */
  if (list_len & 0x7)
    num_blocks++;

  /* Set to what folder this items go */
  send_write_command (rio_dev, address, num_blocks, card);

  /* Now write the items to the bulk pipe in 0x4000 byte chunks*/
  p = block;
  count = 0;
  for (item = g_list_first (song_list); item; item = item->next)
  {
    /* copy up to 8 entries */
    entry = (song_entry*) p;
    memcpy (p, item->data, sizeof (song_entry));
    #ifdef WORDS_BIGENDIAN
      bswap_song_entry ((song_entry*)p);
    #endif
    p += sizeof (song_entry);
    count++;
    if (count == 8)
    {
      /* Write the block */
      rio_usb_bulk (rio_dev, 2, block, 0x4000);
      count = 0;
      clear_block (block);
      p = block;
    }
  }

  /* Write the last block if it was not full */
  if (count != 0)
    rio_usb_bulk (rio_dev, 2, block, 0x4000);

  free (block);
  return; 
}


/* Folder and song operations */

song_entry *
song_entry_new (char *song_name, char *font_name, int font_number)
{
 char smiley[] = {0x00, 0x00, 0x00, 0x00,
                   0x3e, 0x41, 0x94, 0x80,
                   0xa2, 0x9d, 0x41, 0x3e,
                   0x00, 0x00, 0x00, 0x00,
                   0x00, 0x00, 0x00, 0x00,
                   0x00, 0x00, 0x80, 0x80,
                   0x80, 0x80, 0x00, 0x00,
                   0x00, 0x00, 0x00, 0x00};

  rio_bitmap_data     *bitmap;
  song_entry          *entry;

 
  /* Fill file info */
  entry = (song_entry *) calloc (sizeof (song_entry), 1);
  entry->offset = (WORD)  0;
  entry->length = (DWORD) 0;
  entry->dunno3 = (WORD) 0x0020;
  entry->mp3sig = (DWORD) 0x0092fbff;
  entry->time   = (DWORD) time (NULL);
  bitmap = bitmap_data_new (song_name, font_name, font_number);
  if (bitmap)
  {
    memcpy (&entry->bitmap, bitmap, sizeof (rio_bitmap_data));
  } else {
    entry->bitmap.num_blocks = 2;
    memcpy (entry->bitmap.bitmap, smiley, 0x20);
  }
  sprintf (entry->name1, "%s", song_name);
  sprintf (entry->name2, "%s", song_name);

  return entry;
}


folder_entry *
folder_entry_new (char *name, char *font_name, int font_number)
{
  folder_entry *fe;
  rio_bitmap_data *bits;

  fe = calloc (sizeof (folder_entry), 1);
  if (fe)
  {
    fe->dunno3 = 0x002100ff;
    fe->time   = time (NULL);
  
    bits = bitmap_data_new (name, font_name, font_number);
    if (bits)
    {
      memcpy (&fe->bitmap, bits, sizeof (rio_bitmap_data));
      free (bits);
    }

    sprintf (fe->name1, name);
    sprintf (fe->name2, name);
  }

/* data on ppc stored in big_endian, switch to little endian in preparation to send to rio */
/* keith -- 1/27 -- start in be format for use on ppc, byteswap only before writiing out */

/* #ifdef WORDS_BIGENDIAN
  bswap_folder_entry (fe);
#endif */

  return fe;
}

#ifdef WORDS_BIGENDIAN
void
bswap_folder_entry (folder_entry *fe)
{
  fe->offset = bswap_16(fe->offset);
  fe->dunno1 = bswap_16(fe->dunno1);
  fe->fst_free_entry_off = bswap_16(fe->fst_free_entry_off);
  fe->dunno2 = bswap_16(fe->dunno2);
  fe->dunno3 = bswap_32(fe->dunno3);
  fe->dunno4 = bswap_32(fe->dunno4);
  fe->time = bswap_32(fe->time);
}

void
bswap_song_entry (song_entry *se)
{
  se->offset = bswap_16(se->offset);
  se->dunno1 = bswap_16(se->dunno1);
  se->length = bswap_32(se->length);
  se->dunno2 = bswap_16(se->dunno2);
  se->dunno3 = bswap_16(se->dunno3);
  se->mp3sig = bswap_32(se->mp3sig);
  se->time = bswap_32(se->time);
}


#endif

BYTE *
new_empty_block ()
{
  BYTE *block = calloc (0x4000, 1);
  clear_block (block);
  return block;
}

void
clear_block (BYTE *block)
{
  BYTE *p;

  p = block;
  if (block)
  {
    while ( (p - block) < 0x4000)
    {
      *p     = 0xff;
      *(p+1) = 0xff;
      p += 0x800;
    }
  }

  return;
}


/*  -------------------------------------------------

                     Bulk transfers 

   -------------------------------------------------- */
int bulk_read(struct usbdevice *rio_dev, void *block, int size)
{
  return rio_usb_bulk(rio_dev, 0x81, block, size);
}

int bulk_write(struct usbdevice *rio_dev, void *block, int size)
{
  return rio_usb_bulk(rio_dev, 0x02, block, size);
}

int rio_usb_bulk(struct usbdevice *rio_dev, int ep, void *block, int size)
{
  int len;
  int transmitted=0;
  void *data;
  int ret;

  do {
    len = size - transmitted;
    data = (unsigned char *)block + transmitted;

/* The 5000 is a timeout value */
    ret = usb_bulk_msg(rio_dev, ep, len, data, 5000);
    if (ret < 0) {
      printf("rio_usb_bulk: usb_bulk returned error %x\n", ret);
      return -ret;
    } else {
      transmitted += ret;
    }
  } while (ret > 0 && transmitted < size);

  return transmitted;
}

/*  -------------------------------------------------

                   Lower level commands 

   -------------------------------------------------- */


unsigned long
send_write_command (struct usbdevice *rio_dev, int address, int num_blocks, int card)
{   
  int length = num_blocks * 0x4000;
  int num_big_reads, num_small_reads;
  unsigned long write_status=0;

  num_big_reads   = length / 0x10000;
  num_small_reads = length % 0x10000;

  /* rio returns 0 on command failure for 0x4f and 0x46. . we return -1 */

  write_status = send_command (rio_dev, 0x4c, address, card);

  write_status = send_command (rio_dev, 0x4f, 0xffff, card);
  /* if command fails end comm gracefully and return -1 */
  if (write_status == 0)
  {
	finish_communication(rio_dev);
	return -1; 
  }

  write_status = send_command (rio_dev, 0x46, num_big_reads, num_small_reads);
  /* if command fails end comm gracefully and return -1 */
  if (write_status == 0)
  {
  	finish_communication(rio_dev);
	return -1; 
  }

  return 0;
}

unsigned long
send_read_command (struct usbdevice *rio_dev, int address, int num_blocks, int card)
{
  int length = num_blocks * 0x4000;
  int num_big_reads, num_small_reads;
  unsigned long read_status=0;

  num_big_reads   = length / 0x10000;
  num_small_reads = length % 0x10000;

  /* rio returns 0 on failure . . we use -1 */

  read_status = send_command (rio_dev, 0x4e, address, card);
  /* if command fails end comm gracefully and return -1 */
  if (read_status == 0)
  {
	finish_communication(rio_dev);
	return -1; 
  }

  read_status = send_command (rio_dev, 0x45, num_big_reads, num_small_reads);
  /* if command fails end comm gracefully and return -1 */ 
  if (read_status == 0)
  {
	finish_communication(rio_dev);
	return -1; 
  }

  return 0;
}

unsigned long
send_command (struct usbdevice *rio_dev, int req, int val, int idx)
{
  unsigned long status = 0;
  int ret;

  ret = rio_ctl_msg (rio_dev, RIO_DIR_IN, req, val, idx, 4, (void*)&status);

#ifdef WORDS_BIGENDIAN
   status = bswap_32 (status);
#endif

  return (ret < 0) ? -1 : status;

}

int
rio_ctl_msg (struct usbdevice *rio_dev, int direction, unsigned char request, unsigned short value, unsigned short index, unsigned short length, void *data)
{
  int ret;

  unsigned char requesttype = 0;

  if (direction == RIO_DIR_IN)
    requesttype = USB_DIR_IN |
        USB_TYPE_VENDOR | USB_RECIP_DEVICE;
  else
    requesttype = USB_DIR_OUT |
        USB_TYPE_VENDOR | USB_RECIP_DEVICE;

/* The 5000 is a timeout value */
  ret = usb_control_msg(rio_dev, requesttype, request, value, index, length, data, 5000);
  return (ret < 0) ? -1 : 0;
}

void
dump_block (FILE *fp, BYTE *block, int num_bytes)
{
  int i;

  for (i = 0; i < num_bytes; i++)
    fputc (block[i], fp);
}

rio_bitmap_data *
smiley_new ()
{
  rio_bitmap_data *new_smiley;
  char smiley[] = {0x00, 0x00, 0x00, 0x00,
                   0x3e, 0x41, 0x94, 0x80,
                   0xa2, 0x9d, 0x41, 0x3e,
                   0x00, 0x00, 0x00, 0x00,
                   0x00, 0x00, 0x00, 0x00,
                   0x00, 0x00, 0x80, 0x80,
                   0x80, 0x80, 0x00, 0x00,
                   0x00, 0x00, 0x00, 0x00};

  
   new_smiley = malloc ( sizeof (rio_bitmap_data) );
   if (new_smiley)
   {
     memcpy (&new_smiley->bitmap, smiley, 32);
     new_smiley->num_blocks = 2;
   }

/* on big_endian platforms, need to byteswap num_blocks to write to le rio */

#ifdef WORDS_BIGENDIAN
   new_smiley->num_blocks = bswap_16(new_smiley->num_blocks);
#endif

   return new_smiley;
}

rio_bitmap_data *
bitmap_data_new (char *name, char *font_name, int font_number)
{
  BYTE mp3_bits[] = { 0x00, 0x00, 0x00, 0x00,
                      0x00, 0x00, 0xbc, 0xd2,
                      0x92, 0x92, 0x92, 0x92,
		      0x00, 0x00, 0x00, 0x00,

		      0x00, 0x00, 0x00, 0x00,
                      0x01, 0x02, 0xb0, 0xc8,
                      0x88, 0x88, 0xca, 0xb1,
                      0x80, 0x80, 0x00, 0x00,

		      0x00, 0x00, 0x00, 0x00,
		      0xc0, 0x20, 0x20, 0xc0,
		      0x20, 0x20, 0x20, 0xc0,
		      0x00, 0x00, 0x00, 0x00};
  rio_bitmap_data *new_bitmap;
  struct fon_font *f;
  psf_font *g;

  if (name == NULL)
    return smiley_new ();


  new_bitmap = malloc ( sizeof (rio_bitmap_data) );
  if (new_bitmap)
  {
    f = fon_load_font (font_name);
    if (f)
    {
      f->font_number = font_number;
      new_bitmap->num_blocks = fon_write_string (f, name, 
		(BYTE*)&new_bitmap->bitmap);
      fon_delete_font (f);
    } else {
      g = psf_load_font(DEFAULT_PSF_FONT);
      if (g)
      {
	printf("%s load failed.  Trying default.psf font\n",font_name);
	new_bitmap->num_blocks = psf_write_string(g, name,
		  (BYTE*)&new_bitmap->bitmap);
        psf_delete_font (g);
      }
      else {
        printf("All font loads failed.  Creating folder named mp3\n");
	memcpy (&new_bitmap->bitmap, mp3_bits, 48);
        new_bitmap->num_blocks = 3;
      }
    }
  }

/* get bitmap struct into little endian format in preparation for 
   transfer to rio */

#ifdef WORDS_BIGENDIAN
  new_bitmap->num_blocks = bswap_16(new_bitmap->num_blocks);
#endif

  return new_bitmap;
}


/* safe_strcpy.  Borrowed from Samba */

char *safe_strcpy(char *dest,const char *src, size_t maxlength)
{
    size_t len;

    if (!dest) {
        printf("ERROR: NULL dest in safe_strcpy\n");
        return NULL;
    }

    if (!src) {
        *dest = 0;
        return dest;
    }

    len = strlen(src);

    if (len > maxlength) {
           printf("ERROR: string overflow by %d in safe_strcpy [%.50s]\n",
                     (int)(len-maxlength), src);
            len = maxlength;
    }

    memcpy(dest, src, len);
    dest[len] = 0;
    return dest;
}

/* safe_strcpy.  Borrowed from Samba */

char *safe_strcat(char *dest, const char *src, size_t maxlength)
{
    size_t src_len, dest_len;

    if (!dest) {
        printf("ERROR: NULL dest in safe_strcat\n");
        return NULL;
    }

    if (!src) {
        return dest;
    }

    src_len = strlen(src);
    dest_len = strlen(dest);

    if (src_len + dest_len > maxlength) {
            printf("ERROR: string overflow by %d in safe_strcat [%.50s]\n",
                     (int)(src_len + dest_len - maxlength), src);
            src_len = maxlength - dest_len;
    }

    memcpy(&dest[dest_len], src, src_len);
    dest[dest_len + src_len] = 0;
    return dest;
}

int lprintf(unsigned vl, const char *format, ...)
{
        va_list ap;
        int r;

        if (vl > verboselevel)
                return 0;
        va_start(ap, format);
#ifdef HAVE_VSYSLOG
        if (syslogmsg) {
                static const int logprio[] = { LOG_ERR, LOG_INFO };
                vsyslog((vl > 1) ? LOG_DEBUG : logprio[vl], format, ap);
                r = 0;
        } else
#endif
                r = vfprintf(stderr, format, ap);
        va_end(ap);
        return r;
}



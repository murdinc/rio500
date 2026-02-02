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


/* 
    -------------------
    Function prototypes 
    -------------------
*/

#include "rio500_usb.h"
#include "glib.h"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#define _GNU_SOURCE
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <sys/ioctl.h>


#ifdef WITH_USBDEVFS
#ifdef HAVE_LINUX_USBDEVICE_FS_H
#include <linux/types.h>
#include <linux/usbdevice_fs.h>
#endif

#if (!defined(HAVE_LINUX_USBDEVICE_FS_H))
#include <linux/types.h>
#include "usbdevice_fs.h"
#endif

#include "usbdevfs.h"
#endif /* WITH_USBDEVFS */

#ifndef LIBRIO500_H
#define LIBRIO500_H

typedef guint16 WORD;
typedef guint8	BYTE;
typedef guint32 DWORD;
typedef char string[256];

/* RIO COMMANDS */

#define READ_FROM_USB               0x45
#define WRITE_TO_USB                0x46
#define START_USB_COMM              0x47
#define END_USB_COMM                0x48
#define RIO_FORMAT_DEVICE           0x4d
#define QUERY_FREE_MEM              0x50
#define QUERY_OFFSET_LAST_WRITE     0x43
#define SEND_FOLDER_LOCATION        0x56
#define END_FOLDER_TRANSFERS        0x58

#define FOLDER_BLOCK_SIZE           0x4000

#define BUFFER_SIZE                 0x4000

#ifdef WITH_USBDEVFS
struct usb_device_descriptor_x {
        __u8  bLength;
        __u8  bDescriptorType;
        __u8  bcdUSB[2];
        __u8  bDeviceClass;
        __u8  bDeviceSubClass;
        __u8  bDeviceProtocol;
        __u8  bMaxPacketSize0;
        __u8  idVendor[2];
        __u8  idProduct[2];
        __u8  bcdDevice[2];
        __u8  iManufacturer;
        __u8  iProduct;
        __u8  iSerialNumber;
        __u8  bNumConfigurations;
};

struct usbdevice {
        int fd;
        struct usb_device_descriptor_x desc;
};
struct usbdevice;
#endif


typedef struct
{
  WORD    num_blocks;
  BYTE    bitmap[1536];
} rio_bitmap_data;

typedef struct
{
  WORD             offset;
  WORD             dunno1;
  DWORD            length;
  WORD             dunno2;
  WORD             dunno3;
  DWORD            mp3sig;
  DWORD            time;
  rio_bitmap_data bitmap; 
  BYTE             name1[362];
  BYTE             name2[128];
} song_entry;


typedef struct
{
  WORD            offset;
  WORD            dunno1;
  WORD            fst_free_entry_off;
  WORD            dunno2;
  DWORD           dunno3;
  DWORD           dunno4;
  DWORD           time;
  rio_bitmap_data bitmap; 
  BYTE            name1[362];
  BYTE            name2[128];
} folder_entry;

typedef struct
{
  WORD            dunno1;
  WORD            block_size;
  WORD            num_blocks;
  WORD            first_free_block;
  WORD            num_unused_blocks;
  DWORD           dunno2;
  DWORD           dunno3;
} mem_status;

typedef struct
{
  WORD            offset;
  WORD            bytes;
  WORD            folder_num; 
} folder_location;


/* functions order from high-level to low-level */
#ifndef WITH_USBDEVFS

mem_status    *get_mem_status (int fd, int card);
unsigned long  query_mem_left (int fd, int card);
unsigned long  query_firmware_rev (int fd);
void           send_folder_location (int fd, int offset, int folder_num, int card);
void           format_flash (int fd, int card); 
void           init_communication (int fd);
void           finish_communication (int fd);

void  bswap_folder_entry(folder_entry *);
void  bswap_song_entry(song_entry *);
GList *read_folder_entries (int fd, int card);
GList *read_song_entries (int fd, GList *folder_entries, int folder_num, int card);
void   write_folder_entries (int fd, GList *entries, int card);
void   write_song_entries (int fd, int folder_num, GList *entries, int card);
unsigned long get_num_folder_blocks (int fd, int address, int card);

unsigned long  send_command (int fd, int req, int value, int index);
unsigned long  send_read_command (int fd, int address, int num_blocks, int card);
unsigned long  send_write_command (int fd, int address, int num_blocks, int card);

void rio_ctl_msg (int fd, int dir, int req, int val, int idx, int len, BYTE *d);
int  bulk_read (int fd, BYTE *block, int num_bytes);
int  bulk_write (int fd, BYTE *block, int num_bytes);
void dump_block (FILE *fp, BYTE *block, int num_bytes);
struct usbdevice *usb_open_bynumber(unsigned int busnum, unsigned int devnum, int vendorid, int productid);
struct usbdevice *usb_open(int vendorid, int productid, unsigned int timeout);

#else

mem_status    *get_mem_status (struct usbdevice *rio_dev, int card);
unsigned long  query_mem_left (struct usbdevice *rio_dev, int card);
void           send_folder_location (struct usbdevice *rio_dev, int offset, int folder_num, int card);
void           format_flash (struct usbdevice *rio_dev, int card);
struct usbdevice *init_communication ();
void           finish_communication (struct usbdevice *rio_dev);
void           rio_usb_close (struct usbdevice *rio_dev);

void  bswap_folder_entry(folder_entry *);
void  bswap_song_entry(song_entry *);
GList *read_folder_entries (struct usbdevice *rio_dev, int card);
GList *read_song_entries (struct usbdevice *rio_dev, GList *folder_entries, int folder_num, int card);
void   write_folder_entries (struct usbdevice *rio_dev, GList *entries, int card);
void   write_song_entries (struct usbdevice *rio_dev, int folder_num, GList *entries, int card);

unsigned long  send_command (struct usbdevice *rio_dev, int req, int value, int index);
unsigned long  send_read_command (struct usbdevice *rio_dev, int address, int num_blocks, int card);
unsigned long  send_write_command (struct usbdevice *rio_dev, int address, int num_blocks, int card);

int rio_ctl_msg (struct usbdevice *rio_dev, int dir, unsigned char req, unsigned short val, unsigned short idx, unsigned short len, void *d);
int bulk_read (struct usbdevice *rio_dev, void *block, int num_bytes);
int bulk_write (struct usbdevice *rio_dev, void *block, int num_bytes);
int rio_usb_bulk (struct usbdevice *rio_dev, int ep, void *block, int len);
void dump_block (FILE *fp, BYTE *block, int num_bytes);

#endif

song_entry    * song_entry_new (char *name, char *font_name, int font_number);
folder_entry  * folder_entry_new (char *name, char *font_name, int font_number);
rio_bitmap_data  * bitmap_data_new (char *name, char *font_name, int font_number);
BYTE             * new_empty_block ();
void               clear_block (BYTE *block);


/* safe_strcpy and safe_strcat from samba */
char *safe_strcpy(char *dest,const char *src, size_t maxlength);
char *safe_strcat(char *dest, const char *src, size_t maxlength);



#endif /* LIBRIO500_H */

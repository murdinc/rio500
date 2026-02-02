#include <librio500_api.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>

/* Local functions */
static void rio_api_open_l (Rio500 *rio);
static void rio_api_clear_folders_l (GList *folders);
static void rio_api_clear_songs_l (GList *songs);
static GList *rio_api_read_songs_l (Rio500 *rio, GList *folders, int numf);

static int    write_song (Rio500 *rio, char *filename);
static int    file_size (char *filename);

#ifndef WITH_USBDEVFS

static int    remove_folder (int fd, int folder_num, int card);
static int    remove_song (int fd, int song_num, int folder_num, int card);
static int    is_first_folder (int fd, int card);
static GList *add_song_to_list (GList *, char *fnm, int o, char *font, int fn);
static void   add_folder (int fd, char *name, char *font_name, int font_number, int card);
static void   rename_folder (int rio_dev, int folder_num, char *name, char *font_name, int font_number, int card);
static void   rename_song (int rio_dev, int folder_num, int song_num, char *name, char *font_name, int font_number, int card);

#else /* With USBDEFVFS */

static int    remove_folder (struct usbdevice *rio_dev, int folder_num, int card);
static int    remove_song (struct usbdevice *rio_dev, int song_num, int folder_num, int card);
static int    is_first_folder (struct usbdevice *rio_dev, int card);
static GList *add_song_to_list (GList *, char *fnm, int o, char *font, int fn);
static void   add_folder (struct usbdevice *rio_dev, char *name, char *font_name, int font_number, int card);
static void rename_folder (struct usbdevice *rio_dev, int folder_num, char *name, char *font_name, int font_number, int card);
static void rename_song (struct usbdevice *rio_dev, int folder_num, int song_num, char *name, char *font_name, int font_number, int card);

#endif /* WITH_USBDEVFS */


static void   start_comm (Rio500 *rio);
static void   end_comm (Rio500 *rio);
static gint   g_alpha_sort (gconstpointer a, gconstpointer b);

/* 

               API functions
               -------------

 */




/* -------------------------------------------------------------------
   NAME:        rio_new
   DESCRIPTION: Create a new instance of the rio class.
   ------------------------------------------------------------------- */

Rio500 *
rio_new ()
{
  Rio500  *instance;
  char    *font_name = malloc(strlen(DEFAULT_FONT_PATH)+strlen(DEFAULT_FON_FONT)+1);

  instance = calloc (sizeof (Rio500), 1);
  if (instance == NULL)
    return NULL;

  /* set defaults */
  instance->font_num = 0;
  strcpy(font_name,DEFAULT_FONT_PATH);
  strcat(font_name, DEFAULT_FON_FONT);
  instance->font = font_name;
  instance->card = 0;
  rio_api_open_l (instance);

  return instance;
}

/* -------------------------------------------------------------------
   NAME:        rio_delete
   DESCRIPTION: Destroy instance of rio object.
   ------------------------------------------------------------------- */

void
rio_delete (Rio500 *rio)
{
  g_return_if_fail (rio != NULL);
#ifndef WITH_USBDEVFS
  if (rio->rio_dev > 0)
    close (rio->rio_dev);
#else
  if (rio->rio_dev != NULL)
    usb_close(rio->rio_dev);
#endif
  free (rio);
  return;
}


/* -------------------------------------------------------------------
   NAME:        rio_destroy_content
   DESCRIPTION: Frees all memory used by a call to rio_get_content.
   ------------------------------------------------------------------- */

void
rio_destroy_content (GList *content)
{
  g_return_if_fail (content);
  rio_api_clear_folders_l (content);
}


/* -------------------------------------------------------------------
   NAME:        rio_format
   DESCRIPTION: Formats the Rio.
   ------------------------------------------------------------------- */

int
rio_format (Rio500 *rio)
{
  g_return_val_if_fail (rio != NULL, -1);
#ifndef WITH_USBDEVFS
  g_return_val_if_fail (rio->rio_dev > 0, -1);
#else
  g_return_val_if_fail (rio->rio_dev == NULL, -1);
#endif

  start_comm (rio);

  format_flash (rio->rio_dev,rio->card);

  end_comm (rio);

  return TRUE;
}


/* -------------------------------------------------------------------
   NAME:        rio_set_report_func
   DESCRIPTION: This sets the report function that will be used by
                all API routines to report progress. This is used to
		display progress information while carrying out 
		operations on the rio. This is useful, for example,
		during downloads to rio to show progress and to
		be able to accept input for abort.
   ------------------------------------------------------------------- */

   int
   rio_set_report_func (Rio500 *rio, RioStatusFunc func)
   {
     g_return_val_if_fail (rio, -1);
     rio->stat_func = func;
     return TRUE;
   }


/* -------------------------------------------------------------------
   NAME:        rio_get_content
   DESCRIPTION: Reads all folder and song information from the rio.
                It returns a GList (double linked lists) of 
		RioFolderEntries. Each entry has the name of the
		folder, the folder number (starting from 0) and a 
		GList of RioSongEntries.
   ------------------------------------------------------------------- */

GList *
rio_get_content (Rio500 *rio)
{
  GList *folders, *content, *item, *song_item;
  RioFolderEntry *folder;
  RioSongEntry *song;
  folder_entry *entry;
  char message[255];
  int folder_num;

  g_return_val_if_fail (rio != NULL, NULL);

  /* Open connection to rio */
  start_comm (rio);

  /* Read the stuff */
  folders = read_folder_entries (rio->rio_dev, rio->card);
  folder_num = 0;
  content = NULL;

  /* Check to see if read_folder_entries returned nada */
  if (folders == NULL)
  {
     end_comm(rio);
     return NULL;
  }

  for (item = g_list_first (folders); item; item = item->next)
   {
     entry = (folder_entry *)item->data;
     if (entry == NULL)
       continue;
     folder = calloc (sizeof (RioFolderEntry), 1);
     if (folder)
     {
       folder->name = g_strdup (entry->name1);
       sprintf (message, "Reading songs from folder %s", folder->name);
       if (rio->stat_func)
         (*rio->stat_func)(0, message, 0);
       folder->songs = rio_api_read_songs_l (rio, folders,  folder_num);
       folder->folder_num = folder_num;
       song_item = g_list_first (folder->songs);
       for ( ; song_item ; song_item = song_item->next)
	 {
	   song = (RioSongEntry *) song_item->data;
	   song->parent = folder;
	 }
       content = g_list_append (content, folder);
       folder_num++;
     }
   }

  /* Finish communication */
  end_comm (rio);

  return g_list_first (content);
}


/* -------------------------------------------------------------------
   NAME:        rio_get_song_list
   DESCRIPTION: Returns a GList of song_entry for the folder_num
                folder. folders is a GList of folder_entry structs.
   ------------------------------------------------------------------- */

GList *
rio_get_song_list (Rio500 *rio, GList *folders, int folder_num)
{
  GList *songs;

  g_return_val_if_fail (rio != NULL, NULL);
  g_return_val_if_fail (rio->rio_dev > 0, NULL);

  /* Open connection to rio */
  start_comm (rio);

  /* Read the stuff */
  songs = read_song_entries (rio->rio_dev, folders, folder_num, rio->card);

  /* Finish communication */
  end_comm (rio);

  return songs;
}

/* -------------------------------------------------------------------
   NAME:        rio_add_song
   DESCRIPTION: Uploads filename (full path to file to upload to rio)
	              to the rio player.
   ------------------------------------------------------------------- */

int
rio_add_song (Rio500 *rio, int folder_num, char *filename)
{
  int               retries, song_location;
  int               folder_block_offset, song_block_offset;
  int               font_number, mem_left;
  GList            *folders, *songs;
  folder_entry     *f_entry;
  char             *font_name = rio->font;

  /* set defaults */
  font_number = rio->font_num;

  /* Init communication with rio */
  start_comm (rio);

  /* Make sure there's enough space left */
  /* Sometimes quert_mem_left returns 0 but there really is space in
     the device. So... try 3 times and make sure that it really is
     returning 0. */
  retries = 3;
  while (retries-- > 0)
  {
    mem_left = query_mem_left (rio->rio_dev,rio->card);
    send_command (rio->rio_dev, 0x42, 0, 0);
    if (mem_left > 0)
      break;
  }
  if (file_size (filename) > mem_left)
  {
     end_comm (rio);
     return (-1);
  }

  /* Read folder & song block */
  folders = read_folder_entries (rio->rio_dev,rio->card);
  if ( folder_num > g_list_length (folders)-1 )
    folder_num = 0;
  songs   = read_song_entries ( rio->rio_dev, folders, folder_num, rio->card);

  /* Write the song to the Rio */
  song_location = write_song (rio, filename);

  /* Add an entry to the song block */
 songs = add_song_to_list ( songs, filename, song_location, font_name, font_number);
  if (songs == NULL)
  {
    end_comm (rio);
    return (-1);
  }

  /* Write song block to the correct folder */
  write_song_entries (rio->rio_dev, folder_num, songs ,rio->card);
  send_command (rio->rio_dev, 0x42, 0, 0);
  send_command (rio->rio_dev, 0x42, 0, 0);

  song_block_offset = send_command (rio->rio_dev, 0x43, 0x0, 0x0);

  /* Now write the folder block again */
  f_entry = (folder_entry *) ((GList *) g_list_nth (folders, folder_num))->data; 
  f_entry->offset = song_block_offset;
  f_entry->fst_free_entry_off += 0x800;

  write_folder_entries ( rio->rio_dev, folders ,rio->card);
  send_command (rio->rio_dev, 0x42, 0, 0);
  send_command (rio->rio_dev, 0x42, 0, 0);
  folder_block_offset = send_command (rio->rio_dev, 0x43, 0x0, 0x0);

  /* Tell Rio where the root folder block is. */
  send_folder_location (rio->rio_dev, folder_block_offset, folder_num, rio->card);

  /* Not really sure what this does */
  send_command (rio->rio_dev, 0x58, 0x0, rio->card);

  /* Close device */
  end_comm (rio);

  return (1);
}


/* -------------------------------------------------------------------
   NAME:        rio_del_song
   DESCRIPTION: Deletes song_num from folder_num.
   ------------------------------------------------------------------- */

int
rio_del_song (Rio500 *rio, int folder_num, int song_num)
{
  int status = -1;
  g_return_val_if_fail (rio != NULL, -1);

#ifndef WITH_USBDEVFS
  g_return_val_if_fail (rio->rio_dev > 0, -1);
#else
  g_return_val_if_fail (rio->rio_dev == NULL, -1);
#endif

  /* Open connection to rio */
  start_comm (rio);

  /* Do a quick check on the parameters */
	if (folder_num < 0 || song_num < 0)
	  return status;

	/* Call internal function to delete song. */
	status = remove_song (rio->rio_dev, song_num, folder_num, rio->card);
 
  /* Finish communication */
  end_comm (rio);

  return status;
}

/* -------------------------------------------------------------------
   NAME:        rio_del_folder
   DESCRIPTION: Removes the folder_num'th folder from the rio.
   ------------------------------------------------------------------- */

int
rio_del_folder (Rio500 *rio, int folder_num)
{
  int status = -1;

  g_return_val_if_fail (rio != NULL, -1);

#ifndef WITH_USBDEVFS
  g_return_val_if_fail (rio->rio_dev > 0, -1);
#else
  g_return_val_if_fail (rio->rio_dev == NULL, -1);
#endif

  /* Open connection to rio */
  start_comm (rio);

  /* Do a quick check on the parameters */
	if (folder_num < 0)
	  return status;

	/* Call internal function to delete song. */
	status = remove_folder (rio->rio_dev, folder_num, rio->card);
 
  /* Finish communication */
  end_comm (rio);

  return status;
}

/* -------------------------------------------------------------------
   NAME:        rio_add_folder
   DESCRIPTION: Adds folder with name folder_name to the rio.
   ------------------------------------------------------------------- */

int
rio_add_folder (Rio500 *rio, char *folder_name)
{
  int font_number=rio->font_num;
  char *font_name = rio->font;
#ifndef WITH_USBDEVFS
  int rio_dev;
#else
  struct usbdevice *rio_dev;
#endif

  g_return_val_if_fail (rio != NULL, -1);
  g_return_val_if_fail (folder_name != NULL, -1);


  /* Init communication with rio */
  start_comm (rio);

  add_folder (rio->rio_dev, folder_name, font_name, font_number, rio->card);

  /* Close device */
  end_comm (rio);

  return 1;
}

/* -------------------------------------------------------------------
   NAME:        rio_add_directory
   DESCRIPTION: Adds an entire directory of .mp3's the rio.
   ------------------------------------------------------------------- */

/* In progress: keith -- 2/19/00 */

int
rio_add_directory(Rio500 *rio, char *dir_name, int folder_num)
{
  int count=0, font_number=rio->font_num, ret, mem_left, dirsize=0;
  int retries=0;
  char *font_name = rio->font;
  char *sp, *olddir;
  DIR *dp;
  struct dirent *de;
  GList *songs = NULL;
  GList *next_song = NULL;
#ifndef WITH_USBDEVFS
  int rio_dev;
#else
  struct usbdevice *rio_dev;
#endif

  g_return_val_if_fail (rio != NULL, -1);
  g_return_val_if_fail (dir_name != NULL, -1);

  /* Open up directory and find mp3's */
  
  dp = opendir(dir_name);
  if (dp == NULL) {
#ifdef DEBUG
	printf("Cannot open directory %s\n",dir_name);
#endif
	return(RIO_NODIR);
  }

  ret = chdir(dir_name);
  if (ret < 0) {return RIO_NODIR;}

  while((de=readdir(dp)) != NULL) {
    sp=(char *)(strstr(de->d_name,".mp3"));
    if(sp != NULL) {
	dirsize=dirsize+file_size(de->d_name);
     	songs = g_list_append(songs,de->d_name);
    }  
  }  

  /* alphabetize the song list to add */
  
  songs = g_list_sort(songs, g_alpha_sort);
  
  /* Init communication with rio */
  start_comm (rio);

  /* Make sure there's enough space left */
  /* Sometimes quert_mem_left returns 0 but there really is space in
     the device. So... try 3 times and make sure that it really is
     returning 0. */

  retries = 3;
  while (retries-- > 0)
  {
    mem_left = query_mem_left (rio->rio_dev,0);
    send_command (rio->rio_dev, 0x42, 0, 0);
    if (mem_left > 0)
      break;
  }
  if (dirsize > mem_left)
  {
     end_comm (rio);
     return (RIO_NOMEM);
  }

  for(next_song=g_list_first(songs);next_song;next_song=next_song->next)
  {
#ifdef DEBUG
	printf("Transferring %s\n",next_song->data);
#endif
	ret = rio_add_song(rio,folder_num,next_song->data);
	
	if (ret < 0)
	{
           return ret;
	   /* Need error message in here */
	   break;
	}
  }

  /* Close device */
  end_comm (rio); 
  return RIO_SUCCESS;
}

/* -------------------------------------------------------------------
   NAME:        rio_rename_folder
   DESCRIPTION: Renames the foldernum to folder_name.
   ------------------------------------------------------------------- */

int
rio_rename_folder (Rio500 *rio, int folder_num, char *folder_name)
{
  int status = -1;
  char *font_name = rio->font;
  int fnum = folder_num;
  g_return_val_if_fail (rio != NULL, -1);

#ifndef WITH_USBDEVFS
  g_return_val_if_fail (rio->rio_dev > 0, -1);
#else
  g_return_val_if_fail (rio->rio_dev == NULL, -1);
#endif

  /* Open connection to rio */
  start_comm (rio);

  rename_folder (rio->rio_dev, fnum, folder_name, font_name, rio->font_num, rio->card);

  /* Finish communication */
  end_comm (rio);

  return status;
}

/* -------------------------------------------------------------------
   NAME:        rio_rename_song
   DESCRIPTION: Renames the songnum in foldernum to song_name.
   ------------------------------------------------------------------- */

int
rio_rename_song (Rio500 *rio, int folder_num, int song_num, char *song_name)
{
  int status = -1;
  char *font_name = rio->font;
  int fnum = folder_num;

  g_return_val_if_fail (rio != NULL, -1);

#ifndef WITH_USBDEVFS
  g_return_val_if_fail (rio->rio_dev > 0, -1);
#else
  g_return_val_if_fail (rio->rio_dev == NULL, -1);
#endif

  /* Open connection to rio */
  start_comm (rio);

  rename_song (rio->rio_dev, fnum, song_num, song_name, font_name, rio->font_num, rio->card);

  /* Finish communication */
  end_comm (rio);

  return status;
}
/* -------------------------------------------------------------------
   NAME:        rio_memory_left
   DESCRIPTION: Return memory left in internal or external memory.
   ------------------------------------------------------------------- */

unsigned long
rio_memory_left (Rio500 *rio)
{
  unsigned long result;

  g_return_val_if_fail (rio != NULL, -1);
#ifndef WITH_USBDEVFS
  g_return_val_if_fail (rio->rio_dev > 0, -1);
#else
  g_return_val_if_fail (rio->rio_dev == NULL, -1);
#endif

  start_comm (rio);

  result = query_mem_left (rio->rio_dev,rio->card);

  end_comm (rio);

  return result;
}

/* -------------------------------------------------------------------
   NAME:        rio_get_mem_total
   DESCRIPTION: Return total memory available either internally
		or externally.
   ------------------------------------------------------------------- */

unsigned long rio_get_mem_total(Rio500* rio)
{
    unsigned long result;
    mem_status *status;

    g_return_val_if_fail (rio != NULL, -1);
#ifndef WITH_USBDEVFS
    g_return_val_if_fail (rio->rio_dev > 0, -1);
#else
    g_return_val_if_fail (rio->rio_dev == NULL, -1);
#endif

    start_comm (rio);
    status = get_mem_status(rio->rio_dev, rio->card);
    result = (status->num_blocks * status->block_size);
    end_comm (rio);

    return result;
}

/* -------------------------------------------------------------------
   NAME:        rio_set_font
   DESCRIPTION: sets font name/number to be stored in rio500 struct.
		By default this is filled with the compiled in font.
		All fonts must be located in the default font path
		and all must be either .fon fonts or .psf fonts
   ------------------------------------------------------------------- */

int
rio_set_font (Rio500 *rio, char *font_name, int font_number)
{
  char *temp = DEFAULT_FONT_PATH;  
  char *font = malloc(strlen(temp)+strlen(font_name)+1);
 
  g_return_val_if_fail (rio != NULL, -1);
#ifndef WITH_USBDEVFS
  g_return_val_if_fail (rio->rio_dev > 0, -1);
#else
  g_return_val_if_fail (rio->rio_dev == NULL, -1);
#endif

  strcpy(font,DEFAULT_FONT_PATH);
  strcat(font,font_name);

  rio->font_num = font_number;
  rio->font = font;
  return 0;

}

/* -------------------------------------------------------------------
   NAME:        rio_set_card
   DESCRIPTION: Pass 1 if operations to involve external card
                pass 0 if operations to use internal memory
   ------------------------------------------------------------------- */

int
rio_set_card (Rio500 *rio, int card)
{

  g_return_val_if_fail (rio != NULL, -1);
#ifndef WITH_USBDEVFS
  g_return_val_if_fail (rio->rio_dev > 0, -1);
#else
  g_return_val_if_fail (rio->rio_dev == NULL, -1);
#endif

  rio->card = card;
  return 0;

}

/* -------------------------------------------------------------------

                            Internal functions

   ------------------------------------------------------------------- */

static void
rio_api_open_l (Rio500 *rio)
{
#ifndef WITH_USBDEVFS
  if (rio)
    rio->rio_dev = open (DEFAULT_DEV_PATH, O_RDWR);
#endif
}

static GList *
rio_api_read_songs_l (Rio500 *rio, GList *folders, int num_folder)
{
  GList      *songs, *rio_song_list, *item;
  song_entry *entry;
  int song_num;
  RioSongEntry *song;

  songs = read_song_entries (rio->rio_dev, folders, num_folder,rio->card);
  rio_song_list = NULL;
  song_num = 0;
  for (item = songs; item; item = item->next)
  {
    entry = (song_entry *)item->data;
    if (entry == NULL)
      continue;
    song = malloc (sizeof (RioSongEntry));
    if (song)
    {
      song->name = g_strdup (entry->name1);
      song->size = (unsigned long)entry->length;
      song->song_num = song_num++;
      rio_song_list = g_list_append (rio_song_list, song);
    }
  }

  return g_list_first (rio_song_list);
}

static void
rio_api_clear_folders_l (GList *folders)
{
  GList *item;
  RioFolderEntry *folder;

  for (item = g_list_first (folders); item; item = item->next)
  {
    folder = (RioFolderEntry *)item->data; 
    if (folder)
    {
      if (folder->name)
        free (folder->name);
      if (folder->songs)
        rio_api_clear_songs_l (folder->songs);
    }
  }

  return;
}

static void
rio_api_clear_songs_l (GList *songs)
{
  GList *item;
  RioSongEntry *song;

  for (item = g_list_first (songs); item; item = item->next)
  {
    song = (RioSongEntry *)item->data; 
    if (song)
    {
      if (song->name)
        free (song->name);
    }
  }

  return;
}

GList *
add_song_to_list (GList *songs, char *filename, int offset, char *font_name, int font_number)
{
  FILE                *fp;
  GList               *new_entry;
  int                  size;
  char                *striped_name;
  song_entry          *entry;

  fp = fopen (filename, "r");
  if (fp == NULL)
    return NULL;
  fclose (fp);

  size = file_size (filename);
  striped_name = g_basename (filename);

  /* Fill file info */
  entry = song_entry_new (striped_name, font_name, font_number);
  entry->offset = (WORD)  offset;
  entry->length = (DWORD) size;

  new_entry = g_list_append (songs, entry);

  return g_list_first (new_entry);
}

int
write_song (Rio500 *rio, char *filename)
{
  int input_file;
  int  i, j, size;
  int  total, count, num_blocks, remainder, blocks_left;
  BYTE *block, *p;
  int num_chunks, song_location;
  char message[255];

  block = (char *)malloc (0x80000);

  i = 0;
  input_file = open (filename, O_RDONLY);
  if (input_file == -1)
    return -1;

  size = file_size (filename);

  num_blocks = size / 0x10000;
  remainder  = size % 0x10000;

  sprintf (message, "Transfering %s ...", g_basename (filename));

  /* First send num_chunks, blocks at a time */
  num_chunks = 0x10;
  total = 0;
  blocks_left = num_blocks - num_chunks;

  send_command (rio->rio_dev, 0x4f, 0xffff, rio->card);
  while (blocks_left > 0)
  {
    send_command (rio->rio_dev, 0x46, num_chunks, 0x0);
    for (j=0;j<num_chunks / 2;j++)
    {
      count = read (input_file, block, 0x20000);
      total += count;
      if (count != 0x20000)
        printf ("[Short read!]");
      bulk_write (rio->rio_dev, block, 0x20000);
      if (rio->stat_func)
        (*rio->stat_func)(0, message, (int)(100*total/size));
    }
    blocks_left -= num_chunks;
    // Send a 0x42.
    send_command (rio->rio_dev, 0x42, 0, 0);
    send_command (rio->rio_dev, 0x42, 0, 0);
  }

  /* Send remaining blocks */
  blocks_left += num_chunks;
  //send_command (rio_dev, 0x4f, 0xffff, 0);
  send_command (rio->rio_dev, 0x46, blocks_left, 00);
  while (blocks_left > 0)
  {
    count = read (input_file, block, 0x10000);
    total += count;
    if (count != 0x10000)
     printf ("[Short read!]");
    bulk_write (rio->rio_dev, block, 0x10000);
    if (rio->stat_func)
      (*rio->stat_func)(0, message, (int)(100*total/size));
    blocks_left--;
    send_command (rio->rio_dev, 0x42, 0, 0);
    send_command (rio->rio_dev, 0x42, 0, 0);
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
      //send_command (rio_dev, 0x4f, 0xffff, 0);
      send_command (rio->rio_dev, 0x46, 00, 0x4000);
      bulk_write (rio->rio_dev, p, 0x4000);
    } else {
     //send_command (rio_dev, 0x4f, 0xffff, 0);
      send_command (rio->rio_dev, 0x46, 00, j);
      bulk_write (rio->rio_dev, p, j);
    }
    if (rio->stat_func)
      (*rio->stat_func)(0, message, (int)(100*total/size));
    j -= 0x4000;
    p += 0x4000;
    send_command (rio->rio_dev, 0x42, 0, 0);
    send_command (rio->rio_dev, 0x42, 0, 0);
  }

  send_command (rio->rio_dev, 0x42, 0, 0);
  send_command (rio->rio_dev, 0x42, 0, 0);
  song_location = send_command (rio->rio_dev, 0x43, 0, 0);

  return song_location;
}

static int
file_size (char *filename)
{
  int z,input_file;
  struct stat file_stat;
  int  size;


  input_file = open (filename, O_RDONLY);
  if (input_file == -1)
    return 0;

  stat (filename, &file_stat);
  size = file_stat.st_size;
  z = close (input_file);
  return size;
}

static int
#ifndef WITH_USBDEVFS
remove_folder (int rio_dev, int folder_num, int card)
#else
remove_folder (struct usbdevice *rio_dev, int folder_num, int card)
#endif
{
  folder_entry     *folder;
  int               song_num, folder_block_offset;
  GList            *folders, *songs;

  /* Read folder & song block */
  folders = read_folder_entries (rio_dev,card);
  if ( folder_num > g_list_length (folders)-1 )
    return -1;
  else
    songs   = read_song_entries (rio_dev, folders, folder_num,card); 


  folder  = (folder_entry *)g_list_nth_data (g_list_first(folders), folder_num);

  for (song_num = g_list_length (songs) - 1; song_num >= 0; song_num--)
    remove_song (rio_dev, song_num, folder_num, card);

  folders = g_list_remove (folders, (gpointer) folder);

  send_command (rio_dev, 0x4c, ((folder_num << 8) | 0xff), card);
  write_folder_entries (rio_dev, folders,card);
  send_command (rio_dev, 0x42, 0, 0);
  send_command (rio_dev, 0x42, 0, 0);
  folder_block_offset = send_command (rio_dev, 0x43, 0x0, 0x0);

  /* Tell Rio where the root folder block is. */
  send_folder_location (rio_dev, folder_block_offset, 0,card);

  /* Not really sure what this does */
  send_command (rio_dev, 0x58, 0x0, card);

  return 0;
}

static int
#ifndef WITH_USBDEVFS
remove_song (int rio_dev, int song_num, int folder_num, int card)
#else
remove_song (struct usbdevice *rio_dev, int song_num, int folder_num, int card)
#endif
{
  song_entry       *song;
  folder_entry     *f_entry;
  int               folder_block_offset, song_block_offset;
  GList            *folders, *songs;

   /* Read folder & song block */
   folders = read_folder_entries (rio_dev,card);
   if (folders == NULL)
     return -1;
   if ( folder_num > g_list_length (folders)-1 )
   {
     songs   = read_song_entries (rio_dev, folders, 0,card); /* use folder 0 by default */
     folder_num = 0;
   } else
     songs   = read_song_entries (rio_dev, folders, folder_num,card); 

   /* Remove the song entry from the list */
   if ( songs == NULL || song_num > g_list_length (songs) -1)
     return -1;

   song = (song_entry *) g_list_nth_data (songs, song_num);

   songs = g_list_remove (songs, (gpointer)song);

   /* Send remove command to the rio */
   send_command (rio_dev, 0x4c, ((folder_num << 8) | song_num), card);

   /* Write song block to the correct folder */
   write_song_entries (rio_dev, folder_num, songs, card);
   send_command (rio_dev, 0x42, 0, 0);
   send_command (rio_dev, 0x42, 0, 0);

   song_block_offset = send_command (rio_dev, 0x43, 0x0, 0x0);

   /* Now write the folder block again */
   f_entry = (folder_entry *) ((GList *) g_list_nth (folders, folder_num))->data; 
   f_entry->offset = song_block_offset;
   f_entry->fst_free_entry_off -= 0x800;

   write_folder_entries ( rio_dev, folders, card);
   send_command (rio_dev, 0x42, 0, 0);
   send_command (rio_dev, 0x42, 0, 0);
   folder_block_offset = send_command (rio_dev, 0x43, 0x0, 0x0);

   /* Tell Rio where the root folder block is. */
   send_folder_location (rio_dev, folder_block_offset, folder_num,card);

   /* Not really sure what this does */
   send_command (rio_dev, 0x58, 0x0, card);

   return 0;
}


static int
#ifndef WITH_USBDEVFS
is_first_folder (int rio_dev, int card)
#else
is_first_folder (struct usbdevice *rio_dev, int card)
#endif
{
  int result;

  result = send_command (rio_dev, 0x59, 0xff00, card);
  if (result > 0)
    return FALSE;

  /* Try again just in case */
  send_command (rio_dev, 0x42, 0, 0);
  result = send_command (rio_dev, 0x59, 0xff00, card);
  send_command (rio_dev, 0x42, 0, 0);
  send_command (rio_dev, 0x42, 0, 0);
  send_command (rio_dev, 0x58, 0x0, card);
  if (result > 0)
    return FALSE;

  return TRUE;
}

static void
#ifndef WITH_USBDEVFS
add_folder (int rio_dev, char *name, char *font_name, int font_number, int card)
#else
add_folder (struct usbdevice *rio_dev, char *name, char *font_name, int font_number, int card)
#endif
{   
  GList *folders;
  int   song_block_loc, last_folder;
  int   folder_block_loc;
  folder_entry *entry;

  folders = NULL;

  /* Check if this is the first folder */ 
  if ( is_first_folder (rio_dev, card) )
  {
    folders = NULL; 
  } else {
    folders = read_folder_entries (rio_dev,card);
  }

  /* We can only have up to 256 folder entries: 0 - 255 */
  last_folder = g_list_length (folders);
  
  if ( last_folder > 255)
    return;

  /* Now create an new entry for the folder */
  entry = folder_entry_new (name, font_name, font_number);


  /* Write song and folder blocks back to rio */
  write_song_entries (rio_dev, last_folder, NULL,card);

  /* Afer a write it is a good idea to wait a bit */
  send_command (rio_dev, 0x42, 0, 0);
  send_command (rio_dev, 0x42, 0, 0);

  /* Now read the location of the new, empty song block */
  song_block_loc = send_command (rio_dev, 0x43, 0, 0);
  entry->offset = song_block_loc; 

  folders = g_list_append (folders, entry);

  /* Write folder list */
  write_folder_entries (rio_dev, folders,card);

  /* Wait a bit after the read */
  send_command (rio_dev, 0x42, 0, 0);
  send_command (rio_dev, 0x42, 0, 0);

  /* Tell rio where the root folder block is */
  folder_block_loc = send_command (rio_dev, 0x43, 0, 0);
  send_folder_location (rio_dev, folder_block_loc, last_folder,card);

  send_command (rio_dev, 0x58, 0x0, card);

  /* done */
  return;
}

static void
#ifndef WITH_USBDEVFS
rename_folder (int rio_dev, int folder_num, char *name, char *font_name, int font_number, int card)
#else
rename_folder (struct usbdevice *rio_dev, int folder_num, char *name, char *font_name, int font_number, int card)
#endif
{   
  GList *folders, *item;
  int   last_folder;
  int   folder_block_loc;
  folder_entry *entry, *new_entry;

  send_command(rio_dev,0x42,0,0);
  folders = read_folder_entries (rio_dev,card);

  /* Check folder_num rang */
  last_folder = g_list_length (folders);
  if (folder_num < 0 || folder_num > last_folder)
    return;
  
  /* Now create an new entry for the folder */
  new_entry = folder_entry_new (name, font_name, font_number);
  entry     = (folder_entry*) g_list_nth_data (folders, folder_num);

  new_entry->offset = entry->offset;
  new_entry->fst_free_entry_off = entry->fst_free_entry_off;

  /* Replace data */
  item = g_list_nth (folders, folder_num);
  if (item)
  {
    free (item->data);
    item->data = new_entry;
  }

  /* Write folder list */
  write_folder_entries (rio_dev, folders,card);

  /* Wait a bit after the read */
  send_command (rio_dev, 0x42, 0, 0);
  send_command (rio_dev, 0x42, 0, 0);

  /* Tell rio where the root folder block is */
  folder_block_loc = send_command (rio_dev, 0x43, 0, 0);
  send_folder_location (rio_dev, folder_block_loc, last_folder,card);

  send_command (rio_dev, 0x58, 0x0, card);

  /* done */
  return;
}

static void
#ifndef WITH_USBDEVFS
rename_song (int rio_dev, int folder_num, int song_num, char *name, char *font_name, int font_number, int card)
#else
rename_song (struct usbdevice *rio_dev, int folder_num, int song_num, char *name, char *font_name, int font_number, int card)
#endif
{   
  GList *folders, *item, *songs;
  int   last_folder;
  int   folder_block_offset, song_block_offset;
  song_entry *entry, *new_entry;
  folder_entry *f_entry;

  send_command(rio_dev,0x42,0,0);
  folders = read_folder_entries (rio_dev,card);

  /* Check folder_num range */
  last_folder = g_list_length (folders);
  if (folder_num < 0 || folder_num > last_folder)
     folder_num = 0;
  
  songs   = read_song_entries ( rio_dev, folders, folder_num,card);

  /* Now create an new entry for the folder */
  new_entry = song_entry_new (name, font_name, font_number);
  entry     = (song_entry*) g_list_nth_data (songs, song_num);

  new_entry->offset = entry->offset;
  new_entry->dunno1 = entry->dunno1;
  new_entry->dunno2 = entry->dunno2;
  new_entry->dunno3 = entry->dunno3;
  new_entry->mp3sig = entry->mp3sig;
  new_entry->length = entry->length;

  /* Replace data */
  item = g_list_nth (songs, song_num);
  if (item)
  {
    free (item->data);
    item->data = new_entry;
  }

  /* Write song block to the correct folder */
  write_song_entries (rio_dev, folder_num, songs,card );
  send_command (rio_dev, 0x42, 0, 0);
  send_command (rio_dev, 0x42, 0, 0);

  song_block_offset = send_command (rio_dev, 0x43, 0x0, 0x0);

  /* Now write the folder block again */
  f_entry = (folder_entry *) g_list_nth_data (folders, folder_num);
  f_entry->offset = song_block_offset;

  write_folder_entries ( rio_dev, folders,card);
  send_command (rio_dev, 0x42, 0, 0);
  send_command (rio_dev, 0x42, 0, 0);
  folder_block_offset = send_command (rio_dev, 0x43, 0x0, 0x0);

  /* Tell Rio where the root folder block is. */
  send_folder_location (rio_dev, folder_block_offset, folder_num,card);

   /* Not really sure what this does */
  send_command (rio_dev, 0x58, 0x0, card);

  /* Write folder list */
  write_folder_entries (rio_dev, folders,card);

  /* Wait a bit after the read */
  send_command (rio_dev, 0x42, 0, 0);
  send_command (rio_dev, 0x42, 0, 0);

  /* Tell rio where the root folder block is */
  folder_block_offset = send_command (rio_dev, 0x43, 0, 0);
  send_folder_location (rio_dev, folder_block_offset, last_folder,card);

  send_command (rio_dev, 0x58, 0x0, card);

  /* done */
  return;
}



static void
start_comm (Rio500 *rio)
{
  g_return_if_fail (rio != NULL);

#ifndef WITH_USBDEVFS
  if (rio->rio_dev < 0)
    rio_api_open_l (rio);
#else
  if (rio->rio_dev == NULL)
    rio->rio_dev = init_communication();
#endif

#ifndef WITH_USBDEVFS
  g_return_if_fail (rio->rio_dev > 0);
#else
  g_return_if_fail (rio->rio_dev != NULL);
#endif

  if (rio->stat_func)
    (*rio->stat_func) (0, "Opening rio device...",0);

#ifndef WITH_USBDEVFS
  init_communication (rio->rio_dev);
#endif
}

static void
end_comm (Rio500 *rio)
{
  g_return_if_fail (rio != NULL);
#ifndef WITH_USBDEVFS
  if (rio->rio_dev < 0)
    rio_api_open_l (rio);
#else
  if (rio->rio_dev == NULL)
    rio->rio_dev = init_communication();
#endif

#ifndef WITH_USBDEVFS
  g_return_if_fail (rio->rio_dev > 0);
#else
  g_return_if_fail (rio->rio_dev != NULL);
#endif

  if (rio->stat_func)
    (*rio->stat_func) (0, "Communication finished.",0);

  finish_communication (rio->rio_dev);

#ifdef WITH_USBDEVFS
  rio->rio_dev = NULL;
#endif
}

gint g_alpha_sort (gconstpointer a, gconstpointer b)
{
  int x;

  x=strcmp(a,b);
  return x;
}

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
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include "getopt.h"

#include "librio500.h"


#ifndef TRUE

#define TRUE 				1
#define FALSE 				0

#endif /* TRUE */

int name_flag = 0;
int whole_folder = 0;
int folder_num_set = 0;
int card_number = 0;
char *folder_num_string = NULL;
char numstring[4];

static gint   g_alpha_sort (gconstpointer a, gconstpointer b);
char * int_to_string(int x);
void usage (char *progname);
int main(int argc, char *argv[]);
void get_some_switches (int argc, char *argv[], int *folder_num, int *automat, int *card);

#ifndef WITH_USBDEVFS
int remove_folder (int rio_dev, int folder_num, int card_number);
int remove_song (int rio_dev, int song_num, int folder_num, int card_number);
#else
int remove_folder (struct usbdevice *rio_dev, int folder_num, int card_number);
int remove_song (struct usbdevice *rio_dev, int song_num, int folder_num, int card_number);
#endif


void
usage (char *progname)
{
  printf ("\nusage: %s [OPTIONS] <folder/song1> . . . <folder/songN>\n", progname);
  printf ("\n [OPTIONS] Try --help for more information\n");
  printf ("\n <folder/songN> is the index or name of the folder/song you");
  printf ("\n want to erase.");
  printf ("\n\n");
  return;
}

int
main(int argc, char *argv[])
{
  int		    song_retries,folder_retries;
  int		    folder_list_length,song_list_length = 0;
  int               match_flag,count,counter,song_num,folder_num;
  int               num_things_to_delete;
  char		    *song_num_string=NULL;
  GList		    *things_to_delete = NULL;
  GList		    *indices_to_delete = NULL;
  GList		    *rio_folders = NULL;
  char              answer[255];
  int               automatic = 0;
  folder_entry *    folder_ent;
  song_entry *	    song_ent;
  GList		    *arg_ent;
  GList 	    *song_lists[8];
#ifndef WITH_USBDEVFS
  int               rio_dev;
#else
  struct usbdevice *rio_dev;
  rio_dev = malloc(sizeof(struct usbdevice));
#endif

/* Some quick checking before we process switches */

  if (argc < 2)
  {
    usage (argv[0]);
#ifdef WITH_USBDEVFS
    free(rio_dev);
#endif
    exit (-1);
  }


  folder_num=song_num=num_things_to_delete=0;

  /* Process switches */
  get_some_switches(argc,argv,&folder_num,&automatic,&card_number);

  /* Build folder and song lookup lists */

#ifndef WITH_USBDEVFS
  rio_dev = open (DEFAULT_DEV_PATH, O_RDWR);
  if (rio_dev < 0)
    {
      printf ("\nVerify that the rio500.o module is loaded, and your Rio is \n");
      printf ("connected and powered up.\n\n");
      exit (-1);
    }

   /* Init communication with rio */
   init_communication (rio_dev);
#else

   if(!(rio_dev = init_communication())) {
     printf("init_communication() failed!\n");
     return -1;
   }
#endif

  folder_list_length = folder_retries = 0; 
  /* sometimes read_folder_entries fails . . try 3 times */
  while (folder_list_length == 0 && folder_retries < 3)
  {
    send_command (rio_dev, 0x42, 0, 0);

    rio_folders = read_folder_entries (rio_dev,card_number);
    folder_list_length = g_list_length(rio_folders);
    folder_retries++;
  }

  if (g_list_length(rio_folders) == 0)
  {
  	printf("\nReading the folder list from the Rio500 failed\n");
	printf("Make sure your rio has folders/songs to delete and\n");
        printf ("verify that the rio500.o module is loaded, and your Rio is \n");
        printf ("connected and powered up.\n\n");

	   finish_communication (rio_dev);
	#ifndef WITH_USBDEVFS
	   close (rio_dev);
	#endif
        exit(-1);
  }
  /* song lookup list */	
  for(count=0;count<g_list_length(rio_folders);count++)
  {
     song_list_length = song_retries = 0;

    /* If song_list len == 0, retry twice just to be sure */
   
    while (song_list_length == 0 && song_retries < 3)
     {
       song_lists[count]= read_song_entries (rio_dev, rio_folders, count, card_number);
       song_list_length = g_list_length(song_lists[count]);
       song_retries++;
     }
  }

   finish_communication (rio_dev);
#ifndef WITH_USBDEVFS
   close (rio_dev);
#endif

  /* first deal with folder name if -F flag used */

  if (name_flag == 1 && folder_num_set == 1)
  {
    match_flag = 0;
    for(count = 0;count<g_list_length(rio_folders);count++)
    {
	folder_ent = (folder_entry *) g_list_nth(rio_folders,count)->data;
	if (strcmp(folder_num_string,folder_ent->name1) == 0)
	{
           folder_num=count;
	   match_flag = 1;
        }
    }
   /* If no matches of foldername<->folderindex then errormsg and exit */
      if (match_flag == 0)
      {
        printf("\n%s did not match any folder stored on the rio500\n",folder_num_string);
        printf("Check the folder names and try again\n");
        exit(-1);
      }

   }

  /* Make sure folder_num isn't set to something stupid with -F */
  if (folder_num < 0 || folder_num>256) /* new limit is 256 folders */
  {
    printf ("Non-existent folder\n");
    usage (argv[0]);
    exit (-1);
  }

  /* Sanity check -- make sure there is a file to delete */
  if (optind == argc)
  {
	printf("\nNeed to specify a file name/index to delete\n");
	usage(argv[0]);
	exit(-1);
  }

  /* grab all the deletion arguments */

  while (optind < argc)
  {
	  things_to_delete = g_list_append(things_to_delete,argv[optind++]);
          num_things_to_delete++;
  }
  /* do name to index lookups before sorting */
  /* whole folder case first */
  if (name_flag == 1 && whole_folder == 1)
  {
     for(count=0;count<num_things_to_delete;count++)
     {
	arg_ent = g_list_nth(things_to_delete,count);
	match_flag = 0;
	for(counter=0;counter<g_list_length(rio_folders);counter++)
        {
	   folder_ent = (folder_entry *) g_list_nth(rio_folders,counter)->data;
	   if (strcmp((char *)folder_ent->name1,(char *)arg_ent->data) == 0)
	   {
             /* First we need to put counter into string */
	     match_flag = 1;
	     folder_num_string = malloc(4); /* up to index of 999 */
	     int_to_string(counter);
	     folder_num_string = strcpy(folder_num_string,numstring);
	     indices_to_delete = g_list_append(indices_to_delete,folder_num_string);
           }
         }
      /* end of match loop, check that a match was found */
      if (match_flag == 0)
      {
  	printf("\n%s did not match any folder stored on the rio500\n",(char*)arg_ent->data);
	printf("Check the folder names and try again\n");
	exit(-1);
      }
    }
    things_to_delete = indices_to_delete;
   }

   if (name_flag == 1 && whole_folder == 0) /* if args are songs */
   {
      for(count=0;count<num_things_to_delete;count++)
      {
	arg_ent = g_list_nth(things_to_delete,count);
        match_flag = 0;
	for(counter=0;counter<g_list_length(song_lists[folder_num]);counter++)
	{
	  song_ent = (song_entry *) g_list_nth(song_lists[folder_num],counter)->data;
          if (strcmp((char *)song_ent->name1,(char *)arg_ent->data) == 0)
	  {
             /* First we need to put counter into string */
	     match_flag = 1;
             song_num_string = malloc(4); /* up to index of 999 */
             int_to_string(counter);
	     song_num_string = strcpy(song_num_string,numstring);
             indices_to_delete = g_list_append(indices_to_delete,song_num_string);
          }
	}
      /* end of match loop, check that a match was found */
      if (match_flag == 0)
      {
        printf("\n%s did not match any song stored on the rio500\n",(char*)arg_ent->data);
        printf("Check the folder names and try again\n");
        exit(-1);

      }
     }
     things_to_delete = indices_to_delete;
    }	  

  /* sort the glist */
  things_to_delete = g_list_sort(things_to_delete,g_alpha_sort);

  /* loop through list of args and delete */
  for(count = 0; count < num_things_to_delete; count++)
  {
	if (whole_folder == 0) /* grab nth index and get it ready */
	{
         song_num = atoi((char *)(g_list_nth(things_to_delete,count)->data));
	}
	else
	{
	 folder_num = atoi((char *)(g_list_nth(things_to_delete,count)->data));
	}

	/* sanity check -- make sure folder num, song num exist on rio */
	if ((whole_folder == 1) && (folder_num >= folder_list_length))
	{
	  printf("\nYou are attempting to delete a non-existent folder index");
	  printf("\nCheck the folder index number and try again\n");
	  exit(-1);
	}
	if ((whole_folder == 0) && (song_num >= g_list_length(song_lists[folder_num])))
	{
          printf("\nYou are attempting to delete a non-existent song index");
	  printf("\nCheck the song index number and try again\n");
	  exit(-1);
        }
/* Open connection to rio */
#ifndef WITH_USBDEVFS
  rio_dev = open (DEFAULT_DEV_PATH, O_RDWR);
  if (rio_dev < 0)
    {
      printf ("\nVerify that the rio500.o module is loaded, and your Rio is \n");
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

   /* Remove song */ 
   if ( whole_folder == 1)
   {
     if (!automatic)
       {
	folder_ent = (folder_entry *) g_list_nth(rio_folders,folder_num)->data; 
	printf ("Are you sure you want to remove folder <%s>? Type <yes> to confirm. ",(char *)folder_ent->name1);
	 scanf ( "%s", answer );
	 if ( strcmp (answer, "yes") != 0 )
	   {
	     finish_communication (rio_dev);
#ifndef WITH_USBDEVFS
	     close (rio_dev);
#endif
	     exit (0);
	   }
       }
     remove_folder (rio_dev, folder_num, card_number);
   }
   else 
   {
     if (!automatic)
       {
	song_ent=(song_entry *) g_list_nth(song_lists[folder_num],song_num)->data; 
	printf ("Are you sure you want to remove song <%s>? Type <yes> to confirm. ",(char *)song_ent->name1);
         scanf ( "%s", answer );
         if ( strcmp (answer, "yes") != 0 )
           {
             finish_communication (rio_dev);
#ifndef WITH_USBDEVFS
             close (rio_dev);
#endif
             exit (0);
           }
       }
     remove_song (rio_dev, song_num, folder_num, card_number);
    }

   /* Close device */
   finish_communication (rio_dev);
#ifndef WITH_USBDEVFS
   close (rio_dev);
#endif
 } /* end of loop */
#ifdef WITH_USBDEVFS
   free (rio_dev);
#endif
   exit (0);
}

int
#ifndef WITH_USBDEVFS
remove_folder (int rio_dev, int folder_num, int card_number)
#else
remove_folder (struct usbdevice *rio_dev, int folder_num, int card_number)
#endif
{
  folder_entry     *folder;
  int               song_num, folder_block_offset;
  GList            *folders, *songs;

  /* Read folder & song block */
  folders = read_folder_entries (rio_dev,card_number);
  if ( folder_num > g_list_length (folders)-1 )
    return -1;
  else
    songs   = read_song_entries (rio_dev, folders, folder_num, card_number); 


  folder  = (folder_entry *)g_list_nth_data (g_list_first(folders), folder_num);
  if (folder == NULL)
     return 0;
  printf ("Removing folder <%s>...\n", folder->name1);

  for (song_num = g_list_length (songs) - 1; song_num >= 0; song_num--)
    remove_song (rio_dev, song_num, folder_num, card_number);

  folders = g_list_remove (folders, (gpointer) folder);

  send_command (rio_dev, 0x4c, ((folder_num << 8) | 0xff), card_number);
  write_folder_entries (rio_dev, folders,card_number);
  send_command (rio_dev, 0x42, 0, 0);
  send_command (rio_dev, 0x42, 0, 0);
  folder_block_offset = send_command (rio_dev, 0x43, 0x0, 0x0);

#ifdef DEBUG
  fprintf (stderr, "Folder block written to 0x%04x\n", folder_block_offset);
#endif

  /* Tell Rio where the root folder block is. */
  send_folder_location (rio_dev, folder_block_offset, 0, card_number);

  /* Not really sure what this does */
  send_command (rio_dev, 0x58, 0x0, card_number);

  return 0;
}

#ifndef WITH_USBDEVFS
int remove_song (int rio_dev, int song_num, int folder_num,int card_number)
#else
int remove_song (struct usbdevice *rio_dev, int song_num, int folder_num, int card_number)
#endif
{
  song_entry       *song;
  folder_entry     *f_entry;
  int               folder_block_offset, song_block_offset;
  GList            *folders, *songs;

   /* Read folder & song block */
   folders = read_folder_entries (rio_dev, card_number);
   if ( folder_num > g_list_length (folders)-1 )
   {
     songs   = read_song_entries (rio_dev, folders, 0, card_number); /* use folder 0 by default */
     folder_num = 0;
   } else
     songs   = read_song_entries (rio_dev, folders, folder_num, card_number); 

   /* Remove the song entry from the list */
   if ( song_num > g_list_length (songs) -1)
     return -1;

   song = (song_entry *) g_list_nth_data (songs, song_num);
   if (song == NULL)
     return 0;
   printf ("Removing file <%s>...\n", song->name1);

   songs = g_list_remove (songs, (gpointer)song);

   /* Send remove command to the rio */
   send_command (rio_dev, 0x4c, ((folder_num << 8) | song_num), card_number);

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
   f_entry->fst_free_entry_off -= 0x800;

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

   return 0;
}

static char const shortopts[] = "F:xabwhv";
static struct option const longopts[] =
{
  {"folder", required_argument, NULL, 'F'},
  {"external", no_argument, NULL, 'x'},
  {"automatic", no_argument, NULL, 'a'},
  {"byname",no_argument, NULL, 'b'},
  {"wholefolder", no_argument, NULL, 'w'},
  {"version", no_argument, NULL, 'v'},
  {"help", no_argument, NULL, 'h'},
  {NULL, no_argument, NULL, 0}
};

static char const *const option_help[] =
{
"Input options:",
"",
"  -F x      --folder x         Delete song(s) from folder of index x",
"  -x        --external         Delete from external memory card",
"  -a        --automatic        Deletes without prompting",
"  -w        --wholefolder      Treats arguments as folder names/indices",
"                               Will delete entire folder at once",
"  -b        --byname           Use folder/song names instead of indicies",
"                               Names should be names as shown on the",
"				Rio500", 
"",
"Miscellaneous options:",
"",
"  -v        --version          Output version info.",
"  -h        --help             Output this help.",
"",
"Report bugs to <rio500-devel@lists.sourceforge.net>.",
0
};


/* Process switches and filenames.  */

void
get_some_switches (int argc, char *argv[], int *folder_num, int *automat, int *card_number)
{
    register int optc;
    char const * const *p;
    FILE *fptemp=0;

    if (optind == argc)
        return;
    while ((optc = getopt_long (argc, argv, shortopts, longopts, (int *) 0))
           != -1) {
         switch (optc) {
	    case 'x':
		*card_number=1;
		break;

            case 'F':
                folder_num_set = 1;
		folder_num_string=malloc(sizeof(optarg));
		folder_num_string=strcpy(folder_num_string,optarg);
                break;
	    case 'a':
		*automat = 1;
		break;
	    case 'b':
		name_flag = 1;
		break;
	    case 'w':
		whole_folder = 1;
		break;
            case 'v':
                printf("\nrio_del_song -- version %s\n",VERSION);
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

  /* if --byname not set then folder_num_string is an index */

  if (name_flag == 0 && folder_num_set == 1)
  {
 	*folder_num = atoi(folder_num_string);
  } 

}

gint g_alpha_sort (gconstpointer a, gconstpointer b)
{
  int x,a_len,b_len;
  
  a_len = strlen(a);   /* Want to sort in decending order */
  b_len = strlen(b);   /* so return value will be opposite */
  if (a_len != b_len)  /* of normal strcmp . . also 10 should */
  {
    if (a_len > b_len) /* come before 1 so check length too */
    {
      return (-1);
    }
    else
    {
      return (1);
    }
  }              
  x=strcmp(a,b);
  return (-x);
}

char * int_to_string(int x)
{
   /* can convert upto 999 to string */
   int num;
   char hundreds,tens,ones;

   num = x;
   hundreds = (char) num/100;
   num = num - (hundreds*100);
   tens = (char) num/10;
   ones = (char) num - (tens*10);
   numstring[0]=hundreds+48; /* convert hundreds digit to ascii */
   numstring[1]=tens+48; /* convert tens digit to ascii */
   numstring[2]=ones+48; /* convert ones digit to ascii */
   numstring[3]='\0'; /* terminate it */
   return numstring;

}

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
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <ctype.h>
#include <fcntl.h>
#include <errno.h>
#include "getopt.h"

#include "librio500.h"

#ifndef WITH_USBDEVFS
void add_folder (int rio_dev, char *name, char *font_name, int font_number, int card);
int  is_first_folder (int rio_dev, int card);
#else
void add_folder (struct usbdevice *rio_dev, char *name, char *font_name, int font_number, int card);
int  is_first_folder (struct usbdevice *rio_dev, int card);
#endif
void
get_some_switches (int argc, char *argv[], int *font_number, int *card_number);

char *font_name = DEFAULT_FONT_PATH;
char *temp_name;

void
usage (char *progname)
{
  printf ("\nusage: %s [OPTIONS] <folder_name1>  . . <folder_name256>\n", progname);
  printf ("\n [OPTIONS]  Try --help for more information");
  printf ("\n <folder_nameN> is the name of the folder we want to create\n");
  printf ("\n Currently, the maximum number of supported folders is 256\n");
  printf ("\n");
  return;
} 

int
main(int argc, char *argv[])
{
  int font_number=0,card_number=0,new_size;
  char *foldername;

#ifndef WITH_USBDEVFS
  int rio_dev;
#else
  struct usbdevice *rio_dev;

  rio_dev = malloc(sizeof(struct usbdevice));
#endif

#ifdef DEBUG
  #ifdef WORDS_BIGENDIAN
  printf ("WORDS_BIGENDIAN defined.\n");
  #endif
#endif

/* Check arguments */

  get_some_switches(argc,argv,&font_number,&card_number);

  if (strcmp(font_name,DEFAULT_FONT_PATH) == 0 )
  {
	new_size=strlen(DEFAULT_FONT_PATH)+strlen(DEFAULT_FON_FONT)+1;
	temp_name=(char *)malloc(new_size);
	strcpy(temp_name,font_name);
	strcat(temp_name,DEFAULT_FON_FONT);
	font_name=temp_name;
  }

  while (optind < argc)  /* loop through filenames and add */
  {
  foldername=malloc(33);
  foldername=safe_strcpy(foldername,argv[optind++],32);
    
#ifndef WITH_USBDEVFS
  rio_dev = open (DEFAULT_DEV_PATH, O_RDWR);
  if (rio_dev == -1)
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

   /* Create new folder */
   printf("Adding folder %s\n",foldername);
   add_folder (rio_dev, foldername, font_name, font_number, card_number);

   /* Close device */
   finish_communication (rio_dev);

#ifndef WITH_USBDEVFS
   close (rio_dev);
#endif
   free(foldername);
   } /* end of while loop */
   exit (0);
}

int
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

void
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
  int   first_folder_flag = 0;
  folders = NULL;

  /* Check if this is the first folder */ 

  if ( is_first_folder (rio_dev, card) )
  {
    folders = NULL; 
    last_folder = 0;
    first_folder_flag = 1;
  } else {
  send_command (rio_dev, 0x42, 0, 0);
  folders = read_folder_entries (rio_dev, card);
  }

  /* We can only have up to 256 folder entries: 0 - 255 */
  last_folder = g_list_length (folders);
  //printf("last folder = %d\n",last_folder); 
 
  /* check to see if the folder read failed */
  if (last_folder == 0 && first_folder_flag == 0) {
    printf("\nFailure reading folder list. Please try again.\n");
    printf("If failure persists, submit a bug report to\n");
    printf("rio500-devel@lists.sourceforge.net\n");
    return;
  }

  if ( last_folder > 255)
    return;

  /* Now create an new entry for the folder */
  entry = folder_entry_new (name, font_name, font_number);

  /* Write song and folder blocks back to rio */
  write_song_entries (rio_dev, last_folder, NULL, card);

  /* Afer a write it is a good idea to wait a bit */
  send_command (rio_dev, 0x42, 0, 0);
  send_command (rio_dev, 0x42, 0, 0);

  /* Now read the location of the new, empty song block */
  song_block_loc = send_command (rio_dev, 0x43, 0, 0);
  entry->offset = song_block_loc; 

  folders = g_list_append (folders, entry);

  /* Write folder list */
  write_folder_entries (rio_dev, folders, card);

  /* Wait a bit after the read */
  send_command (rio_dev, 0x42, 0, 0);
  send_command (rio_dev, 0x42, 0, 0);

  /* Tell rio where the root folder block is */
  folder_block_loc = send_command (rio_dev, 0x43, 0, 0);
  send_folder_location (rio_dev, folder_block_loc, last_folder, card);

  send_command (rio_dev, 0x58, 0x0, card);
  /* done */
  return;
}

static char const shortopts[] = "xf:n:hv";
static struct option const longopts[] =
{
  {"external", no_argument, NULL, 'x'},
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
"  -x        --external         Add folder to external memory card",
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

void
get_some_switches (int argc, char *argv[], int *font_number, int *card_number)
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
	    case 'x':
		*card_number=1;
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
                printf("\nrio_add_folder -- version %s\n",VERSION);
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




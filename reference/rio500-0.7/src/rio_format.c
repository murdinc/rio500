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
void get_some_switches (int argc, char *argv[], int * automat, int *format_internal, int *format_external);
void usage (char *progname);

void
usage (char *progname)
{
  printf ("\nusage: %s [OPTIONS] \n", progname);
  printf ("\n");
  printf ("\n Formats the Rio flash memory, erasing all songs and folders\n");
  printf("\n");
return;
}

int 
main(int argc, char *argv[])
{
  char answer[255];
  int automatic = 0;
  int format_internal = 0;
  int format_external = 0;
  int i;

#ifndef WITH_USBDEVFS
  int rio_dev;
#else
  struct usbdevice *rio_dev;

  rio_dev = malloc(sizeof(struct usbdevice));
#endif

  get_some_switches(argc,argv,&automatic,&format_internal,&format_external);

  /* if no switches set, default to formatting internal memory */

  if (!format_internal && !format_external) {
	format_internal=1;
  }
  if (!automatic)
    {
      /* Issue a warning */
      printf ("\n\n");
      printf ("---------------------------------------------------------\n");
      printf ("                  W A R N I N G\n");
      printf ("---------------------------------------------------------\n");
      printf ("\n");
      printf ("This command will erase ALL your folders and songs stored\n");
      printf ("on your RIO 500's ");

      if (format_internal)
        printf("internal ");
      if (format_internal && format_external)
        printf("and ");
      if (format_external)
        printf("external ");

      printf ("memory.\n\nAnswer with yes if you want to continue? ");
 
      scanf ("%s", answer);

      if (strcmp (answer, "yes") != 0) {
#ifdef WITH_USBDEVFS
        free(rio_dev);
#endif
	exit(0);
      }
    }

#ifndef WITH_USBDEVFS
  rio_dev = open (DEFAULT_DEV_PATH, O_RDWR);
  if (rio_dev == -1)
    {
      perror ("open failed");
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

   send_command (rio_dev, 0x42, 0x0, 0x0);
   send_command (rio_dev, 0x42, 0x0, 0x0);
   send_command (rio_dev, 0x42, 0x0, 0x0);

   if (format_external && query_card_count(rio_dev) > 1)
   {
     printf("Formatting external memory card...\n");
     format_flash (rio_dev, 1);
     printf("Done!\n");
   }
   else if (!format_internal)
     printf("Unable to find an external memory card to format.\n");

   if (format_internal)
   {
     printf("Formatting internal memory card...\n");
     format_flash (rio_dev, 0);
     printf("Done!\n");
   }

   /* Close device */
   finish_communication (rio_dev);

#ifndef WITH_USBDEVFS
   close (rio_dev);
#endif

   exit (0);
}

static char const shortopts[] = "ahvxi";

static struct option const longopts[] =
{
  {"automatic", no_argument, NULL, 'a'},
  {"external", no_argument, NULL, 'x'},
  {"internal", no_argument, NULL, 'i'},
  {"both", no_argument, NULL, 'b'},
  {"version", no_argument, NULL, 'v'},
  {"help", no_argument, NULL, 'h'},
  {NULL, no_argument, NULL, 0}
};

static char const *const option_help[] =
{
"Input options:",
"",
"  -a        --automatic        Format rio without prompting",
"  -x        --external         Format only the external smartmedia card",
"  -i        --internal         Format only the internal memory",
"  -b        --both             Format internal memory and external smartmedia card",
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
get_some_switches (int argc, char *argv[], int * automat, int *format_internal, int *format_external)
{
    register int optc;
    char const * const *p;

    if (optind == argc)
        return;
    while ((optc = getopt_long (argc, argv, shortopts, longopts, (int *) 0))
           != -1) {
         switch (optc) {
            case 'a':
                *automat = 1;
                break;
            case 'v':
                printf("\nrio_format -- version %s\n",VERSION);
                exit(0);
                break;
	    case 'i':
		*format_internal=1;
		break;
	    case 'x':
		*format_external=1;
		break;
            case 'b':
		*format_internal=1;
		*format_external=1;
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



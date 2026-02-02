#include <glib.h>
#include <librio500.h> 

#include "config.h"


#ifndef RIO500_API_H
#define RIO500_API_H

/* rio500_api error codes  */

#define PC_MEMERR 0 /* Memory alloc error on PC end */
#define RIO_SUCCESS 1 /* Operation successful */
#define RIO_NOMEM -1 /* Not enough memory in rio for transfer */
#define RIO_NODIR -2/* Directory not found */
#define RIO_INITCOMM -3 /* Error initiating comm with rio */
#define RIO_ENDCOMM -4 /* Error ending comm with rio */
#define RIO_FORMAT -5 /* Error formatting rio */

typedef void (*RioStatusFunc) (int operation, char *msg, int percent);

typedef struct
{
#ifndef WITH_USBDEVFS
  int            rio_dev;
#else
  struct usbdevice *rio_dev;
#endif
  char           *font;
  int            font_num;
  RioStatusFunc  stat_func;
  char		 error_code;
  int		 card;
} Rio500;

typedef struct
{
  char *name;
  GList *songs;
  int    folder_num;
} RioFolderEntry;

typedef struct
{
  RioFolderEntry *parent;
  char           *name;
  unsigned long   size;
  int             song_num;
} RioSongEntry;

Rio500         *rio_new ();

GList          *rio_get_content (Rio500 *);

int             rio_add_folder (Rio500 *, char *folder_name);
int             rio_rename_folder (Rio500 *, int folder_num, char *folder_name);
int             rio_rename_song (Rio500 *, int fn, int sn, char *song_name);
int             rio_add_song (Rio500 *, int folder_num, char *file);
int		rio_add_directory(Rio500 *, char *dir_name, int folder_num);
int             rio_del_song (Rio500 *, int folder_num, int song_num);
int             rio_del_folder (Rio500 *, int folder_num);
int             rio_format (Rio500 *);
unsigned long   rio_memory_left (Rio500 *);

int             rio_set_report_func (Rio500 *, RioStatusFunc report_func);
void            rio_destroy_content (GList *content);
void            rio_delete (Rio500 *);
int		rio_set_font(Rio500 *, char *font_name, int font_number);
int		rio_set_card(Rio500 *, int card);
unsigned long   rio_get_mem_total (Rio500 *);

#endif /* RIO500_API_H */

CFLAGS1=@CFLAGS@

DEFAULT_FONT_PATH = @fontpath@
DEFAULT_FON_FONT = @fonfont@
DEFAULT_PSF_FONT = @psffont@
DEVICE_PATH = @devicepath@
DEVICE_ENTRY = @deviceentry@
EFENCE_LIB = @efence_lib@

DEFAULT_DEV_PATH = $(DEVICE_PATH)/$(DEVICE_ENTRY)

FLAGS1 = $(CFLAGS1) -Iinclude -I$(top_srcdir)/include @GLIB_CFLAGS@ $(CPPFLAGS)
FLAGS2 = -DDEFAULT_PSF_FONT=\"$(DEFAULT_PSF_FONT)\" -DDEFAULT_FONT_PATH=\"$(DEFAULT_FONT_PATH)\"
FLAGS3 = -DDEFAULT_FON_FONT=\"$(DEFAULT_FON_FONT)\" -DDEFAULT_DEV_PATH=\"$(DEFAULT_DEV_PATH)\"

CFLAGS = $(FLAGS1) $(FLAGS2) $(FLAGS3)

LDADD = -L../lib @GLIB_LIBS@ -lrio500 $(EFENCE_LIB)

GETOPT_SOURCES = getopt.c getopt1.c

bin_PROGRAMS = rio_format rio_add_song rio_del_song \
		rio_add_folder rio_stat rio_font_info \
		rio_get_song
EXTRA_DIST = 
rio_format_SOURCES = rio_format.c $(GETOPT_SOURCES)
rio_add_song_SOURCES = rio_add_song.c $(GETOPT_SOURCES)
rio_del_song_SOURCES = rio_del_song.c $(GETOPT_SOURCES)
rio_get_song_SOURCES = rio_get_song.c $(GETOPT_SOURCES)
rio_add_folder_SOURCES = rio_add_folder.c $(GETOPT_SOURCES)
rio_stat_SOURCES = rio_stat.c $(GETOPT_SOURCES)
rio_font_info_SOURCES = rio_font_info.c $(GETOPT_SOURCES)
INCLUDES = -I. @GLIB_CFLAGS@

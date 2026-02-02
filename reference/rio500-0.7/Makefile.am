SUBDIRS = lib src fonts scripts include
EXTRA_DIST = include/config.h include/config.h.in include/stamp-h include/stamp-h.in scripts/rio_add_directory

DEFAULT_FONT_PATH = @fontpath@
DEFAULT_FON_FONT = @fonfont@
DEFAULT_PSF_FONT = @psffont@
EFENCE_LIB = @efence_lib@

FLAGS1 = $(CFLAGS) -Iinclude -I$(srcdir)/include @GLIB_CFLAGS@ $(CPPFLAGS)
FLAGS2 = -DDEFAULT_PSF_FONT=\"$(DEFAULT_PSF_FONT)\" -DDEFAULT_FONT_PATH=\"$(DEFAULT_FONT_PATH)\"
FLAGS3 = -DDEFAULT_FON_FONT=\"$(DEFAULT_FON_FONT)\"

FLAGS = $(FLAGS1) $(FLAGS2) $(FLAGS3)

LIBS = $(LIBS1) -lrio500 $(EFENCE_LIB)
LDFLAGS = $(LDFLAGS1) -L$(srcdir)/lib

CFLAGS1=@CFLAGS@

DEFAULT_FONT_PATH = @fontpath@
DEFAULT_FON_FONT = @fonfont@
DEFAULT_PSF_FONT = @psffont@
DEVICE_PATH = @devicepath@
DEVICE_ENTRY = @deviceentry@

DEFAULT_DEV_PATH = $(DEVICE_PATH)/$(DEVICE_ENTRY)

FLAGS1 = $(CFLAGS1) -I../include -I$(top_srcdir)/include -I/usr/src/linux/drivers/usb @GLIB_CFLAGS@ $(CPPFLAGS)
FLAGS2 = -DDEFAULT_PSF_FONT=\"$(DEFAULT_PSF_FONT)\" -DDEFAULT_FONT_PATH=\"$(DEFAULT_FONT_PATH)\"
FLAGS3 = -DDEFAULT_FON_FONT=\"$(DEFAULT_FON_FONT)\" -DDEFAULT_DEV_PATH=\"$(DEFAULT_DEV_PATH)\"

CFLAGS = $(FLAGS1) $(FLAGS2) $(FLAGS3)

lib_LIBRARIES = librio500_api.a librio500.a
librio500_a_SOURCES = libfon.c libpsf.c usbdrvlinux.c
EXTRA_librio500_a_SOURCES = librio500.c librio500-usbdevfs.c
librio500_api_a_SOURCES = librio500_api.c usbdrvlinux.c
librio500_a_LIBADD = @RIO_LIB_OBJ@
librio500_a_DEPENDENCIES = @RIO_LIB_OBJ@

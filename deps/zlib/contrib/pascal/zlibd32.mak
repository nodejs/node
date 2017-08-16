# Makefile for zlib
# For use with Delphi and C++ Builder under Win32
# Updated for zlib 1.2.x by Cosmin Truta

# ------------ Borland C++ ------------

# This project uses the Delphi (fastcall/register) calling convention:
LOC = -DZEXPORT=__fastcall -DZEXPORTVA=__cdecl

CC = bcc32
LD = bcc32
AR = tlib
# do not use "-pr" in CFLAGS
CFLAGS = -a -d -k- -O2 $(LOC)
LDFLAGS =


# variables
ZLIB_LIB = zlib.lib

OBJ1 = adler32.obj compress.obj crc32.obj deflate.obj gzclose.obj gzlib.obj gzread.obj
OBJ2 = gzwrite.obj infback.obj inffast.obj inflate.obj inftrees.obj trees.obj uncompr.obj zutil.obj
OBJP1 = +adler32.obj+compress.obj+crc32.obj+deflate.obj+gzclose.obj+gzlib.obj+gzread.obj
OBJP2 = +gzwrite.obj+infback.obj+inffast.obj+inflate.obj+inftrees.obj+trees.obj+uncompr.obj+zutil.obj


# targets
all: $(ZLIB_LIB) example.exe minigzip.exe

.c.obj:
	$(CC) -c $(CFLAGS) $*.c

adler32.obj: adler32.c zlib.h zconf.h

compress.obj: compress.c zlib.h zconf.h

crc32.obj: crc32.c zlib.h zconf.h crc32.h

deflate.obj: deflate.c deflate.h zutil.h zlib.h zconf.h

gzclose.obj: gzclose.c zlib.h zconf.h gzguts.h

gzlib.obj: gzlib.c zlib.h zconf.h gzguts.h

gzread.obj: gzread.c zlib.h zconf.h gzguts.h

gzwrite.obj: gzwrite.c zlib.h zconf.h gzguts.h

infback.obj: infback.c zutil.h zlib.h zconf.h inftrees.h inflate.h \
 inffast.h inffixed.h

inffast.obj: inffast.c zutil.h zlib.h zconf.h inftrees.h inflate.h \
 inffast.h

inflate.obj: inflate.c zutil.h zlib.h zconf.h inftrees.h inflate.h \
 inffast.h inffixed.h

inftrees.obj: inftrees.c zutil.h zlib.h zconf.h inftrees.h

trees.obj: trees.c zutil.h zlib.h zconf.h deflate.h trees.h

uncompr.obj: uncompr.c zlib.h zconf.h

zutil.obj: zutil.c zutil.h zlib.h zconf.h

example.obj: test/example.c zlib.h zconf.h

minigzip.obj: test/minigzip.c zlib.h zconf.h


# For the sake of the old Borland make,
# the command line is cut to fit in the MS-DOS 128 byte limit:
$(ZLIB_LIB): $(OBJ1) $(OBJ2)
	-del $(ZLIB_LIB)
	$(AR) $(ZLIB_LIB) $(OBJP1)
	$(AR) $(ZLIB_LIB) $(OBJP2)


# testing
test: example.exe minigzip.exe
	example
	echo hello world | minigzip | minigzip -d

example.exe: example.obj $(ZLIB_LIB)
	$(LD) $(LDFLAGS) example.obj $(ZLIB_LIB)

minigzip.exe: minigzip.obj $(ZLIB_LIB)
	$(LD) $(LDFLAGS) minigzip.obj $(ZLIB_LIB)


# cleanup
clean:
	-del *.obj
	-del *.exe
	-del *.lib
	-del *.tds
	-del zlib.bak
	-del foo.gz


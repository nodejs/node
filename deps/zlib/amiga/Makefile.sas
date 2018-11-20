# SMakefile for zlib
# Modified from the standard UNIX Makefile Copyright Jean-loup Gailly
# Osma Ahvenlampi <Osma.Ahvenlampi@hut.fi>
# Amiga, SAS/C 6.56 & Smake

CC=sc
CFLAGS=OPT
#CFLAGS=OPT CPU=68030
#CFLAGS=DEBUG=LINE
LDFLAGS=LIB z.lib

SCOPTIONS=OPTSCHED OPTINLINE OPTALIAS OPTTIME OPTINLOCAL STRMERGE \
       NOICONS PARMS=BOTH NOSTACKCHECK UTILLIB NOVERSION ERRORREXX \
       DEF=POSTINC

OBJS = adler32.o compress.o crc32.o gzclose.o gzlib.o gzread.o gzwrite.o \
       uncompr.o deflate.o trees.o zutil.o inflate.o infback.o inftrees.o inffast.o

TEST_OBJS = example.o minigzip.o

all: SCOPTIONS example minigzip

check: test
test: all
	example
	echo hello world | minigzip | minigzip -d

install: z.lib
	copy clone zlib.h zconf.h INCLUDE:
	copy clone z.lib LIB:

z.lib: $(OBJS)
	oml z.lib r $(OBJS)

example: example.o z.lib
	$(CC) $(CFLAGS) LINK TO $@ example.o $(LDFLAGS)

minigzip: minigzip.o z.lib
	$(CC) $(CFLAGS) LINK TO $@ minigzip.o $(LDFLAGS)

mostlyclean: clean
clean:
	-delete force quiet example minigzip *.o z.lib foo.gz *.lnk SCOPTIONS

SCOPTIONS: Makefile.sas
	copy to $@ <from <
$(SCOPTIONS)
<

# DO NOT DELETE THIS LINE -- make depend depends on it.

adler32.o: zlib.h zconf.h
compress.o: zlib.h zconf.h
crc32.o: crc32.h zlib.h zconf.h
deflate.o: deflate.h zutil.h zlib.h zconf.h
example.o: zlib.h zconf.h
gzclose.o: zlib.h zconf.h gzguts.h
gzlib.o: zlib.h zconf.h gzguts.h
gzread.o: zlib.h zconf.h gzguts.h
gzwrite.o: zlib.h zconf.h gzguts.h
inffast.o: zutil.h zlib.h zconf.h inftrees.h inflate.h inffast.h
inflate.o: zutil.h zlib.h zconf.h inftrees.h inflate.h inffast.h
infback.o: zutil.h zlib.h zconf.h inftrees.h inflate.h inffast.h
inftrees.o: zutil.h zlib.h zconf.h inftrees.h
minigzip.o: zlib.h zconf.h
trees.o: deflate.h zutil.h zlib.h zconf.h trees.h
uncompr.o: zlib.h zconf.h
zutil.o: zutil.h zlib.h zconf.h

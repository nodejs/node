# descrip.mms: MMS description file for building zlib on VMS
# written by Martin P.J. Zinser <m.zinser@gsi.de>

cc_defs =
c_deb =

.ifdef __DECC__
pref = /prefix=all
.endif

OBJS = adler32.obj, compress.obj, crc32.obj, gzio.obj, uncompr.obj,\
       deflate.obj, trees.obj, zutil.obj, inflate.obj, infblock.obj,\
       inftrees.obj, infcodes.obj, infutil.obj, inffast.obj

CFLAGS= $(C_DEB) $(CC_DEFS) $(PREF)

all : example.exe minigzip.exe
        @ write sys$output " Example applications available"
libz.olb : libz.olb($(OBJS))
	@ write sys$output " libz available"

example.exe : example.obj libz.olb
              link example,libz.olb/lib

minigzip.exe : minigzip.obj libz.olb
              link minigzip,libz.olb/lib,x11vms:xvmsutils.olb/lib

clean :
	delete *.obj;*,libz.olb;*


# Other dependencies.
adler32.obj : zutil.h zlib.h zconf.h
compress.obj : zlib.h zconf.h
crc32.obj : zutil.h zlib.h zconf.h
deflate.obj : deflate.h zutil.h zlib.h zconf.h
example.obj : zlib.h zconf.h
gzio.obj : zutil.h zlib.h zconf.h
infblock.obj : zutil.h zlib.h zconf.h infblock.h inftrees.h infcodes.h infutil.h
infcodes.obj : zutil.h zlib.h zconf.h inftrees.h infutil.h infcodes.h inffast.h
inffast.obj : zutil.h zlib.h zconf.h inftrees.h infutil.h inffast.h
inflate.obj : zutil.h zlib.h zconf.h infblock.h
inftrees.obj : zutil.h zlib.h zconf.h inftrees.h
infutil.obj : zutil.h zlib.h zconf.h inftrees.h infutil.h
minigzip.obj : zlib.h zconf.h
trees.obj : deflate.h zutil.h zlib.h zconf.h
uncompr.obj : zlib.h zconf.h
zutil.obj : zutil.h zlib.h zconf.h

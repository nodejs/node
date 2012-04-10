#!/bin/sh

#
# This is a ugly script use, in conjuction with editing the 'b'
# configuration in the $(TOP)/Configure script which will
# output when finished a file called speed.log which is the
# timings of SSLeay with various options turned on or off.
#
# from the $(TOP) directory
# Edit Configure, modifying things to do with the b/bl-4c-2c etc
# configurations.
#

make clean
perl Configure b
make
apps/ssleay version -v -b -f >speed.1
apps/ssleay speed >speed.1l

perl Configure bl-4c-2c
/bin/rm -f crypto/rc4/*.o crypto/bn/bn*.o crypto/md2/md2_dgst.o
make
apps/ssleay speed rc4 rsa md2 >speed.2l

perl Configure bl-4c-ri
/bin/rm -f crypto/rc4/rc4*.o
make
apps/ssleay speed rc4 >speed.3l

perl Configure b2-is-ri-dp
/bin/rm -f crypto/idea/i_*.o crypto/rc4/*.o crypto/des/ecb_enc.o crypto/bn/bn*.o
apps/ssleay speed rsa rc4 idea des >speed.4l

cat speed.1 >speed.log
cat speed.1l >>speed.log
perl util/sp-diff.pl speed.1l speed.2l >>speed.log
perl util/sp-diff.pl speed.1l speed.3l >>speed.log
perl util/sp-diff.pl speed.1l speed.4l >>speed.log


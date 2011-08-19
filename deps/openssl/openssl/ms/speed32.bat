set makefile=ms\nt.mak

perl Configure b
del tmp\*.obj
nmake -f %makefile%
nmake -f %makefile%
nmake -f %makefile%
out\ssleay version -v -b -f >speed.1
out\ssleay speed >speed.1l

perl Configure bl-4c-2c
del tmp\rc4*.obj tmp\bn*.obj tmp\md2_dgst.obj
nmake -f %makefile%
nmake -f %makefile%
nmake -f %makefile%
out\ssleay speed rc4 rsa md2 >speed.2l

perl Configure bl-4c-ri
del tmp\rc4*.obj
nmake -f %makefile%
nmake -f %makefile%
nmake -f %makefile%
out\ssleay speed rc4 >speed.3l

perl Configure b2-is-ri-dp
del tmp\i_*.obj tmp\rc4*.obj tmp\ecb_enc.obj tmp\bn*.obj
nmake -f %makefile%
nmake -f %makefile%
nmake -f %makefile%
out\ssleay speed rsa rc4 idea des >speed.4l

type speed.1 >speed.log
type speed.1l >>speed.log
perl util\sp-diff.pl speed.1l speed.2l >>speed.log
perl util\sp-diff.pl speed.1l speed.3l >>speed.log
perl util\sp-diff.pl speed.1l speed.4l >>speed.log


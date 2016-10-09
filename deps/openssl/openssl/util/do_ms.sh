#!/bin/sh
#
# generate the Microsoft makefiles and .def files
#

PATH=util:../util:$PATH

# perl util/mk1mf.pl no-sock VC-MSDOS >ms/msdos.mak
# perl util/mk1mf.pl VC-W31-32 >ms/w31.mak
perl util/mk1mf.pl dll VC-WIN16 >ms/w31dll.mak
# perl util/mk1mf.pl VC-WIN32 >ms/nt.mak
perl util/mk1mf.pl dll VC-WIN32 >ms/ntdll.mak
perl util/mk1mf.pl Mingw32 >ms/mingw32.mak
perl util/mk1mf.pl Mingw32-files >ms/mingw32f.mak

perl util/mkdef.pl 16 libeay > ms/libeay16.def
perl util/mkdef.pl 32 libeay > ms/libeay32.def
perl util/mkdef.pl 16 ssleay > ms/ssleay16.def
perl util/mkdef.pl 32 ssleay > ms/ssleay32.def

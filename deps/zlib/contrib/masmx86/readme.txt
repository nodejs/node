
Summary
-------
This directory contains ASM implementations of the functions
longest_match() and inflate_fast().


Use instructions
----------------
Assemble using MASM, and copy the object files into the zlib source
directory, then run the appropriate makefile, as suggested below.  You can
donwload MASM from here:

    http://www.microsoft.com/downloads/details.aspx?displaylang=en&FamilyID=7a1c9da0-0510-44a2-b042-7ef370530c64

You can also get objects files here:

    http://www.winimage.com/zLibDll/zlib124_masm_obj.zip

Build instructions
------------------
* With Microsoft C and MASM:
nmake -f win32/Makefile.msc LOC="-DASMV -DASMINF" OBJA="match686.obj inffas32.obj"

* With Borland C and TASM:
make -f win32/Makefile.bor LOCAL_ZLIB="-DASMV -DASMINF" OBJA="match686.obj inffas32.obj" OBJPA="+match686c.obj+match686.obj+inffas32.obj"


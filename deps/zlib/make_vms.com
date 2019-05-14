$! make libz under VMS written by
$! Martin P.J. Zinser
$!
$! In case of problems with the install you might contact me at
$! zinser@zinser.no-ip.info(preferred) or
$! martin.zinser@eurexchange.com (work)
$!
$! Make procedure history for Zlib
$!
$!------------------------------------------------------------------------------
$! Version history
$! 0.01 20060120 First version to receive a number
$! 0.02 20061008 Adapt to new Makefile.in
$! 0.03 20091224 Add support for large file check
$! 0.04 20100110 Add new gzclose, gzlib, gzread, gzwrite
$! 0.05 20100221 Exchange zlibdefs.h by zconf.h.in
$! 0.06 20120111 Fix missing amiss_err, update zconf_h.in, fix new exmples
$!               subdir path, update module search in makefile.in
$! 0.07 20120115 Triggered by work done by Alexey Chupahin completly redesigned
$!               shared image creation
$! 0.08 20120219 Make it work on VAX again, pre-load missing symbols to shared
$!               image
$! 0.09 20120305 SMS.  P1 sets builder ("MMK", "MMS", " " (built-in)).
$!               "" -> automatic, preference: MMK, MMS, built-in.
$!
$ on error then goto err_exit
$!
$ true  = 1
$ false = 0
$ tmpnam = "temp_" + f$getjpi("","pid")
$ tt = tmpnam + ".txt"
$ tc = tmpnam + ".c"
$ th = tmpnam + ".h"
$ define/nolog tconfig 'th'
$ its_decc = false
$ its_vaxc = false
$ its_gnuc = false
$ s_case   = False
$!
$! Setup variables holding "config" information
$!
$ Make    = "''p1'"
$ name     = "Zlib"
$ version  = "?.?.?"
$ v_string = "ZLIB_VERSION"
$ v_file   = "zlib.h"
$ ccopt   = "/include = []"
$ lopts   = ""
$ dnsrl   = ""
$ aconf_in_file = "zconf.h.in#zconf.h_in#zconf_h.in"
$ conf_check_string = ""
$ linkonly = false
$ optfile  = name + ".opt"
$ mapfile  = name + ".map"
$ libdefs  = ""
$ vax      = f$getsyi("HW_MODEL").lt.1024
$ axp      = f$getsyi("HW_MODEL").ge.1024 .and. f$getsyi("HW_MODEL").lt.4096
$ ia64     = f$getsyi("HW_MODEL").ge.4096
$!
$! 2012-03-05 SMS.
$! Why is this needed?  And if it is needed, why not simply ".not. vax"?
$!
$!!! if axp .or. ia64 then  set proc/parse=extended
$!
$ whoami = f$parse(f$environment("Procedure"),,,,"NO_CONCEAL")
$ mydef  = F$parse(whoami,,,"DEVICE")
$ mydir  = f$parse(whoami,,,"DIRECTORY") - "]["
$ myproc = f$parse(whoami,,,"Name") + f$parse(whoami,,,"type")
$!
$! Check for MMK/MMS
$!
$ if (Make .eqs. "")
$ then
$   If F$Search ("Sys$System:MMS.EXE") .nes. "" Then Make = "MMS"
$   If F$Type (MMK) .eqs. "STRING" Then Make = "MMK"
$ else
$   Make = f$edit( Make, "trim")
$ endif
$!
$ gosub find_version
$!
$  open/write topt tmp.opt
$  open/write optf 'optfile'
$!
$ gosub check_opts
$!
$! Look for the compiler used
$!
$ gosub check_compiler
$ close topt
$ close optf
$!
$ if its_decc
$ then
$   ccopt = "/prefix=all" + ccopt
$   if f$trnlnm("SYS") .eqs. ""
$   then
$     if axp
$     then
$       define sys sys$library:
$     else
$       ccopt = "/decc" + ccopt
$       define sys decc$library_include:
$     endif
$   endif
$!
$! 2012-03-05 SMS.
$! Why /NAMES = AS_IS?  Why not simply ".not. vax"?  And why not on VAX?
$!
$   if axp .or. ia64
$   then
$       ccopt = ccopt + "/name=as_is/opt=(inline=speed)"
$       s_case = true
$   endif
$ endif
$ if its_vaxc .or. its_gnuc
$ then
$    if f$trnlnm("SYS").eqs."" then define sys sys$library:
$ endif
$!
$! Build a fake configure input header
$!
$ open/write conf_hin config.hin
$ write conf_hin "#undef _LARGEFILE64_SOURCE"
$ close conf_hin
$!
$!
$ i = 0
$FIND_ACONF:
$ fname = f$element(i,"#",aconf_in_file)
$ if fname .eqs. "#" then goto AMISS_ERR
$ if f$search(fname) .eqs. ""
$ then
$   i = i + 1
$   goto find_aconf
$ endif
$ open/read/err=aconf_err aconf_in 'fname'
$ open/write aconf zconf.h
$ACONF_LOOP:
$ read/end_of_file=aconf_exit aconf_in line
$ work = f$edit(line, "compress,trim")
$ if f$extract(0,6,work) .nes. "#undef"
$ then
$   if f$extract(0,12,work) .nes. "#cmakedefine"
$   then
$       write aconf line
$   endif
$ else
$   cdef = f$element(1," ",work)
$   gosub check_config
$ endif
$ goto aconf_loop
$ACONF_EXIT:
$ write aconf ""
$ write aconf "/* VMS specifics added by make_vms.com: */"
$ write aconf "#define VMS 1"
$ write aconf "#include <unistd.h>"
$ write aconf "#include <unixio.h>"
$ write aconf "#ifdef _LARGEFILE"
$ write aconf "# define off64_t __off64_t"
$ write aconf "# define fopen64 fopen"
$ write aconf "# define fseeko64 fseeko"
$ write aconf "# define lseek64 lseek"
$ write aconf "# define ftello64 ftell"
$ write aconf "#endif"
$ write aconf "#if !defined( __VAX) && (__CRTL_VER >= 70312000)"
$ write aconf "# define HAVE_VSNPRINTF"
$ write aconf "#endif"
$ close aconf_in
$ close aconf
$ if f$search("''th'") .nes. "" then delete 'th';*
$! Build the thing plain or with mms
$!
$ write sys$output "Compiling Zlib sources ..."
$ if make.eqs.""
$ then
$   if (f$search( "example.obj;*") .nes. "") then delete example.obj;*
$   if (f$search( "minigzip.obj;*") .nes. "") then delete minigzip.obj;*
$   CALL MAKE adler32.OBJ "CC ''CCOPT' adler32" -
                adler32.c zlib.h zconf.h
$   CALL MAKE compress.OBJ "CC ''CCOPT' compress" -
                compress.c zlib.h zconf.h
$   CALL MAKE crc32.OBJ "CC ''CCOPT' crc32" -
                crc32.c zlib.h zconf.h
$   CALL MAKE deflate.OBJ "CC ''CCOPT' deflate" -
                deflate.c deflate.h zutil.h zlib.h zconf.h
$   CALL MAKE gzclose.OBJ "CC ''CCOPT' gzclose" -
                gzclose.c zutil.h zlib.h zconf.h
$   CALL MAKE gzlib.OBJ "CC ''CCOPT' gzlib" -
                gzlib.c zutil.h zlib.h zconf.h
$   CALL MAKE gzread.OBJ "CC ''CCOPT' gzread" -
                gzread.c zutil.h zlib.h zconf.h
$   CALL MAKE gzwrite.OBJ "CC ''CCOPT' gzwrite" -
                gzwrite.c zutil.h zlib.h zconf.h
$   CALL MAKE infback.OBJ "CC ''CCOPT' infback" -
                infback.c zutil.h inftrees.h inflate.h inffast.h inffixed.h
$   CALL MAKE inffast.OBJ "CC ''CCOPT' inffast" -
                inffast.c zutil.h zlib.h zconf.h inffast.h
$   CALL MAKE inflate.OBJ "CC ''CCOPT' inflate" -
                inflate.c zutil.h zlib.h zconf.h infblock.h
$   CALL MAKE inftrees.OBJ "CC ''CCOPT' inftrees" -
                inftrees.c zutil.h zlib.h zconf.h inftrees.h
$   CALL MAKE trees.OBJ "CC ''CCOPT' trees" -
                trees.c deflate.h zutil.h zlib.h zconf.h
$   CALL MAKE uncompr.OBJ "CC ''CCOPT' uncompr" -
                uncompr.c zlib.h zconf.h
$   CALL MAKE zutil.OBJ "CC ''CCOPT' zutil" -
                zutil.c zutil.h zlib.h zconf.h
$   write sys$output "Building Zlib ..."
$   CALL MAKE libz.OLB "lib/crea libz.olb *.obj" *.OBJ
$   write sys$output "Building example..."
$   CALL MAKE example.OBJ "CC ''CCOPT' [.test]example" -
                [.test]example.c zlib.h zconf.h
$   call make example.exe "LINK example,libz.olb/lib" example.obj libz.olb
$   write sys$output "Building minigzip..."
$   CALL MAKE minigzip.OBJ "CC ''CCOPT' [.test]minigzip" -
              [.test]minigzip.c zlib.h zconf.h
$   call make minigzip.exe -
              "LINK minigzip,libz.olb/lib" -
              minigzip.obj libz.olb
$ else
$   gosub crea_mms
$   write sys$output "Make ''name' ''version' with ''Make' "
$   'make'
$ endif
$!
$! Create shareable image
$!
$ gosub crea_olist
$ write sys$output "Creating libzshr.exe"
$ call map_2_shopt 'mapfile' 'optfile'
$ LINK_'lopts'/SHARE=libzshr.exe modules.opt/opt,'optfile'/opt
$ write sys$output "Zlib build completed"
$ delete/nolog tmp.opt;*
$ exit
$AMISS_ERR:
$ write sys$output "No source for config.hin found."
$ write sys$output "Tried any of ''aconf_in_file'"
$ goto err_exit
$CC_ERR:
$ write sys$output "C compiler required to build ''name'"
$ goto err_exit
$ERR_EXIT:
$ set message/facil/ident/sever/text
$ close/nolog optf
$ close/nolog topt
$ close/nolog aconf_in
$ close/nolog aconf
$ close/nolog out
$ close/nolog min
$ close/nolog mod
$ close/nolog h_in
$ write sys$output "Exiting..."
$ exit 2
$!
$!
$MAKE: SUBROUTINE   !SUBROUTINE TO CHECK DEPENDENCIES
$ V = 'F$Verify(0)
$! P1 = What we are trying to make
$! P2 = Command to make it
$! P3 - P8  What it depends on
$
$ If F$Search(P1) .Eqs. "" Then Goto Makeit
$ Time = F$CvTime(F$File(P1,"RDT"))
$arg=3
$Loop:
$       Argument = P'arg
$       If Argument .Eqs. "" Then Goto Exit
$       El=0
$Loop2:
$       File = F$Element(El," ",Argument)
$       If File .Eqs. " " Then Goto Endl
$       AFile = ""
$Loop3:
$       OFile = AFile
$       AFile = F$Search(File)
$       If AFile .Eqs. "" .Or. AFile .Eqs. OFile Then Goto NextEl
$       If F$CvTime(F$File(AFile,"RDT")) .Ges. Time Then Goto Makeit
$       Goto Loop3
$NextEL:
$       El = El + 1
$       Goto Loop2
$EndL:
$ arg=arg+1
$ If arg .Le. 8 Then Goto Loop
$ Goto Exit
$
$Makeit:
$ VV=F$VERIFY(0)
$ write sys$output P2
$ 'P2
$ VV='F$Verify(VV)
$Exit:
$ If V Then Set Verify
$ENDSUBROUTINE
$!------------------------------------------------------------------------------
$!
$! Check command line options and set symbols accordingly
$!
$!------------------------------------------------------------------------------
$! Version history
$! 0.01 20041206 First version to receive a number
$! 0.02 20060126 Add new "HELP" target
$ CHECK_OPTS:
$ i = 1
$ OPT_LOOP:
$ if i .lt. 9
$ then
$   cparm = f$edit(p'i',"upcase")
$!
$! Check if parameter actually contains something
$!
$   if f$edit(cparm,"trim") .nes. ""
$   then
$     if cparm .eqs. "DEBUG"
$     then
$       ccopt = ccopt + "/noopt/deb"
$       lopts = lopts + "/deb"
$     endif
$     if f$locate("CCOPT=",cparm) .lt. f$length(cparm)
$     then
$       start = f$locate("=",cparm) + 1
$       len   = f$length(cparm) - start
$       ccopt = ccopt + f$extract(start,len,cparm)
$       if f$locate("AS_IS",f$edit(ccopt,"UPCASE")) .lt. f$length(ccopt) -
          then s_case = true
$     endif
$     if cparm .eqs. "LINK" then linkonly = true
$     if f$locate("LOPTS=",cparm) .lt. f$length(cparm)
$     then
$       start = f$locate("=",cparm) + 1
$       len   = f$length(cparm) - start
$       lopts = lopts + f$extract(start,len,cparm)
$     endif
$     if f$locate("CC=",cparm) .lt. f$length(cparm)
$     then
$       start  = f$locate("=",cparm) + 1
$       len    = f$length(cparm) - start
$       cc_com = f$extract(start,len,cparm)
        if (cc_com .nes. "DECC") .and. -
           (cc_com .nes. "VAXC") .and. -
           (cc_com .nes. "GNUC")
$       then
$         write sys$output "Unsupported compiler choice ''cc_com' ignored"
$         write sys$output "Use DECC, VAXC, or GNUC instead"
$       else
$         if cc_com .eqs. "DECC" then its_decc = true
$         if cc_com .eqs. "VAXC" then its_vaxc = true
$         if cc_com .eqs. "GNUC" then its_gnuc = true
$       endif
$     endif
$     if f$locate("MAKE=",cparm) .lt. f$length(cparm)
$     then
$       start  = f$locate("=",cparm) + 1
$       len    = f$length(cparm) - start
$       mmks = f$extract(start,len,cparm)
$       if (mmks .eqs. "MMK") .or. (mmks .eqs. "MMS")
$       then
$         make = mmks
$       else
$         write sys$output "Unsupported make choice ''mmks' ignored"
$         write sys$output "Use MMK or MMS instead"
$       endif
$     endif
$     if cparm .eqs. "HELP" then gosub bhelp
$   endif
$   i = i + 1
$   goto opt_loop
$ endif
$ return
$!------------------------------------------------------------------------------
$!
$! Look for the compiler used
$!
$! Version history
$! 0.01 20040223 First version to receive a number
$! 0.02 20040229 Save/set value of decc$no_rooted_search_lists
$! 0.03 20060202 Extend handling of GNU C
$! 0.04 20090402 Compaq -> hp
$CHECK_COMPILER:
$ if (.not. (its_decc .or. its_vaxc .or. its_gnuc))
$ then
$   its_decc = (f$search("SYS$SYSTEM:DECC$COMPILER.EXE") .nes. "")
$   its_vaxc = .not. its_decc .and. (F$Search("SYS$System:VAXC.Exe") .nes. "")
$   its_gnuc = .not. (its_decc .or. its_vaxc) .and. (f$trnlnm("gnu_cc") .nes. "")
$ endif
$!
$! Exit if no compiler available
$!
$ if (.not. (its_decc .or. its_vaxc .or. its_gnuc))
$ then goto CC_ERR
$ else
$   if its_decc
$   then
$     write sys$output "CC compiler check ... hp C"
$     if f$trnlnm("decc$no_rooted_search_lists") .nes. ""
$     then
$       dnrsl = f$trnlnm("decc$no_rooted_search_lists")
$     endif
$     define/nolog decc$no_rooted_search_lists 1
$   else
$     if its_vaxc then write sys$output "CC compiler check ... VAX C"
$     if its_gnuc
$     then
$         write sys$output "CC compiler check ... GNU C"
$         if f$trnlnm(topt) then write topt "gnu_cc:[000000]gcclib.olb/lib"
$         if f$trnlnm(optf) then write optf "gnu_cc:[000000]gcclib.olb/lib"
$         cc = "gcc"
$     endif
$     if f$trnlnm(topt) then write topt "sys$share:vaxcrtl.exe/share"
$     if f$trnlnm(optf) then write optf "sys$share:vaxcrtl.exe/share"
$   endif
$ endif
$ return
$!------------------------------------------------------------------------------
$!
$! If MMS/MMK are available dump out the descrip.mms if required
$!
$CREA_MMS:
$ write sys$output "Creating descrip.mms..."
$ create descrip.mms
$ open/append out descrip.mms
$ copy sys$input: out
$ deck
# descrip.mms: MMS description file for building zlib on VMS
# written by Martin P.J. Zinser
# <zinser@zinser.no-ip.info or martin.zinser@eurexchange.com>

OBJS = adler32.obj, compress.obj, crc32.obj, gzclose.obj, gzlib.obj\
       gzread.obj, gzwrite.obj, uncompr.obj, infback.obj\
       deflate.obj, trees.obj, zutil.obj, inflate.obj, \
       inftrees.obj, inffast.obj

$ eod
$ write out "CFLAGS=", ccopt
$ write out "LOPTS=", lopts
$ write out "all : example.exe minigzip.exe libz.olb"
$ copy sys$input: out
$ deck
        @ write sys$output " Example applications available"

libz.olb : libz.olb($(OBJS))
	@ write sys$output " libz available"

example.exe : example.obj libz.olb
              link $(LOPTS) example,libz.olb/lib

minigzip.exe : minigzip.obj libz.olb
              link $(LOPTS) minigzip,libz.olb/lib

clean :
	delete *.obj;*,libz.olb;*,*.opt;*,*.exe;*


# Other dependencies.
adler32.obj  : adler32.c zutil.h zlib.h zconf.h
compress.obj : compress.c zlib.h zconf.h
crc32.obj    : crc32.c zutil.h zlib.h zconf.h
deflate.obj  : deflate.c deflate.h zutil.h zlib.h zconf.h
example.obj  : [.test]example.c zlib.h zconf.h
gzclose.obj  : gzclose.c zutil.h zlib.h zconf.h
gzlib.obj    : gzlib.c zutil.h zlib.h zconf.h
gzread.obj   : gzread.c zutil.h zlib.h zconf.h
gzwrite.obj  : gzwrite.c zutil.h zlib.h zconf.h
inffast.obj  : inffast.c zutil.h zlib.h zconf.h inftrees.h inffast.h
inflate.obj  : inflate.c zutil.h zlib.h zconf.h
inftrees.obj : inftrees.c zutil.h zlib.h zconf.h inftrees.h
minigzip.obj : [.test]minigzip.c zlib.h zconf.h
trees.obj    : trees.c deflate.h zutil.h zlib.h zconf.h
uncompr.obj  : uncompr.c zlib.h zconf.h
zutil.obj    : zutil.c zutil.h zlib.h zconf.h
infback.obj  : infback.c zutil.h inftrees.h inflate.h inffast.h inffixed.h
$ eod
$ close out
$ return
$!------------------------------------------------------------------------------
$!
$! Read list of core library sources from makefile.in and create options
$! needed to build shareable image
$!
$CREA_OLIST:
$ open/read min makefile.in
$ open/write mod modules.opt
$ src_check_list = "OBJZ =#OBJG ="
$MRLOOP:
$ read/end=mrdone min rec
$ i = 0
$SRC_CHECK_LOOP:
$ src_check = f$element(i, "#", src_check_list)
$ i = i+1
$ if src_check .eqs. "#" then goto mrloop
$ if (f$extract(0,6,rec) .nes. src_check) then goto src_check_loop
$ rec = rec - src_check
$ gosub extra_filnam
$ if (f$element(1,"\",rec) .eqs. "\") then goto mrloop
$MRSLOOP:
$ read/end=mrdone min rec
$ gosub extra_filnam
$ if (f$element(1,"\",rec) .nes. "\") then goto mrsloop
$MRDONE:
$ close min
$ close mod
$ return
$!------------------------------------------------------------------------------
$!
$! Take record extracted in crea_olist and split it into single filenames
$!
$EXTRA_FILNAM:
$ myrec = f$edit(rec - "\", "trim,compress")
$ i = 0
$FELOOP:
$ srcfil = f$element(i," ", myrec)
$ if (srcfil .nes. " ")
$ then
$   write mod f$parse(srcfil,,,"NAME"), ".obj"
$   i = i + 1
$   goto feloop
$ endif
$ return
$!------------------------------------------------------------------------------
$!
$! Find current Zlib version number
$!
$FIND_VERSION:
$ open/read h_in 'v_file'
$hloop:
$ read/end=hdone h_in rec
$ rec = f$edit(rec,"TRIM")
$ if (f$extract(0,1,rec) .nes. "#") then goto hloop
$ rec = f$edit(rec - "#", "TRIM")
$ if f$element(0," ",rec) .nes. "define" then goto hloop
$ if f$element(1," ",rec) .eqs. v_string
$ then
$   version = 'f$element(2," ",rec)'
$   goto hdone
$ endif
$ goto hloop
$hdone:
$ close h_in
$ return
$!------------------------------------------------------------------------------
$!
$CHECK_CONFIG:
$!
$ in_ldef = f$locate(cdef,libdefs)
$ if (in_ldef .lt. f$length(libdefs))
$ then
$   write aconf "#define ''cdef' 1"
$   libdefs = f$extract(0,in_ldef,libdefs) + -
              f$extract(in_ldef + f$length(cdef) + 1, -
                        f$length(libdefs) - in_ldef - f$length(cdef) - 1, -
                        libdefs)
$ else
$   if (f$type('cdef') .eqs. "INTEGER")
$   then
$     write aconf "#define ''cdef' ", 'cdef'
$   else
$     if (f$type('cdef') .eqs. "STRING")
$     then
$       write aconf "#define ''cdef' ", """", '''cdef'', """"
$     else
$       gosub check_cc_def
$     endif
$   endif
$ endif
$ return
$!------------------------------------------------------------------------------
$!
$! Check if this is a define relating to the properties of the C/C++
$! compiler
$!
$ CHECK_CC_DEF:
$ if (cdef .eqs. "_LARGEFILE64_SOURCE")
$ then
$   copy sys$input: 'tc'
$   deck
#include "tconfig"
#define _LARGEFILE
#include <stdio.h>

int main(){
FILE *fp;
  fp = fopen("temp.txt","r");
  fseeko(fp,1,SEEK_SET);
  fclose(fp);
}

$   eod
$   test_inv = false
$   comm_h = false
$   gosub cc_prop_check
$   return
$ endif
$ write aconf "/* ", line, " */"
$ return
$!------------------------------------------------------------------------------
$!
$! Check for properties of C/C++ compiler
$!
$! Version history
$! 0.01 20031020 First version to receive a number
$! 0.02 20031022 Added logic for defines with value
$! 0.03 20040309 Make sure local config file gets not deleted
$! 0.04 20041230 Also write include for configure run
$! 0.05 20050103 Add processing of "comment defines"
$CC_PROP_CHECK:
$ cc_prop = true
$ is_need = false
$ is_need = (f$extract(0,4,cdef) .eqs. "NEED") .or. (test_inv .eq. true)
$ if f$search(th) .eqs. "" then create 'th'
$ set message/nofac/noident/nosever/notext
$ on error then continue
$ cc 'tmpnam'
$ if .not. ($status)  then cc_prop = false
$ on error then continue
$! The headers might lie about the capabilities of the RTL
$ link 'tmpnam',tmp.opt/opt
$ if .not. ($status)  then cc_prop = false
$ set message/fac/ident/sever/text
$ on error then goto err_exit
$ delete/nolog 'tmpnam'.*;*/exclude='th'
$ if (cc_prop .and. .not. is_need) .or. -
     (.not. cc_prop .and. is_need)
$ then
$   write sys$output "Checking for ''cdef'... yes"
$   if f$type('cdef_val'_yes) .nes. ""
$   then
$     if f$type('cdef_val'_yes) .eqs. "INTEGER" -
         then call write_config f$fao("#define !AS !UL",cdef,'cdef_val'_yes)
$     if f$type('cdef_val'_yes) .eqs. "STRING" -
         then call write_config f$fao("#define !AS !AS",cdef,'cdef_val'_yes)
$   else
$     call write_config f$fao("#define !AS 1",cdef)
$   endif
$   if (cdef .eqs. "HAVE_FSEEKO") .or. (cdef .eqs. "_LARGE_FILES") .or. -
       (cdef .eqs. "_LARGEFILE64_SOURCE") then -
      call write_config f$string("#define _LARGEFILE 1")
$ else
$   write sys$output "Checking for ''cdef'... no"
$   if (comm_h)
$   then
      call write_config f$fao("/* !AS */",line)
$   else
$     if f$type('cdef_val'_no) .nes. ""
$     then
$       if f$type('cdef_val'_no) .eqs. "INTEGER" -
           then call write_config f$fao("#define !AS !UL",cdef,'cdef_val'_no)
$       if f$type('cdef_val'_no) .eqs. "STRING" -
           then call write_config f$fao("#define !AS !AS",cdef,'cdef_val'_no)
$     else
$       call write_config f$fao("#undef !AS",cdef)
$     endif
$   endif
$ endif
$ return
$!------------------------------------------------------------------------------
$!
$! Check for properties of C/C++ compiler with multiple result values
$!
$! Version history
$! 0.01 20040127 First version
$! 0.02 20050103 Reconcile changes from cc_prop up to version 0.05
$CC_MPROP_CHECK:
$ cc_prop = true
$ i    = 1
$ idel = 1
$ MT_LOOP:
$ if f$type(result_'i') .eqs. "STRING"
$ then
$   set message/nofac/noident/nosever/notext
$   on error then continue
$   cc 'tmpnam'_'i'
$   if .not. ($status)  then cc_prop = false
$   on error then continue
$! The headers might lie about the capabilities of the RTL
$   link 'tmpnam'_'i',tmp.opt/opt
$   if .not. ($status)  then cc_prop = false
$   set message/fac/ident/sever/text
$   on error then goto err_exit
$   delete/nolog 'tmpnam'_'i'.*;*
$   if (cc_prop)
$   then
$     write sys$output "Checking for ''cdef'... ", mdef_'i'
$     if f$type(mdef_'i') .eqs. "INTEGER" -
         then call write_config f$fao("#define !AS !UL",cdef,mdef_'i')
$     if f$type('cdef_val'_yes) .eqs. "STRING" -
         then call write_config f$fao("#define !AS !AS",cdef,mdef_'i')
$     goto msym_clean
$   else
$     i = i + 1
$     goto mt_loop
$   endif
$ endif
$ write sys$output "Checking for ''cdef'... no"
$ call write_config f$fao("#undef !AS",cdef)
$ MSYM_CLEAN:
$ if (idel .le. msym_max)
$ then
$   delete/sym mdef_'idel'
$   idel = idel + 1
$   goto msym_clean
$ endif
$ return
$!------------------------------------------------------------------------------
$!
$! Write configuration to both permanent and temporary config file
$!
$! Version history
$! 0.01 20031029 First version to receive a number
$!
$WRITE_CONFIG: SUBROUTINE
$  write aconf 'p1'
$  open/append confh 'th'
$  write confh 'p1'
$  close confh
$ENDSUBROUTINE
$!------------------------------------------------------------------------------
$!
$! Analyze the project map file and create the symbol vector for a shareable
$! image from it
$!
$! Version history
$! 0.01 20120128 First version
$! 0.02 20120226 Add pre-load logic
$!
$ MAP_2_SHOPT: Subroutine
$!
$ SAY := "WRITE_ SYS$OUTPUT"
$!
$ IF F$SEARCH("''P1'") .EQS. ""
$ THEN
$    SAY "MAP_2_SHOPT-E-NOSUCHFILE:  Error, inputfile ''p1' not available"
$    goto exit_m2s
$ ENDIF
$ IF "''P2'" .EQS. ""
$ THEN
$    SAY "MAP_2_SHOPT:  Error, no output file provided"
$    goto exit_m2s
$ ENDIF
$!
$ module1 = "deflate#deflateEnd#deflateInit_#deflateParams#deflateSetDictionary"
$ module2 = "gzclose#gzerror#gzgetc#gzgets#gzopen#gzprintf#gzputc#gzputs#gzread"
$ module3 = "gzseek#gztell#inflate#inflateEnd#inflateInit_#inflateSetDictionary"
$ module4 = "inflateSync#uncompress#zlibVersion#compress"
$ open/read map 'p1
$ if axp .or. ia64
$ then
$     open/write aopt a.opt
$     open/write bopt b.opt
$     write aopt " CASE_SENSITIVE=YES"
$     write bopt "SYMBOL_VECTOR= (-"
$     mod_sym_num = 1
$ MOD_SYM_LOOP:
$     if f$type(module'mod_sym_num') .nes. ""
$     then
$         mod_in = 0
$ MOD_SYM_IN:
$         shared_proc = f$element(mod_in, "#", module'mod_sym_num')
$         if shared_proc .nes. "#"
$         then
$             write aopt f$fao(" symbol_vector=(!AS/!AS=PROCEDURE)",-
        		       f$edit(shared_proc,"upcase"),shared_proc)
$             write bopt f$fao("!AS=PROCEDURE,-",shared_proc)
$             mod_in = mod_in + 1
$             goto mod_sym_in
$         endif
$         mod_sym_num = mod_sym_num + 1
$         goto mod_sym_loop
$     endif
$MAP_LOOP:
$     read/end=map_end map line
$     if (f$locate("{",line).lt. f$length(line)) .or. -
         (f$locate("global:", line) .lt. f$length(line))
$     then
$         proc = true
$         goto map_loop
$     endif
$     if f$locate("}",line).lt. f$length(line) then proc = false
$     if f$locate("local:", line) .lt. f$length(line) then proc = false
$     if proc
$     then
$         shared_proc = f$edit(line,"collapse")
$         chop_semi = f$locate(";", shared_proc)
$         if chop_semi .lt. f$length(shared_proc) then -
              shared_proc = f$extract(0, chop_semi, shared_proc)
$         write aopt f$fao(" symbol_vector=(!AS/!AS=PROCEDURE)",-
        			 f$edit(shared_proc,"upcase"),shared_proc)
$         write bopt f$fao("!AS=PROCEDURE,-",shared_proc)
$     endif
$     goto map_loop
$MAP_END:
$     close/nolog aopt
$     close/nolog bopt
$     open/append libopt 'p2'
$     open/read aopt a.opt
$     open/read bopt b.opt
$ALOOP:
$     read/end=aloop_end aopt line
$     write libopt line
$     goto aloop
$ALOOP_END:
$     close/nolog aopt
$     sv = ""
$BLOOP:
$     read/end=bloop_end bopt svn
$     if (svn.nes."")
$     then
$        if (sv.nes."") then write libopt sv
$        sv = svn
$     endif
$     goto bloop
$BLOOP_END:
$     write libopt f$extract(0,f$length(sv)-2,sv), "-"
$     write libopt ")"
$     close/nolog bopt
$     delete/nolog/noconf a.opt;*,b.opt;*
$ else
$     if vax
$     then
$     open/append libopt 'p2'
$     mod_sym_num = 1
$ VMOD_SYM_LOOP:
$     if f$type(module'mod_sym_num') .nes. ""
$     then
$         mod_in = 0
$ VMOD_SYM_IN:
$         shared_proc = f$element(mod_in, "#", module'mod_sym_num')
$         if shared_proc .nes. "#"
$         then
$     	      write libopt f$fao("UNIVERSAL=!AS",-
      	  			     f$edit(shared_proc,"upcase"))
$             mod_in = mod_in + 1
$             goto vmod_sym_in
$         endif
$         mod_sym_num = mod_sym_num + 1
$         goto vmod_sym_loop
$     endif
$VMAP_LOOP:
$     	  read/end=vmap_end map line
$     	  if (f$locate("{",line).lt. f$length(line)) .or. -
   	      (f$locate("global:", line) .lt. f$length(line))
$     	  then
$     	      proc = true
$     	      goto vmap_loop
$     	  endif
$     	  if f$locate("}",line).lt. f$length(line) then proc = false
$     	  if f$locate("local:", line) .lt. f$length(line) then proc = false
$     	  if proc
$     	  then
$     	      shared_proc = f$edit(line,"collapse")
$     	      chop_semi = f$locate(";", shared_proc)
$     	      if chop_semi .lt. f$length(shared_proc) then -
      	  	  shared_proc = f$extract(0, chop_semi, shared_proc)
$     	      write libopt f$fao("UNIVERSAL=!AS",-
      	  			     f$edit(shared_proc,"upcase"))
$     	  endif
$     	  goto vmap_loop
$VMAP_END:
$     else
$         write sys$output "Unknown Architecture (Not VAX, AXP, or IA64)"
$         write sys$output "No options file created"
$     endif
$ endif
$ EXIT_M2S:
$ close/nolog map
$ close/nolog libopt
$ endsubroutine

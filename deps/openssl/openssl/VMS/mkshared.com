$! MKSHARED.COM -- script to created shareable images on VMS
$!
$! No command line parameters.  This should be run at the start of the source
$! tree (the same directory where one finds INSTALL.VMS).
$!
$! Input:	[.UTIL]LIBEAY.NUM,[.xxx.EXE.CRYPTO]LIBCRYPTO.OLB
$!		[.UTIL]SSLEAY.NUM,[.xxx.EXE.SSL]LIBSSL.OLB
$! Output:	[.xxx.EXE.CRYPTO]LIBCRYPTO.OPT,.MAP,.EXE
$!		[.xxx.EXE.SSL]LIBSSL.OPT,.MAP,.EXE
$!
$! So far, tests have only been made on VMS for Alpha.  VAX will come in time.
$! ===========================================================================
$
$! ----- Prepare info for processing: version number and file info
$ gosub read_version_info
$ if libver .eqs. ""
$ then
$   write sys$error "ERROR: Couldn't find any library version info..."
$   exit
$ endif
$
$ if (f$getsyi("cpu").lt.128)
$ then
$     arch := VAX
$ else
$     arch = f$edit( f$getsyi( "ARCH_NAME"), "UPCASE")
$     if (arch .eqs. "") then arch = "UNK"
$ endif
$
$ if arch .nes. "VAX"
$ then
$   arch_vax = 0
$   libid  = "Crypto"
$   libnum = "[.UTIL]LIBEAY.NUM"
$   libdir = "[.''ARCH'.EXE.CRYPTO]"
$   libolb = "''libdir'LIBCRYPTO.OLB"
$   libopt = "''libdir'LIBCRYPTO.OPT"
$   libmap = "''libdir'LIBCRYPTO.MAP"
$   libgoal= "''libdir'LIBCRYPTO.EXE"
$   libref = ""
$   gosub create_nonvax_shr
$   libid  = "SSL"
$   libnum = "[.UTIL]SSLEAY.NUM"
$   libdir = "[.''ARCH'.EXE.SSL]"
$   libolb = "''libdir'LIBSSL.OLB"
$   libopt = "''libdir'LIBSSL.OPT"
$   libmap = "''libdir'LIBSSL.MAP"
$   libgoal= "''libdir'LIBSSL.EXE"
$   libref = "[.''ARCH'.EXE.CRYPTO]LIBCRYPTO.EXE"
$   gosub create_nonvax_shr
$ else
$   arch_vax = 1
$   libtit = "CRYPTO_TRANSFER_VECTOR"
$   libid  = "Crypto"
$   libnum = "[.UTIL]LIBEAY.NUM"
$   libdir = "[.''ARCH'.EXE.CRYPTO]"
$   libmar = "''libdir'LIBCRYPTO.MAR"
$   libolb = "''libdir'LIBCRYPTO.OLB"
$   libopt = "''libdir'LIBCRYPTO.OPT"
$   libobj = "''libdir'LIBCRYPTO.OBJ"
$   libmap = "''libdir'LIBCRYPTO.MAP"
$   libgoal= "''libdir'LIBCRYPTO.EXE"
$   libref = ""
$   libvec = "LIBCRYPTO"
$   gosub create_vax_shr
$   libtit = "SSL_TRANSFER_VECTOR"
$   libid  = "SSL"
$   libnum = "[.UTIL]SSLEAY.NUM"
$   libdir = "[.''ARCH'.EXE.SSL]"
$   libmar = "''libdir'LIBSSL.MAR"
$   libolb = "''libdir'LIBSSL.OLB"
$   libopt = "''libdir'LIBSSL.OPT"
$   libobj = "''libdir'LIBSSL.OBJ"
$   libmap = "''libdir'LIBSSL.MAP"
$   libgoal= "''libdir'LIBSSL.EXE"
$   libref = "[.''ARCH'.EXE.CRYPTO]LIBCRYPTO.EXE"
$   libvec = "LIBSSL"
$   gosub create_vax_shr
$ endif
$ exit
$
$! ----- Soubroutines to build the shareable libraries
$! For each supported architecture, there's a main shareable library
$! creator, which is called from the main code above.
$! The creator will define a number of variables to tell the next levels of
$! subroutines what routines to use to write to the option files, call the
$! main processor, read_func_num, and when that is done, it will write version
$! data at the end of the .opt file, close it, and link the library.
$!
$! read_func_num reads through a .num file and calls the writer routine for
$! each line.  It's also responsible for checking that order is properly kept
$! in the .num file, check that each line applies to VMS and the architecture,
$! and to fill in "holes" with dummy entries.
$!
$! The creator routines depend on the following variables:
$! libnum	The name of the .num file to use as input
$! libolb	The name of the object library to build from
$! libid	The identification string of the shareable library
$! libopt	The name of the .opt file to write
$! libtit	The title of the assembler transfer vector file (VAX only)
$! libmar	The name of the assembler transfer vector file (VAX only)
$! libmap	The name of the map file to write
$! libgoal	The name of the shareable library to write
$! libref	The name of a shareable library to link in
$!
$! read_func_num depends on the following variables from the creator:
$! libwriter	The name of the writer routine to call for each .num file line
$! -----
$
$! ----- Subroutines for non-VAX
$! -----
$! The creator routine
$ create_nonvax_shr:
$   open/write opt 'libopt'
$   write opt "identification=""",libid," ",libverstr,""""
$   write opt libolb,"/lib"
$   if libref .nes. "" then write opt libref,"/SHARE"
$   write opt "SYMBOL_VECTOR=(-"
$   libfirstentry := true
$   libwrch   := opt
$   libwriter := write_nonvax_transfer_entry
$   textcount = 0
$   gosub read_func_num
$   write opt ")"
$   write opt "GSMATCH=",libvmatch,",",libver
$   close opt
$   link/map='libmap'/full/share='libgoal' 'libopt'/option
$   return
$
$! The record writer routine
$ write_nonvax_transfer_entry:
$   if libentry .eqs. ".dummy" then return
$   if info_kind .eqs. "VARIABLE"
$   then
$     pr:=DATA
$   else
$     pr:=PROCEDURE
$   endif
$   textcount_this = f$length(pr) + f$length(libentry) + 5
$   if textcount + textcount_this .gt. 1024
$   then
$     write opt ")"
$     write opt "SYMBOL_VECTOR=(-"
$     textcount = 16
$     libfirstentry := true
$   endif
$   if libfirstentry
$   then
$     write 'libwrch' "    ",libentry,"=",pr," -"
$   else
$     write 'libwrch' "    ,",libentry,"=",pr," -"
$   endif
$   libfirstentry := false
$   textcount = textcount + textcount_this
$   return
$
$! ----- Subroutines for VAX
$! -----
$! The creator routine
$ create_vax_shr:
$   open/write mar 'libmar'
$   type sys$input:/out=mar:
;
; Transfer vector for VAX shareable image
;
$   write mar "	.TITLE ",libtit
$   write mar "	.IDENT /",libid,"/"
$   type sys$input:/out=mar:
;
; Define macro to assist in building transfer vector entries.  Each entry
; should take no more than 8 bytes.
;
	.MACRO FTRANSFER_ENTRY routine
	.ALIGN QUAD
	.TRANSFER routine
	.MASK	routine
	JMP	routine+2
	.ENDM FTRANSFER_ENTRY
;
; Place entries in own program section.
;
$   write mar "	.PSECT $$",libvec,",QUAD,PIC,USR,CON,REL,LCL,SHR,EXE,RD,NOWRT"
$   write mar libvec,"_xfer:"
$   libwrch   := mar
$   libwriter := write_vax_ftransfer_entry
$   gosub read_func_num
$   type sys$input:/out=mar:
;
; Allocate extra storage at end of vector to allow for expansion.
;
$   write mar "	.BLKB 32768-<.-",libvec,"_xfer>	; 64 pages total."
$!   libwriter := write_vax_vtransfer_entry
$!   gosub read_func_num
$   write mar "	.END"
$   close mar
$   open/write opt 'libopt'
$   write opt "identification=""",libid," ",libverstr,""""
$   write opt libobj
$   write opt libolb,"/lib"
$   if libref .nes. "" then write opt libref,"/SHARE"
$   type sys$input:/out=opt:
!
! Ensure transfer vector is at beginning of image
!
CLUSTER=FIRST
$   write opt "COLLECT=FIRST,$$",libvec
$   write opt "GSMATCH=",libvmatch,",",libver
$   type sys$input:/out=opt:
!
! make psects nonshareable so image can be installed.
!
PSECT_ATTR=$CHAR_STRING_CONSTANTS,NOWRT
$   libwrch   := opt
$   libwriter := write_vax_psect_attr
$   gosub read_func_num
$   close opt
$   macro/obj='libobj' 'libmar'
$   link/map='libmap'/full/share='libgoal' 'libopt'/option
$   return
$
$! The record writer routine for VAX functions
$ write_vax_ftransfer_entry:
$   if info_kind .nes. "FUNCTION" then return
$   if libentry .eqs ".dummy"
$   then
$     write 'libwrch' "	.BLKB 8" ! Dummy is zeroes...
$   else
$     write 'libwrch' "	FTRANSFER_ENTRY ",libentry
$   endif
$   return
$! The record writer routine for VAX variables (should never happen!)
$ write_vax_psect_attr:
$   if info_kind .nes. "VARIABLE" then return
$   if libentry .eqs ".dummy" then return
$   write 'libwrch' "PSECT_ATTR=",libentry,",NOSHR"
$   return
$
$! ----- Common subroutines
$! -----
$! The .num file reader.  This one has great responsability.
$ read_func_num:
$   open libnum 'libnum'
$   goto read_nums
$
$ read_nums:
$   libentrynum=0
$   liblastentry:=false
$   entrycount=0
$   loop:
$     read/end=loop_end/err=loop_end libnum line
$     entrynum=f$int(f$element(1," ",f$edit(line,"COMPRESS,TRIM")))
$     entryinfo=f$element(2," ",f$edit(line,"COMPRESS,TRIM"))
$     curentry=f$element(0," ",f$edit(line,"COMPRESS,TRIM"))
$     info_exist=f$element(0,":",entryinfo)
$     info_platforms=","+f$element(1,":",entryinfo)+","
$     info_kind=f$element(2,":",entryinfo)
$     info_algorithms=","+f$element(3,":",entryinfo)+","
$     if info_exist .eqs. "NOEXIST" then goto loop
$     truesum = 0
$     falsesum = 0
$     negatives = 1
$     plat_i = 0
$     loop1:
$       plat_entry = f$element(plat_i,",",info_platforms)
$       plat_i = plat_i + 1
$       if plat_entry .eqs. "" then goto loop1
$       if plat_entry .nes. ","
$       then
$         if f$extract(0,1,plat_entry) .nes. "!" then negatives = 0
$         if f$getsyi("CPU") .lt. 128
$         then
$           if plat_entry .eqs. "EXPORT_VAR_AS_FUNCTION" then -
$             truesum = truesum + 1
$           if plat_entry .eqs. "!EXPORT_VAR_AS_FUNCTION" then -
$             falsesum = falsesum + 1
$         endif
$!
$         if ((plat_entry .eqs. "VMS") .or. -
            (arch_vax .and. (plat_entry .eqs. "VMSVAX"))) then -
            truesum = truesum + 1
$!
$         if ((plat_entry .eqs. "!VMS") .or. -
            (arch_vax .and. (plat_entry .eqs. "!VMSVAX"))) then -
            falsesum = falsesum + 1
$!
$	  goto loop1
$       endif
$     endloop1:
$!DEBUG!$     if info_platforms - "EXPORT_VAR_AS_FUNCTION" .nes. info_platforms
$!DEBUG!$     then
$!DEBUG!$       write sys$output line
$!DEBUG!$       write sys$output "        truesum = ",truesum,-
$!DEBUG!		", negatives = ",negatives,", falsesum = ",falsesum
$!DEBUG!$     endif
$     if falsesum .ne. 0 then goto loop
$     if truesum+negatives .eq. 0 then goto loop
$     alg_i = 0
$     loop2:
$       alg_entry = f$element(alg_i,",",info_algorithms)
$	alg_i = alg_i + 1
$       if alg_entry .eqs. "" then goto loop2
$       if alg_entry .nes. ","
$       then
$         if alg_entry .eqs. "KRB5" then goto loop ! Special for now
$	  if alg_entry .eqs. "STATIC_ENGINE" then goto loop ! Special for now
$         if f$trnlnm("OPENSSL_NO_"+alg_entry) .nes. "" then goto loop
$	  goto loop2
$       endif
$     endloop2:
$     if info_platforms - "EXPORT_VAR_AS_FUNCTION" .nes. info_platforms
$     then
$!DEBUG!$     write sys$output curentry," ; ",entrynum," ; ",entryinfo
$     endif
$   redo:
$     next:=loop
$     tolibentry=curentry
$     if libentrynum .ne. entrynum
$     then
$       entrycount=entrycount+1
$       if entrycount .lt. entrynum
$       then
$!DEBUG!$         write sys$output "Info: entrycount: ''entrycount', entrynum: ''entrynum' => 0"
$         tolibentry=".dummy"
$         next:=redo
$       endif
$       if entrycount .gt. entrynum
$       then
$         write sys$error "Decreasing library entry numbers!  Can't continue"
$         write sys$error """",line,""""
$         close libnum
$         return
$       endif
$       libentry=tolibentry
$!DEBUG!$       write sys$output entrycount," ",libentry," ",entryinfo
$       if libentry .nes. "" .and. libwriter .nes. "" then gosub 'libwriter'
$     else
$       write sys$error "Info: ""''curentry'"" is an alias for ""''libentry'"".  Overriding..."
$     endif
$     libentrynum=entrycount
$     goto 'next'
$   loop_end:
$   close libnum
$   return
$
$! The version number reader
$ read_version_info:
$   libver = ""
$   open/read vf [.CRYPTO]OPENSSLV.H
$   loop_rvi:
$     read/err=endloop_rvi/end=endloop_rvi vf rvi_line
$     if rvi_line - "SHLIB_VERSION_NUMBER """ .eqs. rvi_line then -
	goto loop_rvi
$     libverstr = f$element(1,"""",rvi_line)
$     libvmajor = f$element(0,".",libverstr)
$     libvminor = f$element(1,".",libverstr)
$     libvedit = f$element(2,".",libverstr)
$     libvpatch = f$cvui(0,8,f$extract(1,1,libvedit)+"@")-f$cvui(0,8,"@")
$     libvedit = f$extract(0,1,libvedit)
$     libver = f$string(f$int(libvmajor)*100)+","+-
	f$string(f$int(libvminor)*100+f$int(libvedit)*10+f$int(libvpatch))
$     if libvmajor .eqs. "0"
$     then
$       libvmatch = "EQUAL"
$     else
$       ! Starting with the 1.0 release, backward compatibility should be
$       ! kept, so switch over to the following
$       libvmatch = "LEQUAL"
$     endif
$   endloop_rvi:
$   close vf
$   return

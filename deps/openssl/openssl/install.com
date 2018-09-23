$! INSTALL.COM -- Installs the files in a given directory tree
$!
$! Author: Richard Levitte <richard@levitte.org>
$! Time of creation: 22-MAY-1998 10:13
$!
$! P1  root of the directory tree
$! P2  "64" for 64-bit pointers.
$!
$!
$! Announce/identify.
$!
$ proc = f$environment( "procedure")
$ write sys$output "@@@ "+ -
   f$parse( proc, , , "name")+ f$parse( proc, , , "type")
$!
$ def_orig = f$environment( "default")
$ on error then goto tidy
$ on control_c then goto tidy
$!
$ if (p1 .eqs. "")
$ then
$   write sys$output "First argument missing."
$   write sys$output -
     "It should be the directory where you want things installed."
$   exit
$ endif
$!
$ if (f$getsyi("cpu") .lt. 128)
$ then
$   arch = "VAX"
$ else
$   arch = f$edit( f$getsyi( "arch_name"), "upcase")
$   if (arch .eqs. "") then arch = "UNK"
$ endif
$!
$ archd = arch
$!
$ if (p2 .nes. "")
$ then
$   if (p2 .eqs. "64")
$   then
$     archd = arch+ "_64"
$   else
$     if (p2 .nes. "32")
$     then
$       write sys$output "Second argument invalid."
$       write sys$output "It should be "32", "64", or nothing."
$       exit
$     endif
$   endif
$ endif
$!
$ root = f$parse( p1, "[]A.;0", , , "syntax_only, no_conceal") - "A.;0"
$ root_dev = f$parse( root, , , "device", "syntax_only")
$ root_dir = f$parse( root, , , "directory", "syntax_only") -
		   - ".][000000" - "[000000." - "][" - "[" - "]"
$ root = root_dev + "[" + root_dir
$!
$ define /nolog wrk_sslroot 'root'.] /trans=conc
$ define /nolog wrk_sslcerts wrk_sslroot:[certs]
$ define /nolog wrk_sslinclude wrk_sslroot:[include]
$ define /nolog wrk_ssllib wrk_sslroot:[lib]
$ define /nolog wrk_sslprivate wrk_sslroot:[private]
$ define /nolog wrk_sslxexe wrk_sslroot:['archd'_exe]
$ define /nolog wrk_sslxlib wrk_sslroot:['arch'_lib]
$!
$! Exhibit the destination directory.
$!
$ write sys$output "   Installing to (WRK_SSLROOT) ="
$ write sys$output "    ''f$trnlnm( "wrk_sslroot")'"
$ write sys$output ""
$!
$ if f$parse("wrk_sslroot:[000000]") .eqs. "" then -
   create /directory /log wrk_sslroot:[000000]
$ if f$parse("wrk_sslxexe:") .eqs. "" then -
   create /directory /log wrk_sslxexe:
$ if f$parse("wrk_sslxlib:") .eqs. "" then -
   create /directory /log wrk_sslxlib:
$ if f$parse("wrk_ssllib:") .eqs. "" then -
   create /directory /log wrk_ssllib:
$ if f$parse("wrk_sslinclude:") .eqs. "" then -
   create /directory /log wrk_sslinclude:
$ if f$parse("wrk_sslcerts:") .eqs. "" then -
   create /directory /log wrk_sslcerts:
$ if f$parse("wrk_sslprivate:") .eqs. "" then -
   create /directory /log wrk_sslprivate:
$ if f$parse("wrk_sslroot:[VMS]") .EQS. "" THEN -
   create /directory /log wrk_sslroot:[VMS]
$!
$ sdirs := CRYPTO, SSL, APPS, VMS !!!, RSAREF, TEST, TOOLS
$ exheader := e_os2.h
$!
$ copy /protection = w:re 'exheader' wrk_sslinclude: /log
$!
$ i = 0
$ loop_sdirs: 
$   d = f$edit( f$element(i, ",", sdirs), "trim")
$   i = i + 1
$   if d .eqs. "," then goto loop_sdirs_end
$   write sys$output "Installing ", d, " files."
$   set default [.'d']
$   @ install-'d'.com 'root'] 'p2'
$   set default 'def_orig'
$ goto loop_sdirs
$ loop_sdirs_end:
$!
$ write sys$output ""
$ write sys$output "	Installation done!"
$ write sys$output ""
$ if (f$search( root+ "...]*.*;-1") .nes. "")
$ then
$   write sys$output "	You might want to purge ", root, "...]"
$   write sys$output ""
$ endif
$!
$ tidy:
$!
$ set default 'def_orig'
$!
$ call deass wrk_sslroot
$ call deass wrk_sslcerts
$ call deass wrk_sslinclude
$ call deass wrk_ssllib
$ call deass wrk_sslprivate
$ call deass wrk_sslxexe
$ call deass wrk_sslxlib
$!
$ exit
$!
$ deass: subroutine
$ if (f$trnlnm( p1, "LNM$PROCESS") .nes. "")
$ then
$   deassign /process 'p1'
$ endif
$ endsubroutine
$!

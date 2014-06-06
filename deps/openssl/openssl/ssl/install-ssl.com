$! INSTALL-SSL.COM -- Installs the files in a given directory tree
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
$ on error then goto tidy
$ on control_c then goto tidy
$!
$ if p1 .eqs. ""
$ then
$   write sys$output "First argument missing."
$   write sys$output -
     "It should be the directory where you want things installed."
$   exit
$ endif
$!
$ if (f$getsyi( "cpu") .lt. 128)
$ then
$     arch = "VAX"
$ else
$     arch = f$edit( f$getsyi( "arch_name"), "upcase")
$     if (arch .eqs. "") then arch = "UNK"
$ endif
$!
$ archd = arch
$ lib32 = "32"
$ shr = "_SHR32"
$!
$ if (p2 .nes. "")
$ then
$   if (p2 .eqs. "64")
$   then
$     archd = arch+ "_64"
$     lib32 = ""
$     shr = "_SHR"
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
$ root_dev = f$parse(root,,,"device","syntax_only")
$ root_dir = f$parse(root,,,"directory","syntax_only") - -
   "[000000." - "][" - "[" - "]"
$ root = root_dev + "[" + root_dir
$!
$ define /nolog wrk_sslroot 'root'.] /trans=conc
$ define /nolog wrk_sslinclude wrk_sslroot:[include]
$ define /nolog wrk_sslxexe wrk_sslroot:['archd'_exe]
$ define /nolog wrk_sslxlib wrk_sslroot:['arch'_lib]
$!
$ if f$parse("wrk_sslroot:[000000]") .eqs. "" then -
   create /directory /log wrk_sslroot:[000000]
$ if f$parse("wrk_sslinclude:") .eqs. "" then -
   create /directory /log wrk_sslinclude:
$ if f$parse("wrk_sslxexe:") .eqs. "" then -
   create /directory /log wrk_sslxexe:
$ if f$parse("wrk_sslxlib:") .eqs. "" then -
   create /directory /log wrk_sslxlib:
$!
$ exheader := ssl.h, ssl2.h, ssl3.h, ssl23.h, tls1.h, dtls1.h, kssl.h
$ e_exe := ssl_task
$ libs := ssl_libssl
$!
$ xexe_dir := [-.'archd'.exe.ssl]
$!
$ copy /protection = w:re 'exheader' wrk_sslinclude: /log
$!
$ i = 0
$ loop_exe:
$   e = f$edit( f$element( i, ",", e_exe), "trim")
$   i = i + 1
$   if e .eqs. "," then goto loop_exe_end
$   set noon
$   file = xexe_dir+ e+ ".exe"
$   if f$search( file) .nes. ""
$   then
$     copy /protection = w:re 'file' wrk_sslxexe: /log
$   endif
$   set on
$ goto loop_exe
$ loop_exe_end:
$!
$ i = 0
$ loop_lib: 
$   e = f$edit(f$element(i, ",", libs),"trim")
$   i = i + 1
$   if e .eqs. "," then goto loop_lib_end
$   set noon
$! Object library.
$   file = xexe_dir+ e+ lib32+ ".olb"
$   if f$search( file) .nes. ""
$   then
$     copy /protection = w:re 'file' wrk_sslxlib: /log
$   endif
$! Shareable image.
$   file = xexe_dir+ e+ shr+ ".exe"
$   if f$search( file) .nes. ""
$   then
$     copy /protection = w:re 'file' wrk_sslxlib: /log
$   endif
$   set on
$ goto loop_lib
$ loop_lib_end:
$!
$ tidy:
$!
$ call deass wrk_sslroot
$ call deass wrk_sslinclude
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

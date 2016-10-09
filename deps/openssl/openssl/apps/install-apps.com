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
$ root_dev = f$parse(root,,,"device","syntax_only")
$ root_dir = f$parse(root,,,"directory","syntax_only") - -
   "[000000." - "][" - "[" - "]"
$ root = root_dev + "[" + root_dir
$!
$ define /nolog wrk_sslroot 'root'.] /trans=conc
$ define /nolog wrk_sslxexe wrk_sslroot:['archd'_exe]
$!
$ if f$parse("wrk_sslroot:[000000]") .eqs. "" then -
   create /directory /log wrk_sslroot:[000000]
$ if f$parse("wrk_sslxexe:") .eqs. "" then -
   create /directory /log wrk_sslxexe:
$!
$ exe := openssl
$!
$ exe_dir := [-.'archd'.exe.apps]
$!
$! Executables.
$!
$ i = 0
$ loop_exe:
$   e = f$edit(f$element( i, ",", exe), "trim")
$   i = i + 1
$   if e .eqs. "," then goto loop_exe_end
$   set noon
$   file = exe_dir+ e+ ".exe"
$   if f$search( file) .nes. ""
$   then
$     copy /protection = w:re 'file' wrk_sslxexe: /log
$   endif
$   set on
$ goto loop_exe
$ loop_exe_end:
$!
$! Miscellaneous.
$!
$ set noon
$ copy /protection = w:re ca.com wrk_sslxexe:ca.com /log
$ copy /protection = w:re openssl-vms.cnf wrk_sslroot:[000000]openssl.cnf /log
$ set on
$!
$ tidy:
$!
$ call deass wrk_sslroot
$ call deass wrk_sslxexe
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

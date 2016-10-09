$! install-vms.com -- Installs the files in a given directory tree
$!
$! Author: Richard Levitte <richard@levitte.org>
$! Time of creation: 23-MAY-1998 19:22
$!
$! P1	root of the directory tree
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
     "Should be the directory where you want things installed."
$   exit
$ endif
$
$ if (f$getsyi( "cpu") .lt. 128)
$ then
$   arch = "VAX"
$ else
$   arch = f$edit( f$getsyi( "arch_name"), "upcase")
$   if (arch .eqs. "") then arch = "UNK"
$ endif
$
$ root = f$parse( P1, "[]A.;0", , , "SYNTAX_ONLY, NO_CONCEAL")- "A.;0"
$ root_dev = f$parse( root, , , "device", "syntax_only")
$ root_dir = f$parse( root, , , "directory", "syntax_only") - -
   "[000000." - "][" - "[" - "]"
$ root = root_dev + "[" + root_dir
$
$ define /nolog wrk_sslroot 'root'.] /translation_attributes = concealed
$ define /nolog wrk_sslinclude wrk_sslroot:[include]
$
$ if f$parse( "wrk_sslroot:[000000]") .eqs. "" then -
   create /directory /log wrk_sslroot:[000000]
$ if f$parse( "wrk_sslinclude:") .eqs. "" then -
   create /directory /log wrk_sslinclude:
$ if f$parse( "wrk_sslroot:[vms]") .eqs. "" then -
   create /directory /log wrk_sslroot:[vms]
$!
$ copy /log /protection = world:re openssl_startup.com wrk_sslroot:[vms]
$ copy /log /protection = world:re openssl_undo.com wrk_sslroot:[vms]
$ copy /log /protection = world:re openssl_utils.com wrk_sslroot:[vms]
$!
$ tidy:
$!
$ call deass wrk_sslroot
$ call deass wrk_sslinclude
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

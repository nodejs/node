$!
$! Startup file for OpenSSL 1.x.
$!
$! 2011-03-05 SMS.
$!
$! This procedure must reside in the OpenSSL installation directory.
$! It will fail if it is copied to a different location.
$!
$! P1  qualifier(s) for DEFINE.  For example, "/SYSTEM" to get the
$!     logical names defined in the system logical name table.
$!
$! P2  "64", to use executables which were built with 64-bit pointers.
$!
$! Good (default) and bad status values.
$!
$ status =    %x00010001 ! RMS$_NORMAL, normal successful completion.
$ rms_e_fnf = %x00018292 ! RMS$_FNF, file not found.
$!
$! Prepare for problems.
$!
$ orig_dev_dir = f$environment( "DEFAULT")
$ on control_y then goto clean_up
$ on error then goto clean_up
$!
$! Determine hardware architecture.
$!
$ if (f$getsyi( "cpu") .lt. 128)
$ then
$   arch_name = "VAX"
$ else
$   arch_name = f$edit( f$getsyi( "arch_name"), "upcase")
$   if (arch_name .eqs. "") then arch_name = "UNK"
$ endif
$!
$ if (p2 .eqs. "64")
$ then
$   arch_name_exe = arch_name+ "_64"
$ else
$   arch_name_exe = arch_name
$ endif
$!
$! Derive the OpenSSL installation device:[directory] from the location
$! of this command procedure.
$!
$ proc = f$environment( "procedure")
$ proc_dev_dir = f$parse( "A.;", proc, , , "no_conceal") - "A.;"
$ proc_dev = f$parse( proc_dev_dir, , , "device", "syntax_only")
$ proc_dir = f$parse( proc_dev_dir, , , "directory", "syntax_only") - -
   ".][000000"- "[000000."- "]["- "["- "]"
$ proc_dev_dir = proc_dev+ "["+ proc_dir+ "]"
$ set default 'proc_dev_dir'
$ set default [-]
$ ossl_dev_dir = f$environment( "default")
$!
$! Check existence of expected directories (to see if this procedure has
$! been moved away from its proper place).
$!
$ if ((f$search( "certs.dir;1") .eqs. "") .or. -
   (f$search( "include.dir;1") .eqs. "") .or. -
   (f$search( "private.dir;1") .eqs. "") .or. -
   (f$search( "vms.dir;1") .eqs. ""))
$ then
$    write sys$output -
      "   Can't find expected common OpenSSL directories in:"
$    write sys$output "   ''ossl_dev_dir'"
$    status = rms_e_fnf
$    goto clean_up
$ endif
$!
$ if ((f$search( "''arch_name_exe'_exe.dir;1") .eqs. "") .or. -
   (f$search( "''arch_name'_lib.dir;1") .eqs. ""))
$ then
$    write sys$output -
      "   Can't find expected architecture-specific OpenSSL directories in:"
$    write sys$output "   ''ossl_dev_dir'"
$    status = rms_e_fnf
$    goto clean_up
$ endif
$!
$! All seems well (enough).  Define the OpenSSL logical names.
$!
$ ossl_root = ossl_dev_dir- "]"+ ".]"
$ define /translation_attributes = concealed /nolog'p1 SSLROOT 'ossl_root'
$ define /nolog 'p1' SSLCERTS     sslroot:[certs]
$ define /nolog 'p1' SSLINCLUDE   sslroot:[include]
$ define /nolog 'p1' SSLPRIVATE   sslroot:[private]
$ define /nolog 'p1' SSLEXE       sslroot:['arch_name_exe'_exe]
$ define /nolog 'p1' SSLLIB       sslroot:['arch_name'_lib]
$!
$! Defining OPENSSL lets a C program use "#include <openssl/{foo}.h>":
$ define /nolog 'p1' OPENSSL      SSLINCLUDE:
$!
$! Run a site-specific procedure, if it exists.
$!
$ if f$search( "sslroot:[vms]openssl_systartup.com") .nes."" then -
   @ sslroot:[vms]openssl_systartup.com
$!
$! Restore the original default dev:[dir] (if known).
$!
$ clean_up:
$!
$ if (f$type( orig_dev_dir) .nes. "")
$ then
$    set default 'orig_dev_dir'
$ endif
$!
$ EXIT 'status'
$!

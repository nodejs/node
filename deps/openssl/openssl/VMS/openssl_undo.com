$!
$! Deassign OpenSSL logical names.
$!
$ call deass "OPENSSL" "''p1'"
$ call deass "SSLCERTS" "''p1'"
$ call deass "SSLEXE" "''p1'"
$ call deass "SSLINCLUDE" "''p1'"
$ call deass "SSLLIB" "''p1'"
$ call deass "SSLPRIVATE" "''p1'"
$ call deass "SSLROOT" "''p1'"
$!
$ exit
$!
$deass: subroutine
$ if (f$trnlnm( p1) .nes. "")
$ then
$    deassign 'p2' 'p1'
$ endif
$ endsubroutine
$!

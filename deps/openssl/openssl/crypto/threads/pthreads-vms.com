$! To compile mttest on VMS.
$!
$! WARNING: only tested with DEC C so far.
$
$ if (f$getsyi("cpu").lt.128)
$ then
$     arch := VAX
$ else
$     arch = f$edit( f$getsyi( "ARCH_NAME"), "UPCASE")
$     if (arch .eqs. "") then arch = "UNK"
$ endif
$ define/user openssl [--.include.openssl]
$ cc/def=PTHREADS mttest.c
$ link mttest,[--.'arch'.exe.ssl]libssl/lib,[--.'arch'.exe.crypto]libcrypto/lib

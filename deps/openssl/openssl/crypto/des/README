
		libdes, Version 4.01 10-Jan-97

		Copyright (c) 1997, Eric Young
			  All rights reserved.

    This program is free software; you can redistribute it and/or modify
    it under the terms specified in COPYRIGHT.
    
--
The primary ftp site for this library is
ftp://ftp.psy.uq.oz.au/pub/Crypto/DES/libdes-x.xx.tar.gz
libdes is now also shipped with SSLeay.  Primary ftp site of
ftp://ftp.psy.uq.oz.au/pub/Crypto/SSL/SSLeay-x.x.x.tar.gz

The best way to build this library is to build it as part of SSLeay.

This kit builds a DES encryption library and a DES encryption program.
It supports ecb, cbc, ofb, cfb, triple ecb, triple cbc, triple ofb,
triple cfb, desx, and MIT's pcbc encryption modes and also has a fast
implementation of crypt(3).
It contains support routines to read keys from a terminal,
generate a random key, generate a key from an arbitrary length string,
read/write encrypted data from/to a file descriptor.

The implementation was written so as to conform with the manual entry
for the des_crypt(3) library routines from MIT's project Athena.

destest should be run after compilation to test the des routines.
rpw should be run after compilation to test the read password routines.
The des program is a replacement for the sun des command.  I believe it
conforms to the sun version.

The Imakefile is setup for use in the kerberos distribution.

These routines are best compiled with gcc or any other good
optimising compiler.
Just turn you optimiser up to the highest settings and run destest
after the build to make sure everything works.

I believe these routines are close to the fastest and most portable DES
routines that use small lookup tables (4.5k) that are publicly available.
The fcrypt routine is faster than ufc's fcrypt (when compiling with
gcc2 -O2) on the sparc 2 (1410 vs 1270) but is not so good on other machines
(on a sun3/260 168 vs 336).  It is a function of CPU on chip cache size.
[ 10-Jan-97 and a function of an incorrect speed testing program in
  ufc which gave much better test figures that reality ].

It is worth noting that on sparc and Alpha CPUs, performance of the DES
library can vary by upto %10 due to the positioning of files after application
linkage.

Eric Young (eay@cryptsoft.com)


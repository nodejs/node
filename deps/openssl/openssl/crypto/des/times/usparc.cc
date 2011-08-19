solaris 2.5.1 usparc 167mhz?? - SC4.0 cc -fast -Xa -xO5

For the ultra sparc, SunC 4.0 cc -fast -Xa -xO5, running 'des_opts'
gives a speed of 475,000 des/s while 'speed' gives 417,000 des/s.
I believe the difference is tied up in optimisation that the compiler
is able to perform when the code is 'inlined'.  For 'speed', the DES
routines are being linked from a library.  I'll record the higher
speed since if performance is everything, you can always inline
'des_enc.c'.

[ 16-Jan-06 - I've been playing with the
  '-xtarget=ultra -xarch=v8plus -Xa -xO5 -Xa'
  and while it makes the des_opts numbers much slower, it makes the
  actual 'speed' numbers look better which is a realistic version of
  using the libraries. ]

options    des ecb/s
16 r1 p    475516.90 100.0%
16 r2 p    439388.10  92.4%
16  c i    427001.40  89.8%
16  c p    419516.50  88.2%
 4 r2 p    409491.70  86.1%
 4 r1 p    404266.90  85.0%
 4  c p    398121.00  83.7%
 4  c i    370588.40  77.9%
 4 r1 i    362742.20  76.3%
16 r2 i    331275.50  69.7%
16 r1 i    324730.60  68.3%
 4 r2 i     63535.10  13.4%	<-- very very weird, must be cache problems.
-DDES_UNROLL -DDES_RISC1 -DDES_PTR


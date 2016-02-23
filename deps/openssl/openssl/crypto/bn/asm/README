<OBSOLETE>

All assember in this directory are just version of the file
crypto/bn/bn_asm.c.

Quite a few of these files are just the assember output from gcc since on 
quite a few machines they are 2 times faster than the system compiler.

For the x86, I have hand written assember because of the bad job all
compilers seem to do on it.  This normally gives a 2 time speed up in the RSA
routines.

For the DEC alpha, I also hand wrote the assember (except the division which
is just the output from the C compiler pasted on the end of the file).
On the 2 alpha C compilers I had access to, it was not possible to do
64b x 64b -> 128b calculations (both long and the long long data types
were 64 bits).  So the hand assember gives access to the 128 bit result and
a 2 times speedup :-).

There are 3 versions of assember for the HP PA-RISC.

pa-risc.s is the origional one which works fine and generated using gcc :-)

pa-risc2W.s and pa-risc2.s are 64 and 32-bit PA-RISC 2.0 implementations
by Chris Ruemmler from HP (with some help from the HP C compiler).

</OBSOLETE>

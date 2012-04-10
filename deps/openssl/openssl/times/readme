The 'times' in this directory are not all for the most recent version of
the library and it should be noted that on some CPUs (specifically sparc
and Alpha), the locations of files in the application after linking can
make upto a %10 speed difference when running benchmarks on things like
cbc mode DES.  To put it mildly this can be very anoying.

About the only way to get around this would be to compile the library as one
object file, or to 'include' the source files in a specific order.

The best way to get an idea of the 'raw' DES speed is to build the 
'speed' program in crypto/des.

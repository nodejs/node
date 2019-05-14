Puff -- A Simple Inflate
3 Mar 2003
Mark Adler
madler@alumni.caltech.edu

What this is --

puff.c provides the routine puff() to decompress the deflate data format.  It
does so more slowly than zlib, but the code is about one-fifth the size of the
inflate code in zlib, and written to be very easy to read.

Why I wrote this --

puff.c was written to document the deflate format unambiguously, by virtue of
being working C code.  It is meant to supplement RFC 1951, which formally
describes the deflate format.  I have received many questions on details of the
deflate format, and I hope that reading this code will answer those questions.
puff.c is heavily commented with details of the deflate format, especially
those little nooks and cranies of the format that might not be obvious from a
specification.

puff.c may also be useful in applications where code size or memory usage is a
very limited resource, and speed is not as important.

How to use it --

Well, most likely you should just be reading puff.c and using zlib for actual
applications, but if you must ...

Include puff.h in your code, which provides this prototype:

int puff(unsigned char *dest,           /* pointer to destination pointer */
         unsigned long *destlen,        /* amount of output space */
         unsigned char *source,         /* pointer to source data pointer */
         unsigned long *sourcelen);     /* amount of input available */

Then you can call puff() to decompress a deflate stream that is in memory in
its entirety at source, to a sufficiently sized block of memory for the
decompressed data at dest.  puff() is the only external symbol in puff.c  The
only C library functions that puff.c needs are setjmp() and longjmp(), which
are used to simplify error checking in the code to improve readabilty.  puff.c
does no memory allocation, and uses less than 2K bytes off of the stack.

If destlen is not enough space for the uncompressed data, then inflate will
return an error without writing more than destlen bytes.  Note that this means
that in order to decompress the deflate data successfully, you need to know
the size of the uncompressed data ahead of time.

If needed, puff() can determine the size of the uncompressed data with no
output space.  This is done by passing dest equal to (unsigned char *)0.  Then
the initial value of *destlen is ignored and *destlen is set to the length of
the uncompressed data.  So if the size of the uncompressed data is not known,
then two passes of puff() can be used--first to determine the size, and second
to do the actual inflation after allocating the appropriate memory.  Not
pretty, but it works.  (This is one of the reasons you should be using zlib.)

The deflate format is self-terminating.  If the deflate stream does not end
in *sourcelen bytes, puff() will return an error without reading at or past
endsource.

On return, *sourcelen is updated to the amount of input data consumed, and
*destlen is updated to the size of the uncompressed data.  See the comments
in puff.c for the possible return codes for puff().

librsaref.so is a demonstration dynamic engine that does RSA
operations using the old RSAref 2.0 implementation.

To make proper use of this engine, you must download RSAref 2.0
(search the web for rsaref.tar.Z for example) and unpack it in this
directory, so you'll end up having the subdirectories "install" and
"source" among others.

To build, do the following:

	make

This will list a number of available targets to choose from.  Most of
them are architecture-specific.  The exception is "gnu" which is to be
used on systems where GNU ld and gcc have been installed in such a way
that gcc uses GNU ld to link together programs and shared libraries.

The make file assumes you use gcc.  To change that, just reassign CC:

	make CC=cc

The result is librsaref.so, which you can copy to any place you wish.

del mttest.exe

cl /O2 -DWIN32 /MD -I..\..\out mttest.c /Femttest ..\..\out\ssleay32.lib ..\..\out\libeay32.lib


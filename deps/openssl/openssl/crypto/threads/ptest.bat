del mttest.exe

purify cl /O2 -DWIN32 /MD -I..\..\out mttest.c /Femttest ..\..\out\ssl32.lib ..\..\out\crypt32.lib


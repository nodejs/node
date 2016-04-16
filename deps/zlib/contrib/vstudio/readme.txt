Building instructions for the DLL versions of Zlib 1.2.8
========================================================

This directory contains projects that build zlib and minizip using
Microsoft Visual C++ 9.0/10.0.

You don't need to build these projects yourself. You can download the
binaries from:
  http://www.winimage.com/zLibDll

More information can be found at this site.





Build instructions for Visual Studio 2008 (32 bits or 64 bits)
--------------------------------------------------------------
- Uncompress current zlib, including all contrib/* files
- Compile assembly code (with Visual Studio Command Prompt) by running:
   bld_ml64.bat (in contrib\masmx64)
   bld_ml32.bat (in contrib\masmx86)
- Open contrib\vstudio\vc9\zlibvc.sln with Microsoft Visual C++ 2008
- Or run: vcbuild /rebuild contrib\vstudio\vc9\zlibvc.sln "Release|Win32"

Build instructions for Visual Studio 2010 (32 bits or 64 bits)
--------------------------------------------------------------
- Uncompress current zlib, including all contrib/* files
- Open contrib\vstudio\vc10\zlibvc.sln with Microsoft Visual C++ 2010

Build instructions for Visual Studio 2012 (32 bits or 64 bits)
--------------------------------------------------------------
- Uncompress current zlib, including all contrib/* files
- Open contrib\vstudio\vc11\zlibvc.sln with Microsoft Visual C++ 2012


Important
---------
- To use zlibwapi.dll in your application, you must define the
  macro ZLIB_WINAPI when compiling your application's source files.


Additional notes
----------------
- This DLL, named zlibwapi.dll, is compatible to the old zlib.dll built
  by Gilles Vollant from the zlib 1.1.x sources, and distributed at
    http://www.winimage.com/zLibDll
  It uses the WINAPI calling convention for the exported functions, and
  includes the minizip functionality. If your application needs that
  particular build of zlib.dll, you can rename zlibwapi.dll to zlib.dll.

- The new DLL was renamed because there exist several incompatible
  versions of zlib.dll on the Internet.

- There is also an official DLL build of zlib, named zlib1.dll. This one
  is exporting the functions using the CDECL convention. See the file
  win32\DLL_FAQ.txt found in this zlib distribution.

- There used to be a ZLIB_DLL macro in zlib 1.1.x, but now this symbol
  has a slightly different effect. To avoid compatibility problems, do
  not define it here.


Gilles Vollant
info@winimage.com

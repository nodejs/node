ZLIB DATA COMPRESSION LIBRARY

zlib 1.2.11 is a general purpose data compression library.  All the code is
thread safe.  The data format used by the zlib library is described by RFCs
(Request for Comments) 1950 to 1952 in the files
http://www.ietf.org/rfc/rfc1950.txt (zlib format), rfc1951.txt (deflate format)
and rfc1952.txt (gzip format).

All functions of the compression library are documented in the file zlib.h
(volunteer to write man pages welcome, contact zlib@gzip.org).  Two compiled
examples are distributed in this package, example and minigzip.  The example_d
and minigzip_d flavors validate that the zlib1.dll file is working correctly.

Questions about zlib should be sent to <zlib@gzip.org>.  The zlib home page
is http://zlib.net/ .  Before reporting a problem, please check this site to
verify that you have the latest version of zlib; otherwise get the latest
version and check whether the problem still exists or not.

PLEASE read DLL_FAQ.txt, and the the zlib FAQ http://zlib.net/zlib_faq.html
before asking for help.


Manifest:

The package zlib-1.2.11-win32-x86.zip will contain the following files:

  README-WIN32.txt This document
  ChangeLog        Changes since previous zlib packages
  DLL_FAQ.txt      Frequently asked questions about zlib1.dll
  zlib.3.pdf       Documentation of this library in Adobe Acrobat format

  example.exe      A statically-bound example (using zlib.lib, not the dll)
  example.pdb      Symbolic information for debugging example.exe

  example_d.exe    A zlib1.dll bound example (using zdll.lib)
  example_d.pdb    Symbolic information for debugging example_d.exe

  minigzip.exe     A statically-bound test program (using zlib.lib, not the dll)
  minigzip.pdb     Symbolic information for debugging minigzip.exe

  minigzip_d.exe   A zlib1.dll bound test program (using zdll.lib)
  minigzip_d.pdb   Symbolic information for debugging minigzip_d.exe

  zlib.h           Install these files into the compilers' INCLUDE path to
  zconf.h          compile programs which use zlib.lib or zdll.lib

  zdll.lib         Install these files into the compilers' LIB path if linking
  zdll.exp         a compiled program to the zlib1.dll binary

  zlib.lib         Install these files into the compilers' LIB path to link zlib
  zlib.pdb         into compiled programs, without zlib1.dll runtime dependency
                   (zlib.pdb provides debugging info to the compile time linker)

  zlib1.dll        Install this binary shared library into the system PATH, or
                   the program's runtime directory (where the .exe resides)
  zlib1.pdb        Install in the same directory as zlib1.dll, in order to debug
                   an application crash using WinDbg or similar tools.

All .pdb files above are entirely optional, but are very useful to a developer
attempting to diagnose program misbehavior or a crash.  Many additional
important files for developers can be found in the zlib127.zip source package
available from http://zlib.net/ - review that package's README file for details.


Acknowledgments:

The deflate format used by zlib was defined by Phil Katz.  The deflate and
zlib specifications were written by L.  Peter Deutsch.  Thanks to all the
people who reported problems and suggested various improvements in zlib; they
are too numerous to cite here.


Copyright notice:

  (C) 1995-2017 Jean-loup Gailly and Mark Adler

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.

  Jean-loup Gailly        Mark Adler
  jloup@gzip.org          madler@alumni.caltech.edu

If you use the zlib library in a product, we would appreciate *not* receiving
lengthy legal documents to sign.  The sources are provided for free but without
warranty of any kind.  The library has been entirely written by Jean-loup
Gailly and Mark Adler; it does not include third-party code.

If you redistribute modified sources, we would appreciate that you include in
the file ChangeLog history information documenting your changes.  Please read
the FAQ for more information on the distribution of modified source versions.

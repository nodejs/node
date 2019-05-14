
This directory contains a Pascal (Delphi, Kylix) interface to the
zlib data compression library.


Directory listing
=================

zlibd32.mak     makefile for Borland C++
example.pas     usage example of zlib
zlibpas.pas     the Pascal interface to zlib
readme.txt      this file


Compatibility notes
===================

- Although the name "zlib" would have been more normal for the
  zlibpas unit, this name is already taken by Borland's ZLib unit.
  This is somehow unfortunate, because that unit is not a genuine
  interface to the full-fledged zlib functionality, but a suite of
  class wrappers around zlib streams.  Other essential features,
  such as checksums, are missing.
  It would have been more appropriate for that unit to have a name
  like "ZStreams", or something similar.

- The C and zlib-supplied types int, uInt, long, uLong, etc. are
  translated directly into Pascal types of similar sizes (Integer,
  LongInt, etc.), to avoid namespace pollution.  In particular,
  there is no conversion of unsigned int into a Pascal unsigned
  integer.  The Word type is non-portable and has the same size
  (16 bits) both in a 16-bit and in a 32-bit environment, unlike
  Integer.  Even if there is a 32-bit Cardinal type, there is no
  real need for unsigned int in zlib under a 32-bit environment.

- Except for the callbacks, the zlib function interfaces are
  assuming the calling convention normally used in Pascal
  (__pascal for DOS and Windows16, __fastcall for Windows32).
  Since the cdecl keyword is used, the old Turbo Pascal does
  not work with this interface.

- The gz* function interfaces are not translated, to avoid
  interfacing problems with the C runtime library.  Besides,
    gzprintf(gzFile file, const char *format, ...)
  cannot be translated into Pascal.


Legal issues
============

The zlibpas interface is:
  Copyright (C) 1995-2003 Jean-loup Gailly and Mark Adler.
  Copyright (C) 1998 by Bob Dellaca.
  Copyright (C) 2003 by Cosmin Truta.

The example program is:
  Copyright (C) 1995-2003 by Jean-loup Gailly.
  Copyright (C) 1998,1999,2000 by Jacques Nomssi Nzali.
  Copyright (C) 2003 by Cosmin Truta.

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the author be held liable for any damages
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



Overview
========

This directory contains an update to the ZLib interface unit,
distributed by Borland as a Delphi supplemental component.

The original ZLib unit is Copyright (c) 1997,99 Borland Corp.,
and is based on zlib version 1.0.4.  There are a series of bugs
and security problems associated with that old zlib version, and
we recommend the users to update their ZLib unit.


Summary of modifications
========================

- Improved makefile, adapted to zlib version 1.2.1.

- Some field types from TZStreamRec are changed from Integer to
  Longint, for consistency with the zlib.h header, and for 64-bit
  readiness.

- The zlib_version constant is updated.

- The new Z_RLE strategy has its corresponding symbolic constant.

- The allocation and deallocation functions and function types
  (TAlloc, TFree, zlibAllocMem and zlibFreeMem) are now cdecl,
  and _malloc and _free are added as C RTL stubs.  As a result,
  the original C sources of zlib can be compiled out of the box,
  and linked to the ZLib unit.


Suggestions for improvements
============================

Currently, the ZLib unit provides only a limited wrapper around
the zlib library, and much of the original zlib functionality is
missing.  Handling compressed file formats like ZIP/GZIP or PNG
cannot be implemented without having this functionality.
Applications that handle these formats are either using their own,
duplicated code, or not using the ZLib unit at all.

Here are a few suggestions:

- Checksum class wrappers around adler32() and crc32(), similar
  to the Java classes that implement the java.util.zip.Checksum
  interface.

- The ability to read and write raw deflate streams, without the
  zlib stream header and trailer.  Raw deflate streams are used
  in the ZIP file format.

- The ability to read and write gzip streams, used in the GZIP
  file format, and normally produced by the gzip program.

- The ability to select a different compression strategy, useful
  to PNG and MNG image compression, and to multimedia compression
  in general.  Besides the compression level

    TCompressionLevel = (clNone, clFastest, clDefault, clMax);

  which, in fact, could have used the 'z' prefix and avoided
  TColor-like symbols

    TCompressionLevel = (zcNone, zcFastest, zcDefault, zcMax);

  there could be a compression strategy

    TCompressionStrategy = (zsDefault, zsFiltered, zsHuffmanOnly, zsRle);

- ZIP and GZIP stream handling via TStreams.


--
Cosmin Truta <cosmint@cs.ubbcluj.ro>

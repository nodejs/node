        ZLIB version 1.2.8 for AS400 installation instructions

I) From an AS400 *SAVF file:

1)      Unpacking archive to an AS400 save file

On the AS400:

_       Create the ZLIB AS400 library:

        CRTLIB LIB(ZLIB) TYPE(*PROD) TEXT('ZLIB compression API library')

_       Create a work save file, for example:

                CRTSAVF FILE(ZLIB/ZLIBSAVF)

On a PC connected to the target AS400:

_       Unpack the save file image to a PC file "ZLIBSAVF"
_       Upload this file into the save file on the AS400, for example
                using ftp in BINARY mode.


2)      Populating the ZLIB AS400 source library

On the AS400:

_       Extract the saved objects into the ZLIB AS400 library using:

RSTOBJ OBJ(*ALL) SAVLIB(ZLIB) DEV(*SAVF) SAVF(ZLIB/ZLIBSAVF) RSTLIB(ZLIB)


3)      Customize installation:

_       Edit CL member ZLIB/TOOLS(COMPILE) and change parameters if needed,
                according to the comments.

_       Compile this member with:

        CRTCLPGM PGM(ZLIB/COMPILE) SRCFILE(ZLIB/TOOLS) SRCMBR(COMPILE)


4)      Compile and generate the service program:

_       This can now be done by executing:

        CALL PGM(ZLIB/COMPILE)



II) From the original source distribution:

1)      On the AS400, create the source library:

        CRTLIB LIB(ZLIB) TYPE(*PROD) TEXT('ZLIB compression API library')

2)      Create the source files:

        CRTSRCPF FILE(ZLIB/SOURCES) RCDLEN(112) TEXT('ZLIB library modules')
        CRTSRCPF FILE(ZLIB/H)       RCDLEN(112) TEXT('ZLIB library includes')
        CRTSRCPF FILE(ZLIB/TOOLS)   RCDLEN(112) TEXT('ZLIB library control utilities')

3)      From the machine hosting the distribution files, upload them (with
                FTP in text mode, for example) according to the following table:

    Original    AS400   AS400    AS400 AS400
    file        file    member   type  description
                SOURCES                Original ZLIB C subprogram sources
    adler32.c           ADLER32  C     ZLIB - Compute the Adler-32 checksum of a dta strm
    compress.c          COMPRESS C     ZLIB - Compress a memory buffer
    crc32.c             CRC32    C     ZLIB - Compute the CRC-32 of a data stream
    deflate.c           DEFLATE  C     ZLIB - Compress data using the deflation algorithm
    gzclose.c           GZCLOSE  C     ZLIB - Close .gz files
    gzlib.c             GZLIB    C     ZLIB - Miscellaneous .gz files IO support
    gzread.c            GZREAD   C     ZLIB - Read .gz files
    gzwrite.c           GZWRITE  C     ZLIB - Write .gz files
    infback.c           INFBACK  C     ZLIB - Inflate using a callback interface
    inffast.c           INFFAST  C     ZLIB - Fast proc. literals & length/distance pairs
    inflate.c           INFLATE  C     ZLIB - Interface to inflate modules
    inftrees.c          INFTREES C     ZLIB - Generate Huffman trees for efficient decode
    trees.c             TREES    C     ZLIB - Output deflated data using Huffman coding
    uncompr.c           UNCOMPR  C     ZLIB - Decompress a memory buffer
    zutil.c             ZUTIL    C     ZLIB - Target dependent utility functions
                H                      Original ZLIB C and ILE/RPG include files
    crc32.h             CRC32    C     ZLIB - CRC32 tables
    deflate.h           DEFLATE  C     ZLIB - Internal compression state
    gzguts.h            GZGUTS   C     ZLIB - Definitions for the gzclose module
    inffast.h           INFFAST  C     ZLIB - Header to use inffast.c
    inffixed.h          INFFIXED C     ZLIB - Table for decoding fixed codes
    inflate.h           INFLATE  C     ZLIB - Internal inflate state definitions
    inftrees.h          INFTREES C     ZLIB - Header to use inftrees.c
    trees.h             TREES    C     ZLIB - Created automatically with -DGEN_TREES_H
    zconf.h             ZCONF    C     ZLIB - Compression library configuration
    zlib.h              ZLIB     C     ZLIB - Compression library C user interface
    as400/zlib.inc      ZLIB.INC RPGLE ZLIB - Compression library ILE RPG user interface
    zutil.h             ZUTIL    C     ZLIB - Internal interface and configuration
                TOOLS                  Building source software & AS/400 README
    as400/bndsrc        BNDSRC         Entry point exportation list
    as400/compile.clp   COMPILE  CLP   Compile sources & generate service program
    as400/readme.txt    README   TXT   Installation instructions

4)      Continue as in I)3).




Notes:  For AS400 ILE RPG programmers, a /copy member defining the ZLIB
                API prototypes for ILE RPG can be found in ZLIB/H(ZLIB.INC).
                Please read comments in this member for more information.

        Remember that most foreign textual data are ASCII coded: this
                implementation does not handle conversion from/to ASCII, so
                text data code conversions must be done explicitely.

        Mainly for the reason above, always open zipped files in binary mode.

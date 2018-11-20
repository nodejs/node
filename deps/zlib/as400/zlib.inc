      *  ZLIB.INC - Interface to the general purpose compression library
      *
      *  ILE RPG400 version by Patrick Monnerat, DATASPHERE.
      *  Version 1.2.8
      *
      *
      *  WARNING:
      *     Procedures inflateInit(), inflateInit2(), deflateInit(),
      *         deflateInit2() and inflateBackInit() need to be called with
      *         two additional arguments:
      *         the package version string and the stream control structure.
      *         size. This is needed because RPG lacks some macro feature.
      *         Call these procedures as:
      *             inflateInit(...: ZLIB_VERSION: %size(z_stream))
      *
      /if not defined(ZLIB_H_)
      /define ZLIB_H_
      *
      **************************************************************************
      *                               Constants
      **************************************************************************
      *
      *  Versioning information.
      *
     D ZLIB_VERSION    C                   '1.2.8'
     D ZLIB_VERNUM     C                   X'1280'
     D ZLIB_VER_MAJOR  C                   1
     D ZLIB_VER_MINOR  C                   2
     D ZLIB_VER_REVISION...
     D                 C                   8
     D ZLIB_VER_SUBREVISION...
     D                 C                   0
      *
      *  Other equates.
      *
     D Z_NO_FLUSH      C                   0
     D Z_PARTIAL_FLUSH...
     D                 C                   1
     D Z_SYNC_FLUSH    C                   2
     D Z_FULL_FLUSH    C                   3
     D Z_FINISH        C                   4
     D Z_BLOCK         C                   5
     D Z_TREES         C                   6
      *
     D Z_OK            C                   0
     D Z_STREAM_END    C                   1
     D Z_NEED_DICT     C                   2
     D Z_ERRNO         C                   -1
     D Z_STREAM_ERROR  C                   -2
     D Z_DATA_ERROR    C                   -3
     D Z_MEM_ERROR     C                   -4
     D Z_BUF_ERROR     C                   -5
     DZ_VERSION_ERROR  C                   -6
      *
     D Z_NO_COMPRESSION...
     D                 C                   0
     D Z_BEST_SPEED    C                   1
     D Z_BEST_COMPRESSION...
     D                 C                   9
     D Z_DEFAULT_COMPRESSION...
     D                 C                   -1
      *
     D Z_FILTERED      C                   1
     D Z_HUFFMAN_ONLY  C                   2
     D Z_RLE           C                   3
     D Z_DEFAULT_STRATEGY...
     D                 C                   0
      *
     D Z_BINARY        C                   0
     D Z_ASCII         C                   1
     D Z_UNKNOWN       C                   2
      *
     D Z_DEFLATED      C                   8
      *
     D Z_NULL          C                   0
      *
      **************************************************************************
      *                                 Types
      **************************************************************************
      *
     D z_streamp       S               *                                        Stream struct ptr
     D gzFile          S               *                                        File pointer
     D z_off_t         S             10i 0                                      Stream offsets
     D z_off64_t       S             20i 0                                      Stream offsets
      *
      **************************************************************************
      *                               Structures
      **************************************************************************
      *
      *  The GZIP encode/decode stream support structure.
      *
     D z_stream        DS                  align based(z_streamp)
     D  zs_next_in                     *                                        Next input byte
     D  zs_avail_in                  10U 0                                      Byte cnt at next_in
     D  zs_total_in                  10U 0                                      Total bytes read
     D  zs_next_out                    *                                        Output buffer ptr
     D  zs_avail_out                 10U 0                                      Room left @ next_out
     D  zs_total_out                 10U 0                                      Total bytes written
     D  zs_msg                         *                                        Last errmsg or null
     D  zs_state                       *                                        Internal state
     D  zs_zalloc                      *   procptr                              Int. state allocator
     D  zs_free                        *   procptr                              Int. state dealloc.
     D  zs_opaque                      *                                        Private alloc. data
     D  zs_data_type                 10i 0                                      ASC/BIN best guess
     D  zs_adler                     10u 0                                      Uncompr. adler32 val
     D                               10U 0                                      Reserved
     D                               10U 0                                      Ptr. alignment
      *
      **************************************************************************
      *                     Utility function prototypes
      **************************************************************************
      *
     D compress        PR            10I 0 extproc('compress')
     D  dest                      65535    options(*varsize)                    Destination buffer
     D  destLen                      10U 0                                      Destination length
     D  source                    65535    const options(*varsize)              Source buffer
     D  sourceLen                    10u 0 value                                Source length
      *
     D compress2       PR            10I 0 extproc('compress2')
     D  dest                      65535    options(*varsize)                    Destination buffer
     D  destLen                      10U 0                                      Destination length
     D  source                    65535    const options(*varsize)              Source buffer
     D  sourceLen                    10U 0 value                                Source length
     D  level                        10I 0 value                                Compression level
      *
     D compressBound   PR            10U 0 extproc('compressBound')
     D  sourceLen                    10U 0 value
      *
     D uncompress      PR            10I 0 extproc('uncompress')
     D  dest                      65535    options(*varsize)                    Destination buffer
     D  destLen                      10U 0                                      Destination length
     D  source                    65535    const options(*varsize)              Source buffer
     D  sourceLen                    10U 0 value                                Source length
      *
      /if not defined(LARGE_FILES)
     D gzopen          PR                  extproc('gzopen')
     D                                     like(gzFile)
     D  path                           *   value options(*string)               File pathname
     D  mode                           *   value options(*string)               Open mode
      /else
     D gzopen          PR                  extproc('gzopen64')
     D                                     like(gzFile)
     D  path                           *   value options(*string)               File pathname
     D  mode                           *   value options(*string)               Open mode
      *
     D gzopen64        PR                  extproc('gzopen64')
     D                                     like(gzFile)
     D  path                           *   value options(*string)               File pathname
     D  mode                           *   value options(*string)               Open mode
      /endif
      *
     D gzdopen         PR                  extproc('gzdopen')
     D                                     like(gzFile)
     D  fd                           10I 0 value                                File descriptor
     D  mode                           *   value options(*string)               Open mode
      *
     D gzbuffer        PR            10I 0 extproc('gzbuffer')
     D  file                               value like(gzFile)                   File pointer
     D  size                         10U 0 value
      *
     D gzsetparams     PR            10I 0 extproc('gzsetparams')
     D  file                               value like(gzFile)                   File pointer
     D  level                        10I 0 value
     D  strategy                     10I 0 value
      *
     D gzread          PR            10I 0 extproc('gzread')
     D  file                               value like(gzFile)                   File pointer
     D  buf                       65535    options(*varsize)                    Buffer
     D  len                          10u 0 value                                Buffer length
      *
     D gzwrite         PR            10I 0 extproc('gzwrite')
     D  file                               value like(gzFile)                   File pointer
     D  buf                       65535    const options(*varsize)              Buffer
     D  len                          10u 0 value                                Buffer length
      *
     D gzputs          PR            10I 0 extproc('gzputs')
     D  file                               value like(gzFile)                   File pointer
     D  s                              *   value options(*string)               String to output
      *
     D gzgets          PR              *   extproc('gzgets')
     D  file                               value like(gzFile)                   File pointer
     D  buf                       65535    options(*varsize)                    Read buffer
     D  len                          10i 0 value                                Buffer length
      *
     D gzputc          PR            10i 0 extproc('gzputc')
     D  file                               value like(gzFile)                   File pointer
     D  c                            10I 0 value                                Character to write
      *
     D gzgetc          PR            10i 0 extproc('gzgetc')
     D  file                               value like(gzFile)                   File pointer
      *
     D gzgetc_         PR            10i 0 extproc('gzgetc_')
     D  file                               value like(gzFile)                   File pointer
      *
     D gzungetc        PR            10i 0 extproc('gzungetc')
     D  c                            10I 0 value                                Character to push
     D  file                               value like(gzFile)                   File pointer
      *
     D gzflush         PR            10i 0 extproc('gzflush')
     D  file                               value like(gzFile)                   File pointer
     D  flush                        10I 0 value                                Type of flush
      *
      /if not defined(LARGE_FILES)
     D gzseek          PR                  extproc('gzseek')
     D                                     like(z_off_t)
     D  file                               value like(gzFile)                   File pointer
     D  offset                             value like(z_off_t)                  Offset
     D  whence                       10i 0 value                                Origin
      /else
     D gzseek          PR                  extproc('gzseek64')
     D                                     like(z_off_t)
     D  file                               value like(gzFile)                   File pointer
     D  offset                             value like(z_off_t)                  Offset
     D  whence                       10i 0 value                                Origin
      *
     D gzseek64        PR                  extproc('gzseek64')
     D                                     like(z_off64_t)
     D  file                               value like(gzFile)                   File pointer
     D  offset                             value like(z_off64_t)                Offset
     D  whence                       10i 0 value                                Origin
      /endif
      *
     D gzrewind        PR            10i 0 extproc('gzrewind')
     D  file                               value like(gzFile)                   File pointer
      *
      /if not defined(LARGE_FILES)
     D gztell          PR                  extproc('gztell')
     D                                     like(z_off_t)
     D  file                               value like(gzFile)                   File pointer
      /else
     D gztell          PR                  extproc('gztell64')
     D                                     like(z_off_t)
     D  file                               value like(gzFile)                   File pointer
      *
     D gztell64        PR                  extproc('gztell64')
     D                                     like(z_off64_t)
     D  file                               value like(gzFile)                   File pointer
      /endif
      *
      /if not defined(LARGE_FILES)
     D gzoffset        PR                  extproc('gzoffset')
     D                                     like(z_off_t)
     D  file                               value like(gzFile)                   File pointer
      /else
     D gzoffset        PR                  extproc('gzoffset64')
     D                                     like(z_off_t)
     D  file                               value like(gzFile)                   File pointer
      *
     D gzoffset64      PR                  extproc('gzoffset64')
     D                                     like(z_off64_t)
     D  file                               value like(gzFile)                   File pointer
      /endif
      *
     D gzeof           PR            10i 0 extproc('gzeof')
     D  file                               value like(gzFile)                   File pointer
      *
     D gzclose_r       PR            10i 0 extproc('gzclose_r')
     D  file                               value like(gzFile)                   File pointer
      *
     D gzclose_w       PR            10i 0 extproc('gzclose_w')
     D  file                               value like(gzFile)                   File pointer
      *
     D gzclose         PR            10i 0 extproc('gzclose')
     D  file                               value like(gzFile)                   File pointer
      *
     D gzerror         PR              *   extproc('gzerror')                   Error string
     D  file                               value like(gzFile)                   File pointer
     D  errnum                       10I 0                                      Error code
      *
     D gzclearerr      PR                  extproc('gzclearerr')
     D  file                               value like(gzFile)                   File pointer
      *
      **************************************************************************
      *                        Basic function prototypes
      **************************************************************************
      *
     D zlibVersion     PR              *   extproc('zlibVersion')               Version string
      *
     D deflateInit     PR            10I 0 extproc('deflateInit_')              Init. compression
     D  strm                               like(z_stream)                       Compression stream
     D  level                        10I 0 value                                Compression level
     D  version                        *   value options(*string)               Version string
     D  stream_size                  10i 0 value                                Stream struct. size
      *
     D deflate         PR            10I 0 extproc('deflate')                   Compress data
     D  strm                               like(z_stream)                       Compression stream
     D  flush                        10I 0 value                                Flush type required
      *
     D deflateEnd      PR            10I 0 extproc('deflateEnd')                Termin. compression
     D  strm                               like(z_stream)                       Compression stream
      *
     D inflateInit     PR            10I 0 extproc('inflateInit_')              Init. expansion
     D  strm                               like(z_stream)                       Expansion stream
     D  version                        *   value options(*string)               Version string
     D  stream_size                  10i 0 value                                Stream struct. size
      *
     D inflate         PR            10I 0 extproc('inflate')                   Expand data
     D  strm                               like(z_stream)                       Expansion stream
     D  flush                        10I 0 value                                Flush type required
      *
     D inflateEnd      PR            10I 0 extproc('inflateEnd')                Termin. expansion
     D  strm                               like(z_stream)                       Expansion stream
      *
      **************************************************************************
      *                        Advanced function prototypes
      **************************************************************************
      *
     D deflateInit2    PR            10I 0 extproc('deflateInit2_')             Init. compression
     D  strm                               like(z_stream)                       Compression stream
     D  level                        10I 0 value                                Compression level
     D  method                       10I 0 value                                Compression method
     D  windowBits                   10I 0 value                                log2(window size)
     D  memLevel                     10I 0 value                                Mem/cmpress tradeoff
     D  strategy                     10I 0 value                                Compression stategy
     D  version                        *   value options(*string)               Version string
     D  stream_size                  10i 0 value                                Stream struct. size
      *
     D deflateSetDictionary...
     D                 PR            10I 0 extproc('deflateSetDictionary')      Init. dictionary
     D  strm                               like(z_stream)                       Compression stream
     D  dictionary                65535    const options(*varsize)              Dictionary bytes
     D  dictLength                   10U 0 value                                Dictionary length
      *
     D deflateCopy     PR            10I 0 extproc('deflateCopy')               Compress strm 2 strm
     D  dest                               like(z_stream)                       Destination stream
     D  source                             like(z_stream)                       Source stream
      *
     D deflateReset    PR            10I 0 extproc('deflateReset')              End and init. stream
     D  strm                               like(z_stream)                       Compression stream
      *
     D deflateParams   PR            10I 0 extproc('deflateParams')             Change level & strat
     D  strm                               like(z_stream)                       Compression stream
     D  level                        10I 0 value                                Compression level
     D  strategy                     10I 0 value                                Compression stategy
      *
     D deflateBound    PR            10U 0 extproc('deflateBound')              Change level & strat
     D  strm                               like(z_stream)                       Compression stream
     D  sourcelen                    10U 0 value                                Compression level
      *
     D deflatePending  PR            10I 0 extproc('deflatePending')            Change level & strat
     D  strm                               like(z_stream)                       Compression stream
     D  pending                      10U 0                                      Pending bytes
     D  bits                         10I 0                                      Pending bits
      *
     D deflatePrime    PR            10I 0 extproc('deflatePrime')              Change level & strat
     D  strm                               like(z_stream)                       Compression stream
     D  bits                         10I 0 value                                # of bits to insert
     D  value                        10I 0 value                                Bits to insert
      *
     D inflateInit2    PR            10I 0 extproc('inflateInit2_')             Init. expansion
     D  strm                               like(z_stream)                       Expansion stream
     D  windowBits                   10I 0 value                                log2(window size)
     D  version                        *   value options(*string)               Version string
     D  stream_size                  10i 0 value                                Stream struct. size
      *
     D inflateSetDictionary...
     D                 PR            10I 0 extproc('inflateSetDictionary')      Init. dictionary
     D  strm                               like(z_stream)                       Expansion stream
     D  dictionary                65535    const options(*varsize)              Dictionary bytes
     D  dictLength                   10U 0 value                                Dictionary length
      *
     D inflateGetDictionary...
     D                 PR            10I 0 extproc('inflateGetDictionary')      Get dictionary
     D  strm                               like(z_stream)                       Expansion stream
     D  dictionary                65535    options(*varsize)                    Dictionary bytes
     D  dictLength                   10U 0                                      Dictionary length
      *
     D inflateSync     PR            10I 0 extproc('inflateSync')               Sync. expansion
     D  strm                               like(z_stream)                       Expansion stream
      *
     D inflateCopy     PR            10I 0 extproc('inflateCopy')
     D  dest                               like(z_stream)                       Destination stream
     D  source                             like(z_stream)                       Source stream
      *
     D inflateReset    PR            10I 0 extproc('inflateReset')              End and init. stream
     D  strm                               like(z_stream)                       Expansion stream
      *
     D inflateReset2   PR            10I 0 extproc('inflateReset2')             End and init. stream
     D  strm                               like(z_stream)                       Expansion stream
     D  windowBits                   10I 0 value                                Log2(buffer size)
      *
     D inflatePrime    PR            10I 0 extproc('inflatePrime')              Insert bits
     D  strm                               like(z_stream)                       Expansion stream
     D  bits                         10I 0 value                                Bit count
     D  value                        10I 0 value                                Bits to insert
      *
     D inflateMark     PR            10I 0 extproc('inflateMark')               Get inflate info
     D  strm                               like(z_stream)                       Expansion stream
      *
     D inflateBackInit...
     D                 PR            10I 0 extproc('inflateBackInit_')
     D  strm                               like(z_stream)                       Expansion stream
     D  windowBits                   10I 0 value                                Log2(buffer size)
     D  window                    65535    options(*varsize)                    Buffer
     D  version                        *   value options(*string)               Version string
     D  stream_size                  10i 0 value                                Stream struct. size
      *
     D inflateBack     PR            10I 0 extproc('inflateBack')
     D  strm                               like(z_stream)                       Expansion stream
     D  in                             *   value procptr                        Input function
     D  in_desc                        *   value                                Input descriptor
     D  out                            *   value procptr                        Output function
     D  out_desc                       *   value                                Output descriptor
      *
     D inflateBackEnd  PR            10I 0 extproc('inflateBackEnd')
     D  strm                               like(z_stream)                       Expansion stream
      *
     D zlibCompileFlags...
     D                 PR            10U 0 extproc('zlibCompileFlags')
      *
      **************************************************************************
      *                        Checksum function prototypes
      **************************************************************************
      *
     D adler32         PR            10U 0 extproc('adler32')                   New checksum
     D  adler                        10U 0 value                                Old checksum
     D  buf                       65535    const options(*varsize)              Bytes to accumulate
     D  len                          10U 0 value                                Buffer length
      *
     D crc32           PR            10U 0 extproc('crc32')                     New checksum
     D  crc                          10U 0 value                                Old checksum
     D  buf                       65535    const options(*varsize)              Bytes to accumulate
     D  len                          10U 0 value                                Buffer length
      *
      **************************************************************************
      *                     Miscellaneous function prototypes
      **************************************************************************
      *
     D zError          PR              *   extproc('zError')                    Error string
     D  err                          10I 0 value                                Error code
      *
     D inflateSyncPoint...
     D                 PR            10I 0 extproc('inflateSyncPoint')
     D  strm                               like(z_stream)                       Expansion stream
      *
     D get_crc_table   PR              *   extproc('get_crc_table')             Ptr to ulongs
      *
     D inflateUndermine...
     D                 PR            10I 0 extproc('inflateUndermine')
     D  strm                               like(z_stream)                       Expansion stream
     D  arg                          10I 0 value                                Error code
      *
     D inflateResetKeep...
     D                 PR            10I 0 extproc('inflateResetKeep')          End and init. stream
     D  strm                               like(z_stream)                       Expansion stream
      *
     D deflateResetKeep...
     D                 PR            10I 0 extproc('deflateResetKeep')          End and init. stream
     D  strm                               like(z_stream)                       Expansion stream
      *
      /endif

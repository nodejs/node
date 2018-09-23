(* zlibpas -- Pascal interface to the zlib data compression library
 *
 * Copyright (C) 2003 Cosmin Truta.
 * Derived from original sources by Bob Dellaca.
 * For conditions of distribution and use, see copyright notice in readme.txt
 *)

unit zlibpas;

interface

const
  ZLIB_VERSION = '1.2.8';
  ZLIB_VERNUM  = $1280;

type
  alloc_func = function(opaque: Pointer; items, size: Integer): Pointer;
                 cdecl;
  free_func  = procedure(opaque, address: Pointer);
                 cdecl;

  in_func    = function(opaque: Pointer; var buf: PByte): Integer;
                 cdecl;
  out_func   = function(opaque: Pointer; buf: PByte; size: Integer): Integer;
                 cdecl;

  z_streamp = ^z_stream;
  z_stream = packed record
    next_in: PChar;       (* next input byte *)
    avail_in: Integer;    (* number of bytes available at next_in *)
    total_in: LongInt;    (* total nb of input bytes read so far *)

    next_out: PChar;      (* next output byte should be put there *)
    avail_out: Integer;   (* remaining free space at next_out *)
    total_out: LongInt;   (* total nb of bytes output so far *)

    msg: PChar;           (* last error message, NULL if no error *)
    state: Pointer;       (* not visible by applications *)

    zalloc: alloc_func;   (* used to allocate the internal state *)
    zfree: free_func;     (* used to free the internal state *)
    opaque: Pointer;      (* private data object passed to zalloc and zfree *)

    data_type: Integer;   (* best guess about the data type: ascii or binary *)
    adler: LongInt;       (* adler32 value of the uncompressed data *)
    reserved: LongInt;    (* reserved for future use *)
  end;

  gz_headerp = ^gz_header;
  gz_header = packed record
    text: Integer;        (* true if compressed data believed to be text *)
    time: LongInt;        (* modification time *)
    xflags: Integer;      (* extra flags (not used when writing a gzip file) *)
    os: Integer;          (* operating system *)
    extra: PChar;         (* pointer to extra field or Z_NULL if none *)
    extra_len: Integer;   (* extra field length (valid if extra != Z_NULL) *)
    extra_max: Integer;   (* space at extra (only when reading header) *)
    name: PChar;          (* pointer to zero-terminated file name or Z_NULL *)
    name_max: Integer;    (* space at name (only when reading header) *)
    comment: PChar;       (* pointer to zero-terminated comment or Z_NULL *)
    comm_max: Integer;    (* space at comment (only when reading header) *)
    hcrc: Integer;        (* true if there was or will be a header crc *)
    done: Integer;        (* true when done reading gzip header *)
  end;

(* constants *)
const
  Z_NO_FLUSH      = 0;
  Z_PARTIAL_FLUSH = 1;
  Z_SYNC_FLUSH    = 2;
  Z_FULL_FLUSH    = 3;
  Z_FINISH        = 4;
  Z_BLOCK         = 5;
  Z_TREES         = 6;

  Z_OK            =  0;
  Z_STREAM_END    =  1;
  Z_NEED_DICT     =  2;
  Z_ERRNO         = -1;
  Z_STREAM_ERROR  = -2;
  Z_DATA_ERROR    = -3;
  Z_MEM_ERROR     = -4;
  Z_BUF_ERROR     = -5;
  Z_VERSION_ERROR = -6;

  Z_NO_COMPRESSION       =  0;
  Z_BEST_SPEED           =  1;
  Z_BEST_COMPRESSION     =  9;
  Z_DEFAULT_COMPRESSION  = -1;

  Z_FILTERED            = 1;
  Z_HUFFMAN_ONLY        = 2;
  Z_RLE                 = 3;
  Z_FIXED               = 4;
  Z_DEFAULT_STRATEGY    = 0;

  Z_BINARY   = 0;
  Z_TEXT     = 1;
  Z_ASCII    = 1;
  Z_UNKNOWN  = 2;

  Z_DEFLATED = 8;

(* basic functions *)
function zlibVersion: PChar;
function deflateInit(var strm: z_stream; level: Integer): Integer;
function deflate(var strm: z_stream; flush: Integer): Integer;
function deflateEnd(var strm: z_stream): Integer;
function inflateInit(var strm: z_stream): Integer;
function inflate(var strm: z_stream; flush: Integer): Integer;
function inflateEnd(var strm: z_stream): Integer;

(* advanced functions *)
function deflateInit2(var strm: z_stream; level, method, windowBits,
                      memLevel, strategy: Integer): Integer;
function deflateSetDictionary(var strm: z_stream; const dictionary: PChar;
                              dictLength: Integer): Integer;
function deflateCopy(var dest, source: z_stream): Integer;
function deflateReset(var strm: z_stream): Integer;
function deflateParams(var strm: z_stream; level, strategy: Integer): Integer;
function deflateTune(var strm: z_stream; good_length, max_lazy, nice_length, max_chain: Integer): Integer;
function deflateBound(var strm: z_stream; sourceLen: LongInt): LongInt;
function deflatePending(var strm: z_stream; var pending: Integer; var bits: Integer): Integer;
function deflatePrime(var strm: z_stream; bits, value: Integer): Integer;
function deflateSetHeader(var strm: z_stream; head: gz_header): Integer;
function inflateInit2(var strm: z_stream; windowBits: Integer): Integer;
function inflateSetDictionary(var strm: z_stream; const dictionary: PChar;
                              dictLength: Integer): Integer;
function inflateSync(var strm: z_stream): Integer;
function inflateCopy(var dest, source: z_stream): Integer;
function inflateReset(var strm: z_stream): Integer;
function inflateReset2(var strm: z_stream; windowBits: Integer): Integer;
function inflatePrime(var strm: z_stream; bits, value: Integer): Integer;
function inflateMark(var strm: z_stream): LongInt;
function inflateGetHeader(var strm: z_stream; var head: gz_header): Integer;
function inflateBackInit(var strm: z_stream;
                         windowBits: Integer; window: PChar): Integer;
function inflateBack(var strm: z_stream; in_fn: in_func; in_desc: Pointer;
                     out_fn: out_func; out_desc: Pointer): Integer;
function inflateBackEnd(var strm: z_stream): Integer;
function zlibCompileFlags: LongInt;

(* utility functions *)
function compress(dest: PChar; var destLen: LongInt;
                  const source: PChar; sourceLen: LongInt): Integer;
function compress2(dest: PChar; var destLen: LongInt;
                  const source: PChar; sourceLen: LongInt;
                  level: Integer): Integer;
function compressBound(sourceLen: LongInt): LongInt;
function uncompress(dest: PChar; var destLen: LongInt;
                    const source: PChar; sourceLen: LongInt): Integer;

(* checksum functions *)
function adler32(adler: LongInt; const buf: PChar; len: Integer): LongInt;
function adler32_combine(adler1, adler2, len2: LongInt): LongInt;
function crc32(crc: LongInt; const buf: PChar; len: Integer): LongInt;
function crc32_combine(crc1, crc2, len2: LongInt): LongInt;

(* various hacks, don't look :) *)
function deflateInit_(var strm: z_stream; level: Integer;
                      const version: PChar; stream_size: Integer): Integer;
function inflateInit_(var strm: z_stream; const version: PChar;
                      stream_size: Integer): Integer;
function deflateInit2_(var strm: z_stream;
                       level, method, windowBits, memLevel, strategy: Integer;
                       const version: PChar; stream_size: Integer): Integer;
function inflateInit2_(var strm: z_stream; windowBits: Integer;
                       const version: PChar; stream_size: Integer): Integer;
function inflateBackInit_(var strm: z_stream;
                          windowBits: Integer; window: PChar;
                          const version: PChar; stream_size: Integer): Integer;


implementation

{$L adler32.obj}
{$L compress.obj}
{$L crc32.obj}
{$L deflate.obj}
{$L infback.obj}
{$L inffast.obj}
{$L inflate.obj}
{$L inftrees.obj}
{$L trees.obj}
{$L uncompr.obj}
{$L zutil.obj}

function adler32; external;
function adler32_combine; external;
function compress; external;
function compress2; external;
function compressBound; external;
function crc32; external;
function crc32_combine; external;
function deflate; external;
function deflateBound; external;
function deflateCopy; external;
function deflateEnd; external;
function deflateInit_; external;
function deflateInit2_; external;
function deflateParams; external;
function deflatePending; external;
function deflatePrime; external;
function deflateReset; external;
function deflateSetDictionary; external;
function deflateSetHeader; external;
function deflateTune; external;
function inflate; external;
function inflateBack; external;
function inflateBackEnd; external;
function inflateBackInit_; external;
function inflateCopy; external;
function inflateEnd; external;
function inflateGetHeader; external;
function inflateInit_; external;
function inflateInit2_; external;
function inflateMark; external;
function inflatePrime; external;
function inflateReset; external;
function inflateReset2; external;
function inflateSetDictionary; external;
function inflateSync; external;
function uncompress; external;
function zlibCompileFlags; external;
function zlibVersion; external;

function deflateInit(var strm: z_stream; level: Integer): Integer;
begin
  Result := deflateInit_(strm, level, ZLIB_VERSION, sizeof(z_stream));
end;

function deflateInit2(var strm: z_stream; level, method, windowBits, memLevel,
                      strategy: Integer): Integer;
begin
  Result := deflateInit2_(strm, level, method, windowBits, memLevel, strategy,
                          ZLIB_VERSION, sizeof(z_stream));
end;

function inflateInit(var strm: z_stream): Integer;
begin
  Result := inflateInit_(strm, ZLIB_VERSION, sizeof(z_stream));
end;

function inflateInit2(var strm: z_stream; windowBits: Integer): Integer;
begin
  Result := inflateInit2_(strm, windowBits, ZLIB_VERSION, sizeof(z_stream));
end;

function inflateBackInit(var strm: z_stream;
                         windowBits: Integer; window: PChar): Integer;
begin
  Result := inflateBackInit_(strm, windowBits, window,
                             ZLIB_VERSION, sizeof(z_stream));
end;

function _malloc(Size: Integer): Pointer; cdecl;
begin
  GetMem(Result, Size);
end;

procedure _free(Block: Pointer); cdecl;
begin
  FreeMem(Block);
end;

procedure _memset(P: Pointer; B: Byte; count: Integer); cdecl;
begin
  FillChar(P^, count, B);
end;

procedure _memcpy(dest, source: Pointer; count: Integer); cdecl;
begin
  Move(source^, dest^, count);
end;

end.

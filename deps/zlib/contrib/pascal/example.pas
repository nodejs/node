(* example.c -- usage example of the zlib compression library
 * Copyright (C) 1995-2003 Jean-loup Gailly.
 * For conditions of distribution and use, see copyright notice in zlib.h
 *
 * Pascal translation
 * Copyright (C) 1998 by Jacques Nomssi Nzali.
 * For conditions of distribution and use, see copyright notice in readme.txt
 *
 * Adaptation to the zlibpas interface
 * Copyright (C) 2003 by Cosmin Truta.
 * For conditions of distribution and use, see copyright notice in readme.txt
 *)

program example;

{$DEFINE TEST_COMPRESS}
{DO NOT $DEFINE TEST_GZIO}
{$DEFINE TEST_DEFLATE}
{$DEFINE TEST_INFLATE}
{$DEFINE TEST_FLUSH}
{$DEFINE TEST_SYNC}
{$DEFINE TEST_DICT}

uses SysUtils, zlibpas;

const TESTFILE = 'foo.gz';

(* "hello world" would be more standard, but the repeated "hello"
 * stresses the compression code better, sorry...
 *)
const hello: PChar = 'hello, hello!';

const dictionary: PChar = 'hello';

var dictId: LongInt; (* Adler32 value of the dictionary *)

procedure CHECK_ERR(err: Integer; msg: String);
begin
  if err <> Z_OK then
  begin
    WriteLn(msg, ' error: ', err);
    Halt(1);
  end;
end;

procedure EXIT_ERR(const msg: String);
begin
  WriteLn('Error: ', msg);
  Halt(1);
end;

(* ===========================================================================
 * Test compress and uncompress
 *)
{$IFDEF TEST_COMPRESS}
procedure test_compress(compr: Pointer; comprLen: LongInt;
                        uncompr: Pointer; uncomprLen: LongInt);
var err: Integer;
    len: LongInt;
begin
  len := StrLen(hello)+1;

  err := compress(compr, comprLen, hello, len);
  CHECK_ERR(err, 'compress');

  StrCopy(PChar(uncompr), 'garbage');

  err := uncompress(uncompr, uncomprLen, compr, comprLen);
  CHECK_ERR(err, 'uncompress');

  if StrComp(PChar(uncompr), hello) <> 0 then
    EXIT_ERR('bad uncompress')
  else
    WriteLn('uncompress(): ', PChar(uncompr));
end;
{$ENDIF}

(* ===========================================================================
 * Test read/write of .gz files
 *)
{$IFDEF TEST_GZIO}
procedure test_gzio(const fname: PChar; (* compressed file name *)
                    uncompr: Pointer;
                    uncomprLen: LongInt);
var err: Integer;
    len: Integer;
    zfile: gzFile;
    pos: LongInt;
begin
  len := StrLen(hello)+1;

  zfile := gzopen(fname, 'wb');
  if zfile = NIL then
  begin
    WriteLn('gzopen error');
    Halt(1);
  end;
  gzputc(zfile, 'h');
  if gzputs(zfile, 'ello') <> 4 then
  begin
    WriteLn('gzputs err: ', gzerror(zfile, err));
    Halt(1);
  end;
  {$IFDEF GZ_FORMAT_STRING}
  if gzprintf(zfile, ', %s!', 'hello') <> 8 then
  begin
    WriteLn('gzprintf err: ', gzerror(zfile, err));
    Halt(1);
  end;
  {$ELSE}
  if gzputs(zfile, ', hello!') <> 8 then
  begin
    WriteLn('gzputs err: ', gzerror(zfile, err));
    Halt(1);
  end;
  {$ENDIF}
  gzseek(zfile, 1, SEEK_CUR); (* add one zero byte *)
  gzclose(zfile);

  zfile := gzopen(fname, 'rb');
  if zfile = NIL then
  begin
    WriteLn('gzopen error');
    Halt(1);
  end;

  StrCopy(PChar(uncompr), 'garbage');

  if gzread(zfile, uncompr, uncomprLen) <> len then
  begin
    WriteLn('gzread err: ', gzerror(zfile, err));
    Halt(1);
  end;
  if StrComp(PChar(uncompr), hello) <> 0 then
  begin
    WriteLn('bad gzread: ', PChar(uncompr));
    Halt(1);
  end
  else
    WriteLn('gzread(): ', PChar(uncompr));

  pos := gzseek(zfile, -8, SEEK_CUR);
  if (pos <> 6) or (gztell(zfile) <> pos) then
  begin
    WriteLn('gzseek error, pos=', pos, ', gztell=', gztell(zfile));
    Halt(1);
  end;

  if gzgetc(zfile) <> ' ' then
  begin
    WriteLn('gzgetc error');
    Halt(1);
  end;

  if gzungetc(' ', zfile) <> ' ' then
  begin
    WriteLn('gzungetc error');
    Halt(1);
  end;

  gzgets(zfile, PChar(uncompr), uncomprLen);
  uncomprLen := StrLen(PChar(uncompr));
  if uncomprLen <> 7 then (* " hello!" *)
  begin
    WriteLn('gzgets err after gzseek: ', gzerror(zfile, err));
    Halt(1);
  end;
  if StrComp(PChar(uncompr), hello + 6) <> 0 then
  begin
    WriteLn('bad gzgets after gzseek');
    Halt(1);
  end
  else
    WriteLn('gzgets() after gzseek: ', PChar(uncompr));

  gzclose(zfile);
end;
{$ENDIF}

(* ===========================================================================
 * Test deflate with small buffers
 *)
{$IFDEF TEST_DEFLATE}
procedure test_deflate(compr: Pointer; comprLen: LongInt);
var c_stream: z_stream; (* compression stream *)
    err: Integer;
    len: LongInt;
begin
  len := StrLen(hello)+1;

  c_stream.zalloc := NIL;
  c_stream.zfree := NIL;
  c_stream.opaque := NIL;

  err := deflateInit(c_stream, Z_DEFAULT_COMPRESSION);
  CHECK_ERR(err, 'deflateInit');

  c_stream.next_in := hello;
  c_stream.next_out := compr;

  while (c_stream.total_in <> len) and
        (c_stream.total_out < comprLen) do
  begin
    c_stream.avail_out := 1; { force small buffers }
    c_stream.avail_in := 1;
    err := deflate(c_stream, Z_NO_FLUSH);
    CHECK_ERR(err, 'deflate');
  end;

  (* Finish the stream, still forcing small buffers: *)
  while TRUE do
  begin
    c_stream.avail_out := 1;
    err := deflate(c_stream, Z_FINISH);
    if err = Z_STREAM_END then
      break;
    CHECK_ERR(err, 'deflate');
  end;

  err := deflateEnd(c_stream);
  CHECK_ERR(err, 'deflateEnd');
end;
{$ENDIF}

(* ===========================================================================
 * Test inflate with small buffers
 *)
{$IFDEF TEST_INFLATE}
procedure test_inflate(compr: Pointer; comprLen : LongInt;
                       uncompr: Pointer; uncomprLen : LongInt);
var err: Integer;
    d_stream: z_stream; (* decompression stream *)
begin
  StrCopy(PChar(uncompr), 'garbage');

  d_stream.zalloc := NIL;
  d_stream.zfree := NIL;
  d_stream.opaque := NIL;

  d_stream.next_in := compr;
  d_stream.avail_in := 0;
  d_stream.next_out := uncompr;

  err := inflateInit(d_stream);
  CHECK_ERR(err, 'inflateInit');

  while (d_stream.total_out < uncomprLen) and
        (d_stream.total_in < comprLen) do
  begin
    d_stream.avail_out := 1; (* force small buffers *)
    d_stream.avail_in := 1;
    err := inflate(d_stream, Z_NO_FLUSH);
    if err = Z_STREAM_END then
      break;
    CHECK_ERR(err, 'inflate');
  end;

  err := inflateEnd(d_stream);
  CHECK_ERR(err, 'inflateEnd');

  if StrComp(PChar(uncompr), hello) <> 0 then
    EXIT_ERR('bad inflate')
  else
    WriteLn('inflate(): ', PChar(uncompr));
end;
{$ENDIF}

(* ===========================================================================
 * Test deflate with large buffers and dynamic change of compression level
 *)
{$IFDEF TEST_DEFLATE}
procedure test_large_deflate(compr: Pointer; comprLen: LongInt;
                             uncompr: Pointer; uncomprLen: LongInt);
var c_stream: z_stream; (* compression stream *)
    err: Integer;
begin
  c_stream.zalloc := NIL;
  c_stream.zfree := NIL;
  c_stream.opaque := NIL;

  err := deflateInit(c_stream, Z_BEST_SPEED);
  CHECK_ERR(err, 'deflateInit');

  c_stream.next_out := compr;
  c_stream.avail_out := Integer(comprLen);

  (* At this point, uncompr is still mostly zeroes, so it should compress
   * very well:
   *)
  c_stream.next_in := uncompr;
  c_stream.avail_in := Integer(uncomprLen);
  err := deflate(c_stream, Z_NO_FLUSH);
  CHECK_ERR(err, 'deflate');
  if c_stream.avail_in <> 0 then
    EXIT_ERR('deflate not greedy');

  (* Feed in already compressed data and switch to no compression: *)
  deflateParams(c_stream, Z_NO_COMPRESSION, Z_DEFAULT_STRATEGY);
  c_stream.next_in := compr;
  c_stream.avail_in := Integer(comprLen div 2);
  err := deflate(c_stream, Z_NO_FLUSH);
  CHECK_ERR(err, 'deflate');

  (* Switch back to compressing mode: *)
  deflateParams(c_stream, Z_BEST_COMPRESSION, Z_FILTERED);
  c_stream.next_in := uncompr;
  c_stream.avail_in := Integer(uncomprLen);
  err := deflate(c_stream, Z_NO_FLUSH);
  CHECK_ERR(err, 'deflate');

  err := deflate(c_stream, Z_FINISH);
  if err <> Z_STREAM_END then
    EXIT_ERR('deflate should report Z_STREAM_END');

  err := deflateEnd(c_stream);
  CHECK_ERR(err, 'deflateEnd');
end;
{$ENDIF}

(* ===========================================================================
 * Test inflate with large buffers
 *)
{$IFDEF TEST_INFLATE}
procedure test_large_inflate(compr: Pointer; comprLen: LongInt;
                             uncompr: Pointer; uncomprLen: LongInt);
var err: Integer;
    d_stream: z_stream; (* decompression stream *)
begin
  StrCopy(PChar(uncompr), 'garbage');

  d_stream.zalloc := NIL;
  d_stream.zfree := NIL;
  d_stream.opaque := NIL;

  d_stream.next_in := compr;
  d_stream.avail_in := Integer(comprLen);

  err := inflateInit(d_stream);
  CHECK_ERR(err, 'inflateInit');

  while TRUE do
  begin
    d_stream.next_out := uncompr;            (* discard the output *)
    d_stream.avail_out := Integer(uncomprLen);
    err := inflate(d_stream, Z_NO_FLUSH);
    if err = Z_STREAM_END then
      break;
    CHECK_ERR(err, 'large inflate');
  end;

  err := inflateEnd(d_stream);
  CHECK_ERR(err, 'inflateEnd');

  if d_stream.total_out <> 2 * uncomprLen + comprLen div 2 then
  begin
    WriteLn('bad large inflate: ', d_stream.total_out);
    Halt(1);
  end
  else
    WriteLn('large_inflate(): OK');
end;
{$ENDIF}

(* ===========================================================================
 * Test deflate with full flush
 *)
{$IFDEF TEST_FLUSH}
procedure test_flush(compr: Pointer; var comprLen : LongInt);
var c_stream: z_stream; (* compression stream *)
    err: Integer;
    len: Integer;
begin
  len := StrLen(hello)+1;

  c_stream.zalloc := NIL;
  c_stream.zfree := NIL;
  c_stream.opaque := NIL;

  err := deflateInit(c_stream, Z_DEFAULT_COMPRESSION);
  CHECK_ERR(err, 'deflateInit');

  c_stream.next_in := hello;
  c_stream.next_out := compr;
  c_stream.avail_in := 3;
  c_stream.avail_out := Integer(comprLen);
  err := deflate(c_stream, Z_FULL_FLUSH);
  CHECK_ERR(err, 'deflate');

  Inc(PByteArray(compr)^[3]); (* force an error in first compressed block *)
  c_stream.avail_in := len - 3;

  err := deflate(c_stream, Z_FINISH);
  if err <> Z_STREAM_END then
    CHECK_ERR(err, 'deflate');

  err := deflateEnd(c_stream);
  CHECK_ERR(err, 'deflateEnd');

  comprLen := c_stream.total_out;
end;
{$ENDIF}

(* ===========================================================================
 * Test inflateSync()
 *)
{$IFDEF TEST_SYNC}
procedure test_sync(compr: Pointer; comprLen: LongInt;
                    uncompr: Pointer; uncomprLen : LongInt);
var err: Integer;
    d_stream: z_stream; (* decompression stream *)
begin
  StrCopy(PChar(uncompr), 'garbage');

  d_stream.zalloc := NIL;
  d_stream.zfree := NIL;
  d_stream.opaque := NIL;

  d_stream.next_in := compr;
  d_stream.avail_in := 2; (* just read the zlib header *)

  err := inflateInit(d_stream);
  CHECK_ERR(err, 'inflateInit');

  d_stream.next_out := uncompr;
  d_stream.avail_out := Integer(uncomprLen);

  inflate(d_stream, Z_NO_FLUSH);
  CHECK_ERR(err, 'inflate');

  d_stream.avail_in := Integer(comprLen-2);   (* read all compressed data *)
  err := inflateSync(d_stream);               (* but skip the damaged part *)
  CHECK_ERR(err, 'inflateSync');

  err := inflate(d_stream, Z_FINISH);
  if err <> Z_DATA_ERROR then
    EXIT_ERR('inflate should report DATA_ERROR');
    (* Because of incorrect adler32 *)

  err := inflateEnd(d_stream);
  CHECK_ERR(err, 'inflateEnd');

  WriteLn('after inflateSync(): hel', PChar(uncompr));
end;
{$ENDIF}

(* ===========================================================================
 * Test deflate with preset dictionary
 *)
{$IFDEF TEST_DICT}
procedure test_dict_deflate(compr: Pointer; comprLen: LongInt);
var c_stream: z_stream; (* compression stream *)
    err: Integer;
begin
  c_stream.zalloc := NIL;
  c_stream.zfree := NIL;
  c_stream.opaque := NIL;

  err := deflateInit(c_stream, Z_BEST_COMPRESSION);
  CHECK_ERR(err, 'deflateInit');

  err := deflateSetDictionary(c_stream, dictionary, StrLen(dictionary));
  CHECK_ERR(err, 'deflateSetDictionary');

  dictId := c_stream.adler;
  c_stream.next_out := compr;
  c_stream.avail_out := Integer(comprLen);

  c_stream.next_in := hello;
  c_stream.avail_in := StrLen(hello)+1;

  err := deflate(c_stream, Z_FINISH);
  if err <> Z_STREAM_END then
    EXIT_ERR('deflate should report Z_STREAM_END');

  err := deflateEnd(c_stream);
  CHECK_ERR(err, 'deflateEnd');
end;
{$ENDIF}

(* ===========================================================================
 * Test inflate with a preset dictionary
 *)
{$IFDEF TEST_DICT}
procedure test_dict_inflate(compr: Pointer; comprLen: LongInt;
                            uncompr: Pointer; uncomprLen: LongInt);
var err: Integer;
    d_stream: z_stream; (* decompression stream *)
begin
  StrCopy(PChar(uncompr), 'garbage');

  d_stream.zalloc := NIL;
  d_stream.zfree := NIL;
  d_stream.opaque := NIL;

  d_stream.next_in := compr;
  d_stream.avail_in := Integer(comprLen);

  err := inflateInit(d_stream);
  CHECK_ERR(err, 'inflateInit');

  d_stream.next_out := uncompr;
  d_stream.avail_out := Integer(uncomprLen);

  while TRUE do
  begin
    err := inflate(d_stream, Z_NO_FLUSH);
    if err = Z_STREAM_END then
      break;
    if err = Z_NEED_DICT then
    begin
      if d_stream.adler <> dictId then
        EXIT_ERR('unexpected dictionary');
      err := inflateSetDictionary(d_stream, dictionary, StrLen(dictionary));
    end;
    CHECK_ERR(err, 'inflate with dict');
  end;

  err := inflateEnd(d_stream);
  CHECK_ERR(err, 'inflateEnd');

  if StrComp(PChar(uncompr), hello) <> 0 then
    EXIT_ERR('bad inflate with dict')
  else
    WriteLn('inflate with dictionary: ', PChar(uncompr));
end;
{$ENDIF}

var compr, uncompr: Pointer;
    comprLen, uncomprLen: LongInt;

begin
  if zlibVersion^ <> ZLIB_VERSION[1] then
    EXIT_ERR('Incompatible zlib version');

  WriteLn('zlib version: ', zlibVersion);
  WriteLn('zlib compile flags: ', Format('0x%x', [zlibCompileFlags]));

  comprLen := 10000 * SizeOf(Integer); (* don't overflow on MSDOS *)
  uncomprLen := comprLen;
  GetMem(compr, comprLen);
  GetMem(uncompr, uncomprLen);
  if (compr = NIL) or (uncompr = NIL) then
    EXIT_ERR('Out of memory');
  (* compr and uncompr are cleared to avoid reading uninitialized
   * data and to ensure that uncompr compresses well.
   *)
  FillChar(compr^, comprLen, 0);
  FillChar(uncompr^, uncomprLen, 0);

  {$IFDEF TEST_COMPRESS}
  WriteLn('** Testing compress');
  test_compress(compr, comprLen, uncompr, uncomprLen);
  {$ENDIF}

  {$IFDEF TEST_GZIO}
  WriteLn('** Testing gzio');
  if ParamCount >= 1 then
    test_gzio(ParamStr(1), uncompr, uncomprLen)
  else
    test_gzio(TESTFILE, uncompr, uncomprLen);
  {$ENDIF}

  {$IFDEF TEST_DEFLATE}
  WriteLn('** Testing deflate with small buffers');
  test_deflate(compr, comprLen);
  {$ENDIF}
  {$IFDEF TEST_INFLATE}
  WriteLn('** Testing inflate with small buffers');
  test_inflate(compr, comprLen, uncompr, uncomprLen);
  {$ENDIF}

  {$IFDEF TEST_DEFLATE}
  WriteLn('** Testing deflate with large buffers');
  test_large_deflate(compr, comprLen, uncompr, uncomprLen);
  {$ENDIF}
  {$IFDEF TEST_INFLATE}
  WriteLn('** Testing inflate with large buffers');
  test_large_inflate(compr, comprLen, uncompr, uncomprLen);
  {$ENDIF}

  {$IFDEF TEST_FLUSH}
  WriteLn('** Testing deflate with full flush');
  test_flush(compr, comprLen);
  {$ENDIF}
  {$IFDEF TEST_SYNC}
  WriteLn('** Testing inflateSync');
  test_sync(compr, comprLen, uncompr, uncomprLen);
  {$ENDIF}
  comprLen := uncomprLen;

  {$IFDEF TEST_DICT}
  WriteLn('** Testing deflate and inflate with preset dictionary');
  test_dict_deflate(compr, comprLen);
  test_dict_inflate(compr, comprLen, uncompr, uncomprLen);
  {$ENDIF}

  FreeMem(compr, comprLen);
  FreeMem(uncompr, uncomprLen);
end.

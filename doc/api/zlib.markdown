# Zlib

    Stability: 3 - Stable

You can access this module with:

    var zlib = require('zlib');

This provides bindings to Gzip/Gunzip, Deflate/Inflate, and
DeflateRaw/InflateRaw classes.  Each class takes the same options, and
is a readable/writable Stream.

## Examples

Compressing or decompressing a file can be done by piping an
fs.ReadStream into a zlib stream, then into an fs.WriteStream.

    var gzip = zlib.createGzip();
    var fs = require('fs');
    var inp = fs.createReadStream('input.txt');
    var out = fs.createWriteStream('input.txt.gz');

    inp.pipe(gzip).pipe(out);

Compressing or decompressing data in one step can be done by using
the convenience methods.

    var input = '.................................';
    zlib.deflate(input, function(err, buffer) {
      if (!err) {
        console.log(buffer.toString('base64'));
      }
    });

    var buffer = new Buffer('eJzT0yMAAGTvBe8=', 'base64');
    zlib.unzip(buffer, function(err, buffer) {
      if (!err) {
        console.log(buffer.toString());
      }
    });

To use this module in an HTTP client or server, use the
[accept-encoding](http://www.w3.org/Protocols/rfc2616/rfc2616-sec14.html#sec14.3)
on requests, and the
[content-encoding](http://www.w3.org/Protocols/rfc2616/rfc2616-sec14.html#sec14.11)
header on responses.

**Note: these examples are drastically simplified to show
the basic concept.**  Zlib encoding can be expensive, and the results
ought to be cached.  See [Memory Usage Tuning](#zlib_memory_usage_tuning)
below for more information on the speed/memory/compression
tradeoffs involved in zlib usage.

    // client request example
    var zlib = require('zlib');
    var http = require('http');
    var fs = require('fs');
    var request = http.get({ host: 'izs.me',
                             path: '/',
                             port: 80,
                             headers: { 'accept-encoding': 'gzip,deflate' } });
    request.on('response', function(response) {
      var output = fs.createWriteStream('izs.me_index.html');

      switch (response.headers['content-encoding']) {
        // or, just use zlib.createUnzip() to handle both cases
        case 'gzip':
          response.pipe(zlib.createGunzip()).pipe(output);
          break;
        case 'deflate':
          response.pipe(zlib.createInflate()).pipe(output);
          break;
        default:
          response.pipe(output);
          break;
      }
    });

    // server example
    // Running a gzip operation on every request is quite expensive.
    // It would be much more efficient to cache the compressed buffer.
    var zlib = require('zlib');
    var http = require('http');
    var fs = require('fs');
    http.createServer(function(request, response) {
      var raw = fs.createReadStream('index.html');
      var acceptEncoding = request.headers['accept-encoding'];
      if (!acceptEncoding) {
        acceptEncoding = '';
      }

      // Note: this is not a conformant accept-encoding parser.
      // See http://www.w3.org/Protocols/rfc2616/rfc2616-sec14.html#sec14.3
      if (acceptEncoding.match(/\bdeflate\b/)) {
        response.writeHead(200, { 'content-encoding': 'deflate' });
        raw.pipe(zlib.createDeflate()).pipe(response);
      } else if (acceptEncoding.match(/\bgzip\b/)) {
        response.writeHead(200, { 'content-encoding': 'gzip' });
        raw.pipe(zlib.createGzip()).pipe(response);
      } else {
        response.writeHead(200, {});
        raw.pipe(response);
      }
    }).listen(1337);

## zlib.createGzip([options])

Returns a new [Gzip](#zlib_class_zlib_gzip) object with an
[options](#zlib_options).

## zlib.createGunzip([options])

Returns a new [Gunzip](#zlib_class_zlib_gunzip) object with an
[options](#zlib_options).

## zlib.createDeflate([options])

Returns a new [Deflate](#zlib_class_zlib_deflate) object with an
[options](#zlib_options).

## zlib.createInflate([options])

Returns a new [Inflate](#zlib_class_zlib_inflate) object with an
[options](#zlib_options).

## zlib.createDeflateRaw([options])

Returns a new [DeflateRaw](#zlib_class_zlib_deflateraw) object with an
[options](#zlib_options).

## zlib.createInflateRaw([options])

Returns a new [InflateRaw](#zlib_class_zlib_inflateraw) object with an
[options](#zlib_options).

## zlib.createUnzip([options])

Returns a new [Unzip](#zlib_class_zlib_unzip) object with an
[options](#zlib_options).


## Class: zlib.Gzip

Compress data using gzip.

## Class: zlib.Gunzip

Decompress a gzip stream.

## Class: zlib.Deflate

Compress data using deflate.

## Class: zlib.Inflate

Decompress a deflate stream.

## Class: zlib.DeflateRaw

Compress data using deflate, and do not append a zlib header.

## Class: zlib.InflateRaw

Decompress a raw deflate stream.

## Class: zlib.Unzip

Decompress either a Gzip- or Deflate-compressed stream by auto-detecting
the header.

## Convenience Methods

<!--type=misc-->

All of these take a string or buffer as the first argument, and call the
supplied callback with `callback(error, result)`.  The
compression/decompression engine is created using the default settings
in all convenience methods.  To supply different options, use the
zlib classes directly.

## zlib.deflate(buf, callback)

Compress a string with Deflate.

## zlib.deflateRaw(buf, callback)

Compress a string with DeflateRaw.

## zlib.gzip(buf, callback)

Compress a string with Gzip.

## zlib.gunzip(buf, callback)

Decompress a raw Buffer with Gunzip.

## zlib.inflate(buf, callback)

Decompress a raw Buffer with Inflate.

## zlib.inflateRaw(buf, callback)

Decompress a raw Buffer with InflateRaw.

## zlib.unzip(buf, callback)

Decompress a raw Buffer with Unzip.

## Options

<!--type=misc-->

Each class takes an options object.  All options are optional.  (The
convenience methods use the default settings for all options.)

Note that some options are only
relevant when compressing, and are ignored by the decompression classes.

* chunkSize (default: 16*1024)
* windowBits
* level (compression only)
* memLevel (compression only)
* strategy (compression only)
* dictionary (deflate/inflate only, empty dictionary by default)

See the description of `deflateInit2` and `inflateInit2`
at <http://zlib.net/manual.html#Advanced> for more information on these.

## Memory Usage Tuning

<!--type=misc-->

From `zlib/zconf.h`, modified to node's usage:

The memory requirements for deflate are (in bytes):

    (1 << (windowBits+2)) +  (1 << (memLevel+9))

that is: 128K for windowBits=15  +  128K for memLevel = 8
(default values) plus a few kilobytes for small objects.

For example, if you want to reduce
the default memory requirements from 256K to 128K, set the options to:

    { windowBits: 14, memLevel: 7 }

Of course this will generally degrade compression (there's no free lunch).

The memory requirements for inflate are (in bytes)

    1 << windowBits

that is, 32K for windowBits=15 (default value) plus a few kilobytes
for small objects.

This is in addition to a single internal output slab buffer of size
`chunkSize`, which defaults to 16K.

The speed of zlib compression is affected most dramatically by the
`level` setting.  A higher level will result in better compression, but
will take longer to complete.  A lower level will result in less
compression, but will be much faster.

In general, greater memory usage options will mean that node has to make
fewer calls to zlib, since it'll be able to process more data in a
single `write` operation.  So, this is another factor that affects the
speed, at the cost of memory usage.

## Constants

<!--type=misc-->

All of the constants defined in zlib.h are also defined on
`require('zlib')`.
In the normal course of operations, you will not need to ever set any of
these.  They are documented here so that their presence is not
surprising.  This section is taken almost directly from the [zlib
documentation](http://zlib.net/manual.html#Constants).  See
<http://zlib.net/manual.html#Constants> for more details.

Allowed flush values.

* `zlib.Z_NO_FLUSH`
* `zlib.Z_PARTIAL_FLUSH`
* `zlib.Z_SYNC_FLUSH`
* `zlib.Z_FULL_FLUSH`
* `zlib.Z_FINISH`
* `zlib.Z_BLOCK`
* `zlib.Z_TREES`

Return codes for the compression/decompression functions. Negative
values are errors, positive values are used for special but normal
events.

* `zlib.Z_OK`
* `zlib.Z_STREAM_END`
* `zlib.Z_NEED_DICT`
* `zlib.Z_ERRNO`
* `zlib.Z_STREAM_ERROR`
* `zlib.Z_DATA_ERROR`
* `zlib.Z_MEM_ERROR`
* `zlib.Z_BUF_ERROR`
* `zlib.Z_VERSION_ERROR`

Compression levels.

* `zlib.Z_NO_COMPRESSION`
* `zlib.Z_BEST_SPEED`
* `zlib.Z_BEST_COMPRESSION`
* `zlib.Z_DEFAULT_COMPRESSION`

Compression strategy.

* `zlib.Z_FILTERED`
* `zlib.Z_HUFFMAN_ONLY`
* `zlib.Z_RLE`
* `zlib.Z_FIXED`
* `zlib.Z_DEFAULT_STRATEGY`

Possible values of the data_type field.

* `zlib.Z_BINARY`
* `zlib.Z_TEXT`
* `zlib.Z_ASCII`
* `zlib.Z_UNKNOWN`

The deflate compression method (the only one supported in this version).

* `zlib.Z_DEFLATED`

For initializing zalloc, zfree, opaque.

* `zlib.Z_NULL`

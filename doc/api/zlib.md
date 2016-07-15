# Zlib

> Stability: 2 - Stable

The `zlib` module provides compression functionality implemented using Gzip and
Deflate/Inflate. It can be accessed using:

```js
const zlib = require('zlib');
```

Compressing or decompressing a stream (such as a file) can be accomplished by
piping the source stream data through a `zlib` stream into a destination stream:

```js
const gzip = zlib.createGzip();
const fs = require('fs');
const inp = fs.createReadStream('input.txt');
const out = fs.createWriteStream('input.txt.gz');

inp.pipe(gzip).pipe(out);
```

It is also possible to compress or decompress data in a single step:

```js
const input = '.................................';
zlib.deflate(input, (err, buffer) => {
  if (!err) {
    console.log(buffer.toString('base64'));
  } else {
    // handle error
  }
});

const buffer = Buffer.from('eJzT0yMAAGTvBe8=', 'base64');
zlib.unzip(buffer, (err, buffer) => {
  if (!err) {
    console.log(buffer.toString());
  } else {
    // handle error
  }
});
```

## Compressing HTTP requests and responses

The `zlib` module can be used to implement support for the `gzip` and `deflate`
content-encoding mechanisms defined by
[HTTP](https://tools.ietf.org/html/rfc7230#section-4.2).

The HTTP [`Accept-Encoding`][] header is used within an http request to identify
the compression encodings accepted by the client. The [`Content-Encoding`][]
header is used to identify the compression encodings actually applied to a
message.

**Note: the examples given below are drastically simplified to show
the basic concept.**  Using `zlib` encoding can be expensive, and the results
ought to be cached.  See [Memory Usage Tuning][] for more information
on the speed/memory/compression tradeoffs involved in `zlib` usage.

```js
// client request example
const zlib = require('zlib');
const http = require('http');
const fs = require('fs');
const request = http.get({ host: 'example.com',
                         path: '/',
                         port: 80,
                         headers: { 'Accept-Encoding': 'gzip,deflate' } });
request.on('response', (response) => {
  var output = fs.createWriteStream('example.com_index.html');

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
const zlib = require('zlib');
const http = require('http');
const fs = require('fs');
http.createServer((request, response) => {
  var raw = fs.createReadStream('index.html');
  var acceptEncoding = request.headers['accept-encoding'];
  if (!acceptEncoding) {
    acceptEncoding = '';
  }

  // Note: this is not a conformant accept-encoding parser.
  // See http://www.w3.org/Protocols/rfc2616/rfc2616-sec14.html#sec14.3
  if (acceptEncoding.match(/\bdeflate\b/)) {
    response.writeHead(200, { 'Content-Encoding': 'deflate' });
    raw.pipe(zlib.createDeflate()).pipe(response);
  } else if (acceptEncoding.match(/\bgzip\b/)) {
    response.writeHead(200, { 'Content-Encoding': 'gzip' });
    raw.pipe(zlib.createGzip()).pipe(response);
  } else {
    response.writeHead(200, {});
    raw.pipe(response);
  }
}).listen(1337);
```

By default, the `zlib` methods with throw an error when decompressing
truncated data. However, if it is known that the data is incomplete, or
the desire is to inspect only the beginning of a compressed file, it is
possible to suppress the default error handling by changing the flushing
method that is used to compressed the last chunk of input data:

```js
// This is a truncated version of the buffer from the above examples
const buffer = Buffer.from('eJzT0yMA', 'base64');

zlib.unzip(buffer, { finishFlush: zlib.Z_SYNC_FLUSH }, (err, buffer) => {
  if (!err) {
    console.log(buffer.toString());
  } else {
    // handle error
  }
});
```

This will not change the behavior in other error-throwing situations, e.g.
when the input data has an invalid format. Using this method, it will not be
possible to determine whether the input ended prematurely or lacks the
integrity checks, making it necessary to manually check that the
decompressed result is valid.

## Memory Usage Tuning

<!--type=misc-->

From `zlib/zconf.h`, modified to node.js's usage:

The memory requirements for deflate are (in bytes):

```js
(1 << (windowBits+2)) +  (1 << (memLevel+9))
```

That is: 128K for windowBits=15  +  128K for memLevel = 8
(default values) plus a few kilobytes for small objects.

For example, to reduce the default memory requirements from 256K to 128K, the
options shoud be set to:

```js
{ windowBits: 14, memLevel: 7 }
```

This will, however, generally degrade compression.

The memory requirements for inflate are (in bytes)

```js
1 << windowBits
```

That is, 32K for windowBits=15 (default value) plus a few kilobytes
for small objects.

This is in addition to a single internal output slab buffer of size
`chunkSize`, which defaults to 16K.

The speed of `zlib` compression is affected most dramatically by the
`level` setting.  A higher level will result in better compression, but
will take longer to complete.  A lower level will result in less
compression, but will be much faster.

In general, greater memory usage options will mean that Node.js has to make
fewer calls to `zlib` because it will be able to process more data on
each `write` operation.  So, this is another factor that affects the
speed, at the cost of memory usage.

## Flushing

Calling [`.flush()`][] on a compression stream will make `zlib` return as much
output as currently possible. This may come at the cost of degraded compression
quality, but can be useful when data needs to be available as soon as possible.

In the following example, `flush()` is used to write a compressed partial
HTTP response to the client:
```js
const zlib = require('zlib');
const http = require('http');

http.createServer((request, response) => {
  // For the sake of simplicity, the Accept-Encoding checks are omitted.
  response.writeHead(200, { 'content-encoding': 'gzip' });
  const output = zlib.createGzip();
  output.pipe(response);

  setInterval(() => {
    output.write(`The current time is ${Date()}\n`, () => {
      // The data has been passed to zlib, but the compression algorithm may
      // have decided to buffer the data for more efficient compression.
      // Calling .flush() will make the data available as soon as the client
      // is ready to receive it.
      output.flush();
    });
  }, 1000);
}).listen(1337);
```

## Constants
<!-- YAML
added: v0.5.8
-->

<!--type=misc-->

All of the constants defined in `zlib.h` are also defined on `require('zlib')`.
In the normal course of operations, it will not be necessary to use these
constants. They are documented so that their presence is not surprising. This
section is taken almost directly from the [zlib documentation][].  See
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

The deflate compression method (the only one supported in this version).

* `zlib.Z_DEFLATED`

For initializing zalloc, zfree, opaque.

* `zlib.Z_NULL`

## Class Options
<!-- YAML
added: v0.11.1
-->

<!--type=misc-->

Each class takes an `options` object.  All options are optional.

Note that some options are only relevant when compressing, and are
ignored by the decompression classes.

* `flush` (default: `zlib.Z_NO_FLUSH`)
* `finishFlush` (default: `zlib.Z_FINISH`)
* `chunkSize` (default: 16*1024)
* `windowBits`
* `level` (compression only)
* `memLevel` (compression only)
* `strategy` (compression only)
* `dictionary` (deflate/inflate only, empty dictionary by default)

See the description of `deflateInit2` and `inflateInit2` at
<http://zlib.net/manual.html#Advanced> for more information on these.

## Class: zlib.Deflate
<!-- YAML
added: v0.5.8
-->

Compress data using deflate.

## Class: zlib.DeflateRaw
<!-- YAML
added: v0.5.8
-->

Compress data using deflate, and do not append a `zlib` header.

## Class: zlib.Gunzip
<!-- YAML
added: v0.5.8
-->

Decompress a gzip stream.

## Class: zlib.Gzip
<!-- YAML
added: v0.5.8
-->

Compress data using gzip.

## Class: zlib.Inflate
<!-- YAML
added: v0.5.8
-->

Decompress a deflate stream.

## Class: zlib.InflateRaw
<!-- YAML
added: v0.5.8
-->

Decompress a raw deflate stream.

## Class: zlib.Unzip
<!-- YAML
added: v0.5.8
-->

Decompress either a Gzip- or Deflate-compressed stream by auto-detecting
the header.

## Class: zlib.Zlib
<!-- YAML
added: v0.5.8
-->

Not exported by the `zlib` module. It is documented here because it is the base
class of the compressor/decompressor classes.

### zlib.flush([kind], callback)
<!-- YAML
added: v0.5.8
-->

`kind` defaults to `zlib.Z_FULL_FLUSH`.

Flush pending data. Don't call this frivolously, premature flushes negatively
impact the effectiveness of the compression algorithm.

Calling this only flushes data from the internal `zlib` state, and does not
perform flushing of any kind on the streams level. Rather, it behaves like a
normal call to `.write()`, i.e. it will be queued up behind other pending
writes and will only produce output when data is being read from the stream.

### zlib.params(level, strategy, callback)
<!-- YAML
added: v0.11.4
-->

Dynamically update the compression level and compression strategy.
Only applicable to deflate algorithm.

### zlib.reset()
<!-- YAML
added: v0.7.0
-->

Reset the compressor/decompressor to factory defaults. Only applicable to
the inflate and deflate algorithms.

## zlib.constants

Provides an object enumerating Zlib-related constants.

## zlib.createDeflate([options])
<!-- YAML
added: v0.5.8
-->

Returns a new [Deflate][] object with an [options][].

## zlib.createDeflateRaw([options])
<!-- YAML
added: v0.5.8
-->

Returns a new [DeflateRaw][] object with an [options][].

## zlib.createGunzip([options])
<!-- YAML
added: v0.5.8
-->

Returns a new [Gunzip][] object with an [options][].

## zlib.createGzip([options])
<!-- YAML
added: v0.5.8
-->

Returns a new [Gzip][] object with an [options][].

## zlib.createInflate([options])
<!-- YAML
added: v0.5.8
-->

Returns a new [Inflate][] object with an [options][].

## zlib.createInflateRaw([options])
<!-- YAML
added: v0.5.8
-->

Returns a new [InflateRaw][] object with an [options][].

## zlib.createUnzip([options])
<!-- YAML
added: v0.5.8
-->

Returns a new [Unzip][] object with an [options][].

## Convenience Methods

<!--type=misc-->

All of these take a [Buffer][] or string as the first argument, an optional
second argument to supply options to the `zlib` classes and will call the
supplied callback with `callback(error, result)`.

Every method has a `*Sync` counterpart, which accept the same arguments, but
without a callback.

### zlib.deflate(buf[, options], callback)
<!-- YAML
added: v0.6.0
-->
### zlib.deflateSync(buf[, options])
<!-- YAML
added: v0.11.12
-->

Compress a Buffer or string with Deflate.

### zlib.deflateRaw(buf[, options], callback)
<!-- YAML
added: v0.6.0
-->
### zlib.deflateRawSync(buf[, options])
<!-- YAML
added: v0.11.12
-->

Compress a Buffer or string with DeflateRaw.

### zlib.gunzip(buf[, options], callback)
<!-- YAML
added: v0.6.0
-->
### zlib.gunzipSync(buf[, options])
<!-- YAML
added: v0.11.12
-->

Decompress a Buffer or string with Gunzip.

### zlib.gzip(buf[, options], callback)
<!-- YAML
added: v0.6.0
-->
### zlib.gzipSync(buf[, options])
<!-- YAML
added: v0.11.12
-->

Compress a Buffer or string with Gzip.

### zlib.inflate(buf[, options], callback)
<!-- YAML
added: v0.6.0
-->
### zlib.inflateSync(buf[, options])
<!-- YAML
added: v0.11.12
-->

Decompress a Buffer or string with Inflate.

### zlib.inflateRaw(buf[, options], callback)
<!-- YAML
added: v0.6.0
-->
### zlib.inflateRawSync(buf[, options])
<!-- YAML
added: v0.11.12
-->

Decompress a Buffer or string with InflateRaw.

### zlib.unzip(buf[, options], callback)
<!-- YAML
added: v0.6.0
-->
### zlib.unzipSync(buf[, options])
<!-- YAML
added: v0.11.12
-->

Decompress a Buffer or string with Unzip.

[`Accept-Encoding`]: https://www.w3.org/Protocols/rfc2616/rfc2616-sec14.html#sec14.3
[`Content-Encoding`]: https://www.w3.org/Protocols/rfc2616/rfc2616-sec14.html#sec14.11
[Memory Usage Tuning]: #zlib_memory_usage_tuning
[zlib documentation]: http://zlib.net/manual.html#Constants
[options]: #zlib_class_options
[Deflate]: #zlib_class_zlib_deflate
[DeflateRaw]: #zlib_class_zlib_deflateraw
[Gunzip]: #zlib_class_zlib_gunzip
[Gzip]: #zlib_class_zlib_gzip
[Inflate]: #zlib_class_zlib_inflate
[InflateRaw]: #zlib_class_zlib_inflateraw
[Unzip]: #zlib_class_zlib_unzip
[`.flush()`]: #zlib_zlib_flush_kind_callback
[Buffer]: buffer.html

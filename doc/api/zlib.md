# Zlib

<!--introduced_in=v0.10.0-->

> Stability: 2 - Stable

<!-- source_link=lib/zlib.js -->

The `node:zlib` module provides compression functionality implemented using
Gzip, Deflate/Inflate, Brotli, and Zstd.

To access it:

```mjs
import zlib from 'node:zlib';
```

```cjs
const zlib = require('node:zlib');
```

Compression and decompression are built around the Node.js [Streams API][].

Compressing or decompressing a stream (such as a file) can be accomplished by
piping the source stream through a `zlib` `Transform` stream into a destination
stream:

```mjs
import {
  createReadStream,
  createWriteStream,
} from 'node:fs';
import process from 'node:process';
import { createGzip } from 'node:zlib';
import { pipeline } from 'node:stream';

const gzip = createGzip();
const source = createReadStream('input.txt');
const destination = createWriteStream('input.txt.gz');

pipeline(source, gzip, destination, (err) => {
  if (err) {
    console.error('An error occurred:', err);
    process.exitCode = 1;
  }
});
```

```cjs
const {
  createReadStream,
  createWriteStream,
} = require('node:fs');
const { createGzip } = require('node:zlib');
const { pipeline } = require('node:stream');

const gzip = createGzip();
const source = createReadStream('input.txt');
const destination = createWriteStream('input.txt.gz');

pipeline(source, gzip, destination, (err) => {
  if (err) {
    console.error('An error occurred:', err);
    process.exitCode = 1;
  }
});
```

Or, using the promise `pipeline` API:

```mjs
import {
  createReadStream,
  createWriteStream,
} from 'node:fs';
import { createGzip } from 'node:zlib';
import { pipeline } from 'node:stream/promises';

async function do_gzip(input, output) {
  const gzip = createGzip();
  const source = createReadStream(input);
  const destination = createWriteStream(output);
  await pipeline(source, gzip, destination);
}

await do_gzip('input.txt', 'input.txt.gz');
```

```cjs
const {
  createReadStream,
  createWriteStream,
} = require('node:fs');
const { createGzip } = require('node:zlib');
const { pipeline } = require('node:stream/promises');

async function do_gzip(input, output) {
  const gzip = createGzip();
  const source = createReadStream(input);
  const destination = createWriteStream(output);
  await pipeline(source, gzip, destination);
}

do_gzip('input.txt', 'input.txt.gz')
  .catch((err) => {
    console.error('An error occurred:', err);
    process.exitCode = 1;
  });
```

It is also possible to compress or decompress data in a single step:

```mjs
import process from 'node:process';
import { Buffer } from 'node:buffer';
import { deflate, unzip } from 'node:zlib';

const input = '.................................';
deflate(input, (err, buffer) => {
  if (err) {
    console.error('An error occurred:', err);
    process.exitCode = 1;
  }
  console.log(buffer.toString('base64'));
});

const buffer = Buffer.from('eJzT0yMAAGTvBe8=', 'base64');
unzip(buffer, (err, buffer) => {
  if (err) {
    console.error('An error occurred:', err);
    process.exitCode = 1;
  }
  console.log(buffer.toString());
});

// Or, Promisified

import { promisify } from 'node:util';
const do_unzip = promisify(unzip);

const unzippedBuffer = await do_unzip(buffer);
console.log(unzippedBuffer.toString());
```

```cjs
const { deflate, unzip } = require('node:zlib');

const input = '.................................';
deflate(input, (err, buffer) => {
  if (err) {
    console.error('An error occurred:', err);
    process.exitCode = 1;
  }
  console.log(buffer.toString('base64'));
});

const buffer = Buffer.from('eJzT0yMAAGTvBe8=', 'base64');
unzip(buffer, (err, buffer) => {
  if (err) {
    console.error('An error occurred:', err);
    process.exitCode = 1;
  }
  console.log(buffer.toString());
});

// Or, Promisified

const { promisify } = require('node:util');
const do_unzip = promisify(unzip);

do_unzip(buffer)
  .then((buf) => console.log(buf.toString()))
  .catch((err) => {
    console.error('An error occurred:', err);
    process.exitCode = 1;
  });
```

## Threadpool usage and performance considerations

All `zlib` APIs, except those that are explicitly synchronous, use the Node.js
internal threadpool. This can lead to surprising effects and performance
limitations in some applications.

Creating and using a large number of zlib objects simultaneously can cause
significant memory fragmentation.

```mjs
import zlib from 'node:zlib';
import { Buffer } from 'node:buffer';

const payload = Buffer.from('This is some data');

// WARNING: DO NOT DO THIS!
for (let i = 0; i < 30000; ++i) {
  zlib.deflate(payload, (err, buffer) => {});
}
```

```cjs
const zlib = require('node:zlib');

const payload = Buffer.from('This is some data');

// WARNING: DO NOT DO THIS!
for (let i = 0; i < 30000; ++i) {
  zlib.deflate(payload, (err, buffer) => {});
}
```

In the preceding example, 30,000 deflate instances are created concurrently.
Because of how some operating systems handle memory allocation and
deallocation, this may lead to significant memory fragmentation.

It is strongly recommended that the results of compression
operations be cached to avoid duplication of effort.

## Compressing HTTP requests and responses

The `node:zlib` module can be used to implement support for the `gzip`, `deflate`,
`br`, and `zstd` content-encoding mechanisms defined by
[HTTP](https://tools.ietf.org/html/rfc7230#section-4.2).

The HTTP [`Accept-Encoding`][] header is used within an HTTP request to identify
the compression encodings accepted by the client. The [`Content-Encoding`][]
header is used to identify the compression encodings actually applied to a
message.

The examples given below are drastically simplified to show the basic concept.
Using `zlib` encoding can be expensive, and the results ought to be cached.
See [Memory usage tuning][] for more information on the speed/memory/compression
tradeoffs involved in `zlib` usage.

```mjs
// Client request example
import fs from 'node:fs';
import zlib from 'node:zlib';
import http from 'node:http';
import process from 'node:process';
import { pipeline } from 'node:stream';

const request = http.get({ host: 'example.com',
                           path: '/',
                           port: 80,
                           headers: { 'Accept-Encoding': 'br,gzip,deflate,zstd' } });
request.on('response', (response) => {
  const output = fs.createWriteStream('example.com_index.html');

  const onError = (err) => {
    if (err) {
      console.error('An error occurred:', err);
      process.exitCode = 1;
    }
  };

  switch (response.headers['content-encoding']) {
    case 'br':
      pipeline(response, zlib.createBrotliDecompress(), output, onError);
      break;
    // Or, just use zlib.createUnzip() to handle both of the following cases:
    case 'gzip':
      pipeline(response, zlib.createGunzip(), output, onError);
      break;
    case 'deflate':
      pipeline(response, zlib.createInflate(), output, onError);
      break;
    case 'zstd':
      pipeline(response, zlib.createZstdDecompress(), output, onError);
      break;
    default:
      pipeline(response, output, onError);
      break;
  }
});
```

```cjs
// Client request example
const zlib = require('node:zlib');
const http = require('node:http');
const fs = require('node:fs');
const { pipeline } = require('node:stream');

const request = http.get({ host: 'example.com',
                           path: '/',
                           port: 80,
                           headers: { 'Accept-Encoding': 'br,gzip,deflate,zstd' } });
request.on('response', (response) => {
  const output = fs.createWriteStream('example.com_index.html');

  const onError = (err) => {
    if (err) {
      console.error('An error occurred:', err);
      process.exitCode = 1;
    }
  };

  switch (response.headers['content-encoding']) {
    case 'br':
      pipeline(response, zlib.createBrotliDecompress(), output, onError);
      break;
    // Or, just use zlib.createUnzip() to handle both of the following cases:
    case 'gzip':
      pipeline(response, zlib.createGunzip(), output, onError);
      break;
    case 'deflate':
      pipeline(response, zlib.createInflate(), output, onError);
      break;
    case 'zstd':
      pipeline(response, zlib.createZstdDecompress(), output, onError);
      break;
    default:
      pipeline(response, output, onError);
      break;
  }
});
```

```mjs
// server example
// Running a gzip operation on every request is quite expensive.
// It would be much more efficient to cache the compressed buffer.
import zlib from 'node:zlib';
import http from 'node:http';
import fs from 'node:fs';
import { pipeline } from 'node:stream';

http.createServer((request, response) => {
  const raw = fs.createReadStream('index.html');
  // Store both a compressed and an uncompressed version of the resource.
  response.setHeader('Vary', 'Accept-Encoding');
  const acceptEncoding = request.headers['accept-encoding'] || '';

  const onError = (err) => {
    if (err) {
      // If an error occurs, there's not much we can do because
      // the server has already sent the 200 response code and
      // some amount of data has already been sent to the client.
      // The best we can do is terminate the response immediately
      // and log the error.
      response.end();
      console.error('An error occurred:', err);
    }
  };

  // Note: This is not a conformant accept-encoding parser.
  // See https://www.w3.org/Protocols/rfc2616/rfc2616-sec14.html#sec14.3
  if (/\bdeflate\b/.test(acceptEncoding)) {
    response.writeHead(200, { 'Content-Encoding': 'deflate' });
    pipeline(raw, zlib.createDeflate(), response, onError);
  } else if (/\bgzip\b/.test(acceptEncoding)) {
    response.writeHead(200, { 'Content-Encoding': 'gzip' });
    pipeline(raw, zlib.createGzip(), response, onError);
  } else if (/\bbr\b/.test(acceptEncoding)) {
    response.writeHead(200, { 'Content-Encoding': 'br' });
    pipeline(raw, zlib.createBrotliCompress(), response, onError);
  } else if (/\bzstd\b/.test(acceptEncoding)) {
    response.writeHead(200, { 'Content-Encoding': 'zstd' });
    pipeline(raw, zlib.createZstdCompress(), response, onError);
  } else {
    response.writeHead(200, {});
    pipeline(raw, response, onError);
  }
}).listen(1337);
```

```cjs
// server example
// Running a gzip operation on every request is quite expensive.
// It would be much more efficient to cache the compressed buffer.
const zlib = require('node:zlib');
const http = require('node:http');
const fs = require('node:fs');
const { pipeline } = require('node:stream');

http.createServer((request, response) => {
  const raw = fs.createReadStream('index.html');
  // Store both a compressed and an uncompressed version of the resource.
  response.setHeader('Vary', 'Accept-Encoding');
  const acceptEncoding = request.headers['accept-encoding'] || '';

  const onError = (err) => {
    if (err) {
      // If an error occurs, there's not much we can do because
      // the server has already sent the 200 response code and
      // some amount of data has already been sent to the client.
      // The best we can do is terminate the response immediately
      // and log the error.
      response.end();
      console.error('An error occurred:', err);
    }
  };

  // Note: This is not a conformant accept-encoding parser.
  // See https://www.w3.org/Protocols/rfc2616/rfc2616-sec14.html#sec14.3
  if (/\bdeflate\b/.test(acceptEncoding)) {
    response.writeHead(200, { 'Content-Encoding': 'deflate' });
    pipeline(raw, zlib.createDeflate(), response, onError);
  } else if (/\bgzip\b/.test(acceptEncoding)) {
    response.writeHead(200, { 'Content-Encoding': 'gzip' });
    pipeline(raw, zlib.createGzip(), response, onError);
  } else if (/\bbr\b/.test(acceptEncoding)) {
    response.writeHead(200, { 'Content-Encoding': 'br' });
    pipeline(raw, zlib.createBrotliCompress(), response, onError);
  } else if (/\bzstd\b/.test(acceptEncoding)) {
    response.writeHead(200, { 'Content-Encoding': 'zstd' });
    pipeline(raw, zlib.createZstdCompress(), response, onError);
  } else {
    response.writeHead(200, {});
    pipeline(raw, response, onError);
  }
}).listen(1337);
```

By default, the `zlib` methods will throw an error when decompressing
truncated data. However, if it is known that the data is incomplete, or
the desire is to inspect only the beginning of a compressed file, it is
possible to suppress the default error handling by changing the flushing
method that is used to decompress the last chunk of input data:

```js
// This is a truncated version of the buffer from the above examples
const buffer = Buffer.from('eJzT0yMA', 'base64');

zlib.unzip(
  buffer,
  // For Brotli, the equivalent is zlib.constants.BROTLI_OPERATION_FLUSH.
  // For Zstd, the equivalent is zlib.constants.ZSTD_e_flush.
  { finishFlush: zlib.constants.Z_SYNC_FLUSH },
  (err, buffer) => {
    if (err) {
      console.error('An error occurred:', err);
      process.exitCode = 1;
    }
    console.log(buffer.toString());
  });
```

This will not change the behavior in other error-throwing situations, e.g.
when the input data has an invalid format. Using this method, it will not be
possible to determine whether the input ended prematurely or lacks the
integrity checks, making it necessary to manually check that the
decompressed result is valid.

## Memory usage tuning

<!--type=misc-->

### For zlib-based streams

From `zlib/zconf.h`, modified for Node.js usage:

The memory requirements for deflate are (in bytes):

```js
(1 << (windowBits + 2)) + (1 << (memLevel + 9));
```

That is: 128K for `windowBits` = 15 + 128K for `memLevel` = 8
(default values) plus a few kilobytes for small objects.

For example, to reduce the default memory requirements from 256K to 128K, the
options should be set to:

```js
const options = { windowBits: 14, memLevel: 7 };
```

This will, however, generally degrade compression.

The memory requirements for inflate are (in bytes) `1 << windowBits`.
That is, 32K for `windowBits` = 15 (default value) plus a few kilobytes
for small objects.

This is in addition to a single internal output slab buffer of size
`chunkSize`, which defaults to 16K.

The speed of `zlib` compression is affected most dramatically by the
`level` setting. A higher level will result in better compression, but
will take longer to complete. A lower level will result in less
compression, but will be much faster.

In general, greater memory usage options will mean that Node.js has to make
fewer calls to `zlib` because it will be able to process more data on
each `write` operation. So, this is another factor that affects the
speed, at the cost of memory usage.

### For Brotli-based streams

There are equivalents to the zlib options for Brotli-based streams, although
these options have different ranges than the zlib ones:

* zlib's `level` option matches Brotli's `BROTLI_PARAM_QUALITY` option.
* zlib's `windowBits` option matches Brotli's `BROTLI_PARAM_LGWIN` option.

See [below][Brotli parameters] for more details on Brotli-specific options.

### For Zstd-based streams

> Stability: 1 - Experimental

There are equivalents to the zlib options for Zstd-based streams, although
these options have different ranges than the zlib ones:

* zlib's `level` option matches Zstd's `ZSTD_c_compressionLevel` option.
* zlib's `windowBits` option matches Zstd's `ZSTD_c_windowLog` option.

See [below][Zstd parameters] for more details on Zstd-specific options.

## Flushing

Calling [`.flush()`][] on a compression stream will make `zlib` return as much
output as currently possible. This may come at the cost of degraded compression
quality, but can be useful when data needs to be available as soon as possible.

In the following example, `flush()` is used to write a compressed partial
HTTP response to the client:

```mjs
import zlib from 'node:zlib';
import http from 'node:http';
import { pipeline } from 'node:stream';

http.createServer((request, response) => {
  // For the sake of simplicity, the Accept-Encoding checks are omitted.
  response.writeHead(200, { 'content-encoding': 'gzip' });
  const output = zlib.createGzip();
  let i;

  pipeline(output, response, (err) => {
    if (err) {
      // If an error occurs, there's not much we can do because
      // the server has already sent the 200 response code and
      // some amount of data has already been sent to the client.
      // The best we can do is terminate the response immediately
      // and log the error.
      clearInterval(i);
      response.end();
      console.error('An error occurred:', err);
    }
  });

  i = setInterval(() => {
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

```cjs
const zlib = require('node:zlib');
const http = require('node:http');
const { pipeline } = require('node:stream');

http.createServer((request, response) => {
  // For the sake of simplicity, the Accept-Encoding checks are omitted.
  response.writeHead(200, { 'content-encoding': 'gzip' });
  const output = zlib.createGzip();
  let i;

  pipeline(output, response, (err) => {
    if (err) {
      // If an error occurs, there's not much we can do because
      // the server has already sent the 200 response code and
      // some amount of data has already been sent to the client.
      // The best we can do is terminate the response immediately
      // and log the error.
      clearInterval(i);
      response.end();
      console.error('An error occurred:', err);
    }
  });

  i = setInterval(() => {
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

### zlib constants

All of the constants defined in `zlib.h` are also defined on
`require('node:zlib').constants`. In the normal course of operations, it will
not be necessary to use these constants. They are documented so that their
presence is not surprising. This section is taken almost directly from the
[zlib documentation][].

Previously, the constants were available directly from `require('node:zlib')`,
for instance `zlib.Z_NO_FLUSH`. Accessing the constants directly from the module
is currently still possible but is deprecated.

Allowed flush values.

* `zlib.constants.Z_NO_FLUSH`
* `zlib.constants.Z_PARTIAL_FLUSH`
* `zlib.constants.Z_SYNC_FLUSH`
* `zlib.constants.Z_FULL_FLUSH`
* `zlib.constants.Z_FINISH`
* `zlib.constants.Z_BLOCK`

Return codes for the compression/decompression functions. Negative
values are errors, positive values are used for special but normal
events.

* `zlib.constants.Z_OK`
* `zlib.constants.Z_STREAM_END`
* `zlib.constants.Z_NEED_DICT`
* `zlib.constants.Z_ERRNO`
* `zlib.constants.Z_STREAM_ERROR`
* `zlib.constants.Z_DATA_ERROR`
* `zlib.constants.Z_MEM_ERROR`
* `zlib.constants.Z_BUF_ERROR`
* `zlib.constants.Z_VERSION_ERROR`

Compression levels.

* `zlib.constants.Z_NO_COMPRESSION`
* `zlib.constants.Z_BEST_SPEED`
* `zlib.constants.Z_BEST_COMPRESSION`
* `zlib.constants.Z_DEFAULT_COMPRESSION`

Compression strategy.

* `zlib.constants.Z_FILTERED`
* `zlib.constants.Z_HUFFMAN_ONLY`
* `zlib.constants.Z_RLE`
* `zlib.constants.Z_FIXED`
* `zlib.constants.Z_DEFAULT_STRATEGY`

### Brotli constants

<!-- YAML
added:
 - v11.7.0
 - v10.16.0
-->

There are several options and other constants available for Brotli-based
streams:

#### Flush operations

The following values are valid flush operations for Brotli-based streams:

* `zlib.constants.BROTLI_OPERATION_PROCESS` (default for all operations)
* `zlib.constants.BROTLI_OPERATION_FLUSH` (default when calling `.flush()`)
* `zlib.constants.BROTLI_OPERATION_FINISH` (default for the last chunk)
* `zlib.constants.BROTLI_OPERATION_EMIT_METADATA`
  * This particular operation may be hard to use in a Node.js context,
    as the streaming layer makes it hard to know which data will end up
    in this frame. Also, there is currently no way to consume this data through
    the Node.js API.

#### Compressor options

There are several options that can be set on Brotli encoders, affecting
compression efficiency and speed. Both the keys and the values can be accessed
as properties of the `zlib.constants` object.

The most important options are:

* `BROTLI_PARAM_MODE`
  * `BROTLI_MODE_GENERIC` (default)
  * `BROTLI_MODE_TEXT`, adjusted for UTF-8 text
  * `BROTLI_MODE_FONT`, adjusted for WOFF 2.0 fonts
* `BROTLI_PARAM_QUALITY`
  * Ranges from `BROTLI_MIN_QUALITY` to `BROTLI_MAX_QUALITY`,
    with a default of `BROTLI_DEFAULT_QUALITY`.
* `BROTLI_PARAM_SIZE_HINT`
  * Integer value representing the expected input size;
    defaults to `0` for an unknown input size.

The following flags can be set for advanced control over the compression
algorithm and memory usage tuning:

* `BROTLI_PARAM_LGWIN`
  * Ranges from `BROTLI_MIN_WINDOW_BITS` to `BROTLI_MAX_WINDOW_BITS`,
    with a default of `BROTLI_DEFAULT_WINDOW`, or up to
    `BROTLI_LARGE_MAX_WINDOW_BITS` if the `BROTLI_PARAM_LARGE_WINDOW` flag
    is set.
* `BROTLI_PARAM_LGBLOCK`
  * Ranges from `BROTLI_MIN_INPUT_BLOCK_BITS` to `BROTLI_MAX_INPUT_BLOCK_BITS`.
* `BROTLI_PARAM_DISABLE_LITERAL_CONTEXT_MODELING`
  * Boolean flag that decreases compression ratio in favour of
    decompression speed.
* `BROTLI_PARAM_LARGE_WINDOW`
  * Boolean flag enabling “Large Window Brotli” mode (not compatible with the
    Brotli format as standardized in [RFC 7932][]).
* `BROTLI_PARAM_NPOSTFIX`
  * Ranges from `0` to `BROTLI_MAX_NPOSTFIX`.
* `BROTLI_PARAM_NDIRECT`
  * Ranges from `0` to `15 << NPOSTFIX` in steps of `1 << NPOSTFIX`.

#### Decompressor options

These advanced options are available for controlling decompression:

* `BROTLI_DECODER_PARAM_DISABLE_RING_BUFFER_REALLOCATION`
  * Boolean flag that affects internal memory allocation patterns.
* `BROTLI_DECODER_PARAM_LARGE_WINDOW`
  * Boolean flag enabling “Large Window Brotli” mode (not compatible with the
    Brotli format as standardized in [RFC 7932][]).

### Zstd constants

> Stability: 1 - Experimental

<!-- YAML
added:
  - v23.8.0
  - v22.15.0
-->

There are several options and other constants available for Zstd-based
streams:

#### Flush operations

The following values are valid flush operations for Zstd-based streams:

* `zlib.constants.ZSTD_e_continue` (default for all operations)
* `zlib.constants.ZSTD_e_flush` (default when calling `.flush()`)
* `zlib.constants.ZSTD_e_end` (default for the last chunk)

#### Compressor options

There are several options that can be set on Zstd encoders, affecting
compression efficiency and speed. Both the keys and the values can be accessed
as properties of the `zlib.constants` object.

The most important options are:

* `ZSTD_c_compressionLevel`
  * Set compression parameters according to pre-defined cLevel table. Default
    level is ZSTD\_CLEVEL\_DEFAULT==3.
* `ZSTD_c_strategy`
  * Select the compression strategy.
  * Possible values are listed in the strategy options section below.

#### Strategy options

The following constants can be used as values for the `ZSTD_c_strategy`
parameter:

* `zlib.constants.ZSTD_fast`
* `zlib.constants.ZSTD_dfast`
* `zlib.constants.ZSTD_greedy`
* `zlib.constants.ZSTD_lazy`
* `zlib.constants.ZSTD_lazy2`
* `zlib.constants.ZSTD_btlazy2`
* `zlib.constants.ZSTD_btopt`
* `zlib.constants.ZSTD_btultra`
* `zlib.constants.ZSTD_btultra2`

Example:

```js
const stream = zlib.createZstdCompress({
  params: {
    [zlib.constants.ZSTD_c_strategy]: zlib.constants.ZSTD_btultra,
  },
});
```

#### Pledged Source Size

It's possible to specify the expected total size of the uncompressed input via
`opts.pledgedSrcSize`, which must be a non-negative safe integer. If the size
doesn't match at the end of the input, compression will fail with the code
`ZSTD_error_srcSize_wrong`.

#### Decompressor options

These advanced options are available for controlling decompression:

* `ZSTD_d_windowLogMax`
  * Select a size limit (in power of 2) beyond which the streaming API will
    refuse to allocate memory buffer in order to protect the host from
    unreasonable memory requirements.

## Class: `Options`

<!-- YAML
added: v0.11.1
changes:
  - version: v26.5.0
    pr-url: https://github.com/nodejs/node/pull/64023
    description: The `rejectGarbageAfterEnd` option was added.
  - version:
    - v14.5.0
    - v12.19.0
    pr-url: https://github.com/nodejs/node/pull/33516
    description: The `maxOutputLength` option is supported now.
  - version: v9.4.0
    pr-url: https://github.com/nodejs/node/pull/16042
    description: The `dictionary` option can be an `ArrayBuffer`.
  - version: v8.0.0
    pr-url: https://github.com/nodejs/node/pull/12001
    description: The `dictionary` option can be an `Uint8Array` now.
  - version: v5.11.0
    pr-url: https://github.com/nodejs/node/pull/6069
    description: The `finishFlush` option is supported now.
-->

<!--type=misc-->

Each zlib-based class takes an `options` object. No options are required.

Some options are only relevant when compressing and are
ignored by the decompression classes.

* `flush` {integer} **Default:** `zlib.constants.Z_NO_FLUSH`
* `finishFlush` {integer} **Default:** `zlib.constants.Z_FINISH`
* `chunkSize` {integer} **Default:** `16 * 1024`
* `windowBits` {integer}
* `level` {integer} (compression only)
* `memLevel` {integer} (compression only)
* `strategy` {integer} (compression only)
* `dictionary` {Buffer|TypedArray|DataView|ArrayBuffer} (deflate/inflate only,
  empty dictionary by default)
* `info` {boolean} (If `true`, returns an object with `buffer` and `engine`.)
* `maxOutputLength` {integer} Limits output size when using
  [convenience methods][]. **Default:** [`buffer.kMaxLength`][]
* `rejectGarbageAfterEnd` {boolean} If `true`, decompression fails when
  trailing input is detected after the end of the compressed stream. This
  includes unreadable bytes and, when decompressing gzip, additional gzip
  members following the first member. **Default:** `false`

See the [`deflateInit2` and `inflateInit2`][] documentation for more
information.

## Class: `BrotliOptions`

<!-- YAML
added: v11.7.0
changes:
  - version: v26.5.0
    pr-url: https://github.com/nodejs/node/pull/64023
    description: The `rejectGarbageAfterEnd` option was added.
  - version:
    - v14.5.0
    - v12.19.0
    pr-url: https://github.com/nodejs/node/pull/33516
    description: The `maxOutputLength` option is supported now.
-->

<!--type=misc-->

Each Brotli-based class takes an `options` object. All options are optional.

* `flush` {integer} **Default:** `zlib.constants.BROTLI_OPERATION_PROCESS`
* `finishFlush` {integer} **Default:** `zlib.constants.BROTLI_OPERATION_FINISH`
* `chunkSize` {integer} **Default:** `16 * 1024`
* `params` {Object} Key-value object containing indexed [Brotli parameters][].
* `maxOutputLength` {integer} Limits output size when using
  [convenience methods][]. **Default:** [`buffer.kMaxLength`][]
* `info` {boolean} If `true`, returns an object with `buffer` and `engine`. **Default:** `false`
* `rejectGarbageAfterEnd` {boolean} If `true`, decompression fails when
  input remains after the first complete compressed stream. **Default:** `false`

For example:

```js
const stream = zlib.createBrotliCompress({
  chunkSize: 32 * 1024,
  params: {
    [zlib.constants.BROTLI_PARAM_MODE]: zlib.constants.BROTLI_MODE_TEXT,
    [zlib.constants.BROTLI_PARAM_QUALITY]: 4,
    [zlib.constants.BROTLI_PARAM_SIZE_HINT]: fs.statSync(inputFile).size,
  },
});
```

## Class: `zlib.BrotliCompress`

<!-- YAML
added:
 - v11.7.0
 - v10.16.0
-->

* Extends: [`ZlibBase`][]

Compress data using the Brotli algorithm.

## Class: `zlib.BrotliDecompress`

<!-- YAML
added:
 - v11.7.0
 - v10.16.0
-->

* Extends: [`ZlibBase`][]

Decompress data using the Brotli algorithm.

## Class: `zlib.Deflate`

<!-- YAML
added: v0.5.8
-->

* Extends: [`ZlibBase`][]

Compress data using deflate.

## Class: `zlib.DeflateRaw`

<!-- YAML
added: v0.5.8
-->

* Extends: [`ZlibBase`][]

Compress data using deflate, and do not append a `zlib` header.

## Class: `zlib.Gunzip`

<!-- YAML
added: v0.5.8
changes:
  - version: v6.0.0
    pr-url: https://github.com/nodejs/node/pull/5883
    description: Trailing garbage at the end of the input stream will now
                 result in an `'error'` event.
  - version: v5.9.0
    pr-url: https://github.com/nodejs/node/pull/5120
    description: Multiple concatenated gzip file members are supported now.
  - version: v5.0.0
    pr-url: https://github.com/nodejs/node/pull/2595
    description: A truncated input stream will now result in an `'error'` event.
-->

* Extends: [`ZlibBase`][]

Decompress a gzip stream.

## Class: `zlib.Gzip`

<!-- YAML
added: v0.5.8
-->

* Extends: [`ZlibBase`][]

Compress data using gzip.

## Class: `zlib.Inflate`

<!-- YAML
added: v0.5.8
changes:
  - version: v5.0.0
    pr-url: https://github.com/nodejs/node/pull/2595
    description: A truncated input stream will now result in an `'error'` event.
-->

* Extends: [`ZlibBase`][]

Decompress a deflate stream.

## Class: `zlib.InflateRaw`

<!-- YAML
added: v0.5.8
changes:
  - version: v6.8.0
    pr-url: https://github.com/nodejs/node/pull/8512
    description: Custom dictionaries are now supported by `InflateRaw`.
  - version: v5.0.0
    pr-url: https://github.com/nodejs/node/pull/2595
    description: A truncated input stream will now result in an `'error'` event.
-->

* Extends: [`ZlibBase`][]

Decompress a raw deflate stream.

## Class: `zlib.Unzip`

<!-- YAML
added: v0.5.8
-->

* Extends: [`ZlibBase`][]

Decompress either a Gzip- or Deflate-compressed stream by auto-detecting
the header.

## Class: `zlib.ZipBuffer`

<!-- YAML
added: REPLACEME
-->

> Stability: 1 - Experimental

The ZIP archive API is experimental. The first access of any ZIP export on
`node:zlib` (this class among them) emits an experimental warning.

An in-memory, **zero-copy** view over the entries of a ZIP archive already
held in a `Buffer`, `TypedArray`, `DataView`, or `ArrayBuffer`. Its set of
entries can be edited - entries added or removed - but, unlike [`ZipFile`][],
those edits are **not** written into the source buffer: a newly added entry is
held as a separate in-memory [`ZipEntry`][] (the passed buffer is a fixed-size
view with no room to append to), and removal just drops the entry from
`ZipBuffer`'s index. The original bytes are never modified.
[`zipBuffer.toBuffer()`][] serializes the current set of entries into a fresh
archive.

`ZipBuffer` does not copy the archive you hand it. It keeps a view onto that
memory and reads each entry's content lazily and directly from it, which is
what makes construction cheap regardless of archive size. The trade-off is
that you **must not modify or reuse** that memory - including the
`ArrayBuffer` backing a `TypedArray`/`DataView` - while the `ZipBuffer`, or
any [`ZipEntry`][] obtained from it, is still in use: a later read would
observe the change and may fail or return corrupt data. Pass a copy (for
example `Buffer.from(source)`) if the source might be mutated or reused.

`add()` and `toBuffer()` each have a `*Sync` counterpart
([`addSync()`][`zipBuffer.addSync()`], [`toBufferSync()`][`zipBuffer.toBufferSync()`])
that performs the same compression work synchronously. As with the
synchronous `node:fs` APIs, these block the Node.js event loop and further
JavaScript execution until the operation completes; use them only where
synchronous execution is appropriate (for example, short-lived scripts or
startup code), not in code that must stay responsive.

```mjs
import { ZipBuffer } from 'node:zlib';
import { readFileSync, writeFileSync } from 'node:fs';
import { Buffer } from 'node:buffer';

const zip = new ZipBuffer(readFileSync('archive.zip'));
for (const [name, entry] of zip) {
  console.log(name, entry.size);
}
await zip.add('hello.txt', Buffer.from('Hello, world!'));
zip.delete('unwanted.txt');
writeFileSync('archive.zip', await zip.toBuffer());
```

```cjs
const { ZipBuffer } = require('node:zlib');
const { readFileSync, writeFileSync } = require('node:fs');

async function main() {
  const zip = new ZipBuffer(readFileSync('archive.zip'));
  for (const [name, entry] of zip) {
    console.log(name, entry.size);
  }
  await zip.add('hello.txt', Buffer.from('Hello, world!'));
  zip.delete('unwanted.txt');
  writeFileSync('archive.zip', await zip.toBuffer());
}
main();
```

### `new zlib.ZipBuffer(buffer)`

<!-- YAML
added: REPLACEME
-->

* `buffer` {Buffer|TypedArray|DataView|ArrayBuffer} A complete ZIP archive.

Parses the archive's central directory. Throws an [`ERR_ZIP_INVALID_ARCHIVE`][]
or [`ERR_ZIP_UNSUPPORTED_FEATURE`][] error if `buffer` is not a well-formed,
supported archive.

`buffer` is **not copied**: the `ZipBuffer` retains a zero-copy view of it (for
a `TypedArray`, `DataView`, or `ArrayBuffer`, of the underlying `ArrayBuffer`)
and reads entry content directly from it on demand. Do not mutate or reuse that
memory while the `ZipBuffer` or any entry read from it is still live; pass a
copy if it might change.

### `zipBuffer.add(filename, data[, options])`

<!-- YAML
added: REPLACEME
-->

* `filename` {string} The entry's name within the archive. A trailing `/`
  marks a directory entry.
* `data` {Buffer|TypedArray|DataView|ArrayBuffer} The entry's complete,
  uncompressed content.
* `options` {Object} See [`zlib.ZipEntry.create()`][].
* Returns: {Promise} Fulfilled with the created {ZipEntry}.

Equivalent to `zipBuffer.addEntry(await zlib.ZipEntry.create(filename, data,
options))`.

### `zipBuffer.addSync(filename, data[, options])`

<!-- YAML
added: REPLACEME
-->

* `filename` {string} The entry's name within the archive. A trailing `/`
  marks a directory entry.
* `data` {Buffer|TypedArray|DataView|ArrayBuffer} The entry's complete,
  uncompressed content.
* `options` {Object} See [`zlib.ZipEntry.createSync()`][].
* Returns: {ZipEntry} The created entry.

The synchronous version of [`zipBuffer.add()`][]. Equivalent to
`zipBuffer.addEntry(zlib.ZipEntry.createSync(filename, data, options))`.

### `zipBuffer.addEntry(entry)`

<!-- YAML
added: REPLACEME
-->

* `entry` {ZipEntry}
* Returns: {ZipEntry} `entry`.

Adds an already-built entry, keyed by its own [`zipEntry.name`][]. Replaces
any existing entry of that name.

### `zipBuffer.clear()`

<!-- YAML
added: REPLACEME
-->

Removes every entry.

### `zipBuffer.comment`

<!-- YAML
added: REPLACEME
-->

* Type: {string}

The archive-level comment, preserved byte-for-byte across
[`zipBuffer.toBuffer()`][] calls unless overridden. The bytes are decoded as
UTF-8 when they are valid UTF-8 and as CP437 otherwise (the field carries no
encoding flag of its own).

### `zipBuffer.delete(name)`

<!-- YAML
added: REPLACEME
-->

* `name` {string}
* Returns: {boolean} `true` if an entry named `name` existed and was removed.

### `zipBuffer.entries()`

<!-- YAML
added: REPLACEME
-->

* Returns: {Iterator} of `[name, entry]` pairs, where `entry` is a
  [`ZipEntry`][].

### `zipBuffer.forEach(callback[, thisArg])`

<!-- YAML
added: REPLACEME
-->

* `callback` {Function}
* `thisArg` {any}

Calls `callback` once for each entry, in the order the archive lists them.

### `zipBuffer.get(name)`

<!-- YAML
added: REPLACEME
-->

* `name` {string}
* Returns: {ZipEntry}

Throws [`ERR_ZIP_ENTRY_NOT_FOUND`][] if the archive has no entry named `name`.

### `zipBuffer.has(name)`

<!-- YAML
added: REPLACEME
-->

* `name` {string}
* Returns: {boolean}

### `zipBuffer.keys()`

<!-- YAML
added: REPLACEME
-->

* Returns: {Iterator} of entry names.

### `zipBuffer.size`

<!-- YAML
added: REPLACEME
-->

* Type: {number}

The number of entries in the archive.

### `zipBuffer.toBuffer([options])`

<!-- YAML
added: REPLACEME
-->

* `options` {string|Object} An archive comment, as a shorthand for
  `{ comment: options }`.
  * `comment` {string} An archive comment. **Default:** [`zipBuffer.comment`][].
  * `baseOffset` {number} Shifts every offset the archive records by this
    many bytes, so the serialized archive is self-describing even when it is
    written somewhere other than the start of its eventual file - for example,
    after `baseOffset` bytes of other content already written to the same
    output. **Default:** `0`.
* Returns: {Promise} Fulfilled with a {Buffer} containing the serialized
  archive.

Serializes the current set of entries - in the order they were added or
read - into a fresh archive, switching to Zip64 structures automatically as
needed (see [`zlib.createZipArchive()`][]).

### `zipBuffer.toBufferSync([options])`

<!-- YAML
added: REPLACEME
-->

* `options` {string|Object} See [`zipBuffer.toBuffer()`][].
* Returns: {Buffer} The serialized archive.

The synchronous version of [`zipBuffer.toBuffer()`][] (see
[`zlib.createZipArchiveSync()`][]).

### `zipBuffer.values()`

<!-- YAML
added: REPLACEME
-->

* Returns: {Iterator} of [`ZipEntry`][].

### `zipBuffer.writable`

<!-- YAML
added: REPLACEME
-->

* Type: {boolean}

Always `true`.

## Class: `zlib.ZipEntry`

<!-- YAML
added: REPLACEME
-->

> Stability: 1 - Experimental

The ZIP archive API is experimental. The first access of any ZIP export on
`node:zlib` (this class among them) emits an experimental warning.

A single file or directory inside a ZIP archive. Instances are produced by
[`ZipBuffer`][] and [`ZipFile`][], or created directly for writing with
`ZipEntry.create()`/`ZipEntry.createStream()`.

`create()` and `content()` each have a `*Sync` counterpart (the streaming
`contentIterator()` does not). As with the synchronous `node:fs` APIs, these
block the
Node.js event loop and further JavaScript execution until the operation
(including any deflate/inflate pass) completes; use them only where
synchronous execution is appropriate (for example, short-lived scripts or
startup code), not in code that must stay responsive.

### Static method: `zlib.ZipEntry.create(filename, data[, options])`

<!-- YAML
added: REPLACEME
-->

* `filename` {string} The entry's name within the archive. A trailing `/`
  marks a directory entry.
* `data` {Buffer|TypedArray|DataView|ArrayBuffer} The entry's complete,
  uncompressed content. Must be empty when `filename` names a directory.
* `options` {Object}
  * `comment` {string} An entry comment.
  * `mode` {integer} Unix permission bits. **Default:** `0o644` (`0o755` for
    directories).
  * `modified` {Date} The entry's modification time. **Default:** the
    current time.
  * `method` {string} One of `'deflate'`, `'store'`, or `'zstd'`. **Default:**
    `'deflate'`, except for directories and empty content, which are always
    stored.
* Returns: {Promise} Fulfilled with a {ZipEntry}.

Compresses `data` (unless `method` is `'store'`, or compression would not
reduce its size) and computes its CRC-32.

When the entry ends up stored uncompressed (because `method` is `'store'`,
or because compression would not reduce the size), the entry retains a
zero-copy view of `data` rather than a copy, and its CRC-32 has already been
recorded. Do not mutate `data` after creating the entry; pass a copy if it
might change.

The MS-DOS date/time fields ZIP uses for `modified` have 2-second resolution
and no time zone. When `modified` does not fall on a whole 2-second
boundary, an Info-ZIP extended-timestamp extra field is written as well,
recording the whole (UTC) second so the time round-trips more precisely (see
[`zipEntry.modified`][]). This applies to every entry-creation path.

### Static method: `zlib.ZipEntry.createStream(filename, source[, options])`

<!-- YAML
added: REPLACEME
-->

* `filename` {string} The entry's name within the archive. Must not end
  in `/`.
* `source` {AsyncIterable} Yields the entry's uncompressed content as
  `Uint8Array` chunks.
* `options` {Object}
  * `comment` {string} An entry comment.
  * `mode` {integer} Unix permission bits. **Default:** `0o644`.
  * `modified` {Date} The entry's modification time. **Default:** the
    current time.
  * `method` {string} One of `'deflate'`, `'store'`, or `'zstd'`. **Default:**
    `'deflate'`.
* Returns: {ZipEntry}

Creates an entry whose content is compressed on the fly as it is serialized
by [`zlib.createZipArchive()`][], without buffering `source` in memory. Its
`size`, `compressedSize`, and `crc32` only become available once
serialization has finished. There is no synchronous counterpart: streaming
entries only make sense with an asynchronous, incrementally-produced
`source`.

`source` is drained exactly once, during serialization. Until that happens
the entry has no readable content, so [`zipEntry.content()`][],
[`zipEntry.contentSync()`][], and [`zipEntry.contentIterator()`][] throw
[`ERR_INVALID_STATE`][]. If the entry is serialized by adding it to a writable
[`ZipFile`][] with [`zipFile.addEntry()`][] (or `addEntrySync()`), it is then
**promoted in place** to a file-backed entry pointing at the copy just written,
so it becomes readable (and can be serialized again) for as long as that
`ZipFile` stays open. Serializing it any other way (for example directly
through [`zlib.createZipArchive()`][]) leaves it spent and unreadable.

Because `source` may hold an operating-system resource (a file read stream,
say), a streaming entry is disposable: its `Symbol.dispose` and
`Symbol.asyncDispose` methods destroy `source` if it has not been consumed.
An entry passed to an archive is disposed by that archive (see
[`zlib.createZipArchive()`][]); dispose an entry directly only when it was
built but never handed to one. Disposal is a no-op for non-streaming entries -
in particular a file-backed entry never closes the [`ZipFile`][] descriptor it
borrows.

### Static method: `zlib.ZipEntry.createSymlink(filename, target[, options])`

<!-- YAML
added: REPLACEME
-->

* `filename` {string} The entry's name within the archive.
* `target` {string} The symbolic link's target path.
* `options` {Object}
  * `comment` {string} An entry comment.
  * `mode` {integer} Unix permission bits. **Default:** `0o777`.
  * `modified` {Date} The entry's modification time. **Default:** the current
    time.
* Returns: {ZipEntry}

Creates a symbolic-link entry: a stored entry whose content is `target` and
whose Unix mode type bits mark it as a symlink, so [`zipEntry.isSymlink`][] is
`true` when it is read back. Extraction tools that honor symlink entries
recreate the link; treat `target` as untrusted (see [`zipEntry.name`][] on
path safety).

### Static method: `zlib.ZipEntry.createSync(filename, data[, options])`

<!-- YAML
added: REPLACEME
-->

* `filename` {string} The entry's name within the archive. A trailing `/`
  marks a directory entry.
* `data` {Buffer|TypedArray|DataView|ArrayBuffer} The entry's complete,
  uncompressed content. Must be empty when `filename` names a directory.
* `options` {Object} See [`zlib.ZipEntry.create()`][].
* Returns: {ZipEntry}

The synchronous version of [`zlib.ZipEntry.create()`][].

### Static method: `zlib.ZipEntry.read(buffer)`

<!-- YAML
added: REPLACEME
-->

* `buffer` {Buffer|TypedArray|DataView|ArrayBuffer} A complete ZIP archive.
* Returns: {Iterator} of {ZipEntry}.

Parses every entry out of `buffer` directly, without indexing it into a
[`ZipBuffer`][]. Like [`ZipBuffer`][], the yielded entries hold zero-copy views
of `buffer` rather than copies of their content, so the same rule applies: do
not mutate or reuse `buffer` while any of them is still in use.

### `zipEntry.comment`

<!-- YAML
added: REPLACEME
-->

* Type: {string}

### `zipEntry.compressed`

<!-- YAML
added: REPLACEME
-->

* Type: {boolean}

`true` if the entry's content is stored in compressed form (any compression
method, currently deflate or Zstandard); `false` if it is stored
uncompressed.

### `zipEntry.compressedSize`

<!-- YAML
added: REPLACEME
-->

* Type: {number}

### `zipEntry.content([options])`

<!-- YAML
added: REPLACEME
-->

* `options` {Object}
  * `verify` {boolean} Verify the entry's CRC-32 checksum. **Default:** `true`.
  * `maxSize` {number} Reject content declaring more than this many
    uncompressed bytes, before allocating anything. **Default:**
    [`zlib.getMaxZipContentSize()`][].
* Returns: {Promise} Fulfilled with a {Buffer} containing the entry's
  decompressed content. The buffer is a fresh copy that shares no memory
  with the archive or with data the entry was created from.

Throws an [`ERR_ZIP_ENTRY_TOO_LARGE`][] error if the entry's declared size
exceeds `maxSize`, an [`ERR_ZIP_ENTRY_CORRUPT`][] error if the content fails
CRC-32 verification or does not match its declared size, and an
[`ERR_INVALID_STATE`][] error for a streaming entry
([`zlib.ZipEntry.createStream()`][]) whose content is not yet available (see
that method for when a streaming entry becomes readable).

### `zipEntry.contentSync([options])`

<!-- YAML
added: REPLACEME
-->

* `options` {Object} See [`zipEntry.content()`][].
* Returns: {Buffer} The entry's decompressed content.

The synchronous version of [`zipEntry.content()`][].

### `zipEntry.contentIterator([options])`

<!-- YAML
added: REPLACEME
-->

* `options` {Object}
  * `verify` {boolean} Verify the entry's CRC-32 checksum. **Default:** `true`.
  * `maxSize` {number} Reject content declaring more than this many
    uncompressed bytes. **Default:** no limit.
* Returns: {AsyncIterator} of {Buffer} chunks of the entry's decompressed
  content.

Unlike [`zipEntry.content()`][], this does not buffer the whole member in
memory. For a file-backed entry (one returned by [`zipFile.get()`][]) the
compressed bytes are read from disk as the iterator is consumed and nothing is
retained; the entry is valid only while its `ZipFile` is open.

For an in-memory entry stored without compression, the yielded chunks are
zero-copy views of the entry's retained content (see
[`zipEntry.rawContent`][]); do not mutate them.

### `zipEntry.crc32`

<!-- YAML
added: REPLACEME
-->

* Type: {number}

### `zipEntry.flags`

<!-- YAML
added: REPLACEME
-->

* Type: {number}

The entry's raw general-purpose bit flag.

### `zipEntry.isDirectory`

<!-- YAML
added: REPLACEME
-->

* Type: {boolean}

`true` if the entry is a directory (its name ends with `/`).

### `zipEntry.isFile`

<!-- YAML
added: REPLACEME
-->

* Type: {boolean}

`true` if the entry is a regular file — that is, neither a directory nor a
symbolic link.

### `zipEntry.isSymlink`

<!-- YAML
added: REPLACEME
-->

* Type: {boolean}

`true` if the entry is a symbolic link (its Unix mode type bits are
`S_IFLNK`); its content is the link target. Always `false` for archives not
written on a Unix-like system. When extracting, treat a symlink's target as
untrusted — see [`zipEntry.name`][] on path safety.

### `zipEntry.mode`

<!-- YAML
added: REPLACEME
-->

* Type: {number}

The entry's Unix mode permission bits, including the setuid, setgid, and
sticky bits (the low 12 bits, `0o7777`), or `0` if the archive was not written
on a Unix-like system. The file-type bits are not included here; use
[`zipEntry.isDirectory`][] / [`zipEntry.isSymlink`][] for the type.

### `zipEntry.modified`

<!-- YAML
added: REPLACEME
-->

* Type: {Date}

The entry's last-modification time. When the archive carries a higher-fidelity
timestamp in an extra field — an NTFS (`0x000a`), Info-ZIP extended (`0x5455`),
or Info-ZIP Unix (`0x5855`) field, as most modern tools write — that absolute
(UTC) time is used; otherwise the coarse, local-time MS-DOS date/time field
(2-second resolution) is used.

Some tools store their high-fidelity timestamp only in the local file header,
so on a file-backed entry (one returned by [`zipFile.get()`][]) the first read
of this property may perform a small synchronous positioned disk read to
resolve that header. If that read fails, the value silently falls back to the
central-directory data.

### `zipEntry.method`

<!-- YAML
added: REPLACEME
-->

* Type: {number}

The entry's raw compression method: `0` for stored, `8` for deflate, `93`
for Zstandard.

### `zipEntry.name`

<!-- YAML
added: REPLACEME
-->

* Type: {string}

The entry's name, decoded from the central directory, which is treated as
authoritative — a local file header that disagrees is ignored, so a
mismatched-header ("ZIP-confusion") archive cannot make `name` disagree with
what is read. The bytes are decoded from a valid Info-ZIP Unicode Path extra
field (`0x7075`) when one is present; otherwise as UTF-8 when the
language-encoding flag (general-purpose bit 11) is set **or the bytes are
valid UTF-8** (plenty of tools wrote UTF-8 names without ever setting the
flag); and as CP437 — the historical default — only when they are not.
See [`zipEntry.nameBuffer`][] for the raw bytes.

The name is returned **verbatim**: it is never normalized, and a name
containing `..`, a leading `/`, a drive letter, or backslashes is neither
rewritten nor rejected. A `ZipFile`/`ZipBuffer` never writes to disk, so
guarding against path traversal ("Zip Slip") when extracting is the caller's
responsibility.

### `zipEntry.nameBuffer`

<!-- YAML
added: REPLACEME
-->

* Type: {Buffer}

The entry's raw name bytes, before any character decoding. Useful when the
archive's names are in an encoding other than UTF-8 or CP437 and the caller
wants to decode them itself.

### `zipEntry.rawContent`

<!-- YAML
added: REPLACEME
-->

* Type: {Buffer|null}

The entry's raw (still compressed, if applicable) content when it is held in
memory, or `null` when there is no in-memory buffer to expose - for an entry
created with [`zlib.ZipEntry.createStream()`][], or a file-backed entry
returned by [`zipFile.get()`][], whose bytes are read from disk on demand
rather than retained. Use [`zipEntry.content()`][] or
[`zipEntry.contentIterator()`][] to read a file-backed entry.

### `zipEntry.size`

<!-- YAML
added: REPLACEME
-->

* Type: {number}

The entry's uncompressed size, in bytes.

## Class: `zlib.ZipFile`

<!-- YAML
added: REPLACEME
-->

> Stability: 1 - Experimental

The ZIP archive API is experimental. The first access of any ZIP export on
`node:zlib` (this class among them) emits an experimental warning.

A random-access view over the entries of a ZIP archive on disk. Only the
archive's tail and central directory are read up front; member content is
read from disk lazily, on demand. Writable when opened with
`{ writable: true }`: [`zipFile.addEntry()`][]/[`zipFile.add()`][] append the
new member's data where the central directory used to be, then rewrite the
central directory immediately after it; [`zipFile.delete()`][] just rewrites
the central directory. Both mean the file is altered as soon as the method's
returned `Promise` fulfills. Deleted or replaced members are left behind as
dead space; [`zipFile.compact()`][] produces a stream with none.

Every method has a `*Sync` counterpart. As with the synchronous `node:fs`
APIs, these block the Node.js event loop and further JavaScript execution
until the operation completes; use them only where synchronous execution is
appropriate (for example, short-lived scripts or startup code), not in code
that must stay responsive. A synchronous method throws `ERR_INVALID_STATE`
if called while an asynchronous `add()`, `addEntry()`, `delete()`, or
`close()` on the same `ZipFile` has not settled yet, since letting the two
interleave could corrupt the archive.

```mjs
import { ZipFile } from 'node:zlib';
import { Buffer } from 'node:buffer';

const zip = await ZipFile.open('archive.zip', { writable: true });
try {
  const entry = await zip.get('member.txt');
  console.log((await entry.content()).toString());
  for await (const chunk of await zip.stream('huge.bin')) {
    // Process each chunk without buffering the whole member.
  }
  await zip.add('new.txt', Buffer.from('hello'));
  await zip.delete('unwanted.txt');
} finally {
  await zip.close();
}
```

```cjs
const { ZipFile } = require('node:zlib');

async function main() {
  const zip = await ZipFile.open('archive.zip', { writable: true });
  try {
    const entry = await zip.get('member.txt');
    console.log((await entry.content()).toString());
    for await (const chunk of await zip.stream('huge.bin')) {
      // Process each chunk without buffering the whole member.
    }
    await zip.add('new.txt', Buffer.from('hello'));
    await zip.delete('unwanted.txt');
  } finally {
    await zip.close();
  }
}
main();
```

### Static method: `zlib.ZipFile.open(filename[, options])`

<!-- YAML
added: REPLACEME
-->

* `filename` {string}
* `options` {Object}
  * `writable` {boolean} Open the underlying file for both reading and
    writing (`'r+'`), enabling [`zipFile.addEntry()`][]/[`zipFile.add()`][]/
    [`zipFile.delete()`][]. **Default:** `false`.
* Returns: {Promise} Fulfilled with a {ZipFile}.

Throws an [`ERR_ZIP_ARCHIVE_TOO_LARGE`][] error if the archive's central
directory is too large to buffer in memory.

### Static method: `zlib.ZipFile.openSync(filename[, options])`

<!-- YAML
added: REPLACEME
-->

* `filename` {string}
* `options` {Object} See [`zlib.ZipFile.open()`][].
* Returns: {ZipFile}

The synchronous version of [`zlib.ZipFile.open()`][].

### `zipFile.add(filename, data[, options])`

<!-- YAML
added: REPLACEME
-->

* `filename` {string} The entry's name within the archive. A trailing `/`
  marks a directory entry.
* `data` {Buffer|TypedArray|DataView|ArrayBuffer} The entry's complete,
  uncompressed content.
* `options` {Object} See [`zlib.ZipEntry.create()`][].
* Returns: {Promise} Fulfilled with the created {ZipEntry}.

Equivalent to `zipFile.addEntry(await zlib.ZipEntry.create(filename, data,
options))`.

### `zipFile.addEntry(entry)`

<!-- YAML
added: REPLACEME
-->

* `entry` {ZipEntry}
* Returns: {Promise} Fulfilled with `entry`.

Writes `entry` where the central directory currently starts, then rewrites
the central directory to include it, replacing any existing entry of the
same name. Throws [`ERR_ZIP_NOT_WRITABLE`][] if the `ZipFile` was not opened
with `{ writable: true }`.

The returned (same) `entry` is left readable: a streaming entry created with
[`zlib.ZipEntry.createStream()`][], which would otherwise be spent once
serialized, is promoted in place to a file-backed entry pointing at the copy
just written (valid while this `ZipFile` is open). In-memory entries keep their
own buffer unchanged.

### `zipFile.addEntrySync(entry)`

<!-- YAML
added: REPLACEME
-->

* `entry` {ZipEntry}
* Returns: {ZipEntry} `entry`.

The synchronous version of [`zipFile.addEntry()`][]. `entry` must not be a
pending streaming entry (one created with
[`zlib.ZipEntry.createStream()`][]) - there is no synchronous way to drain
its asynchronous source.

### `zipFile.addSync(filename, data[, options])`

<!-- YAML
added: REPLACEME
-->

* `filename` {string} The entry's name within the archive. A trailing `/`
  marks a directory entry.
* `data` {Buffer|TypedArray|DataView|ArrayBuffer} The entry's complete,
  uncompressed content.
* `options` {Object} See [`zlib.ZipEntry.createSync()`][].
* Returns: {ZipEntry} The created entry.

The synchronous version of [`zipFile.add()`][]. Equivalent to
`zipFile.addEntrySync(zlib.ZipEntry.createSync(filename, data, options))`.

### `zipFile.close()`

<!-- YAML
added: REPLACEME
-->

* Returns: {Promise}

Closes the underlying file handle.

Closing does not invalidate outstanding objects: `ZipEntry` objects previously
returned by [`zipFile.get()`][] and the `ZipFile`'s own methods will fail with
system-level errors (for example `EBADF`) if used after close, rather than a
dedicated Node.js error code. The same applies to [`zipFile.closeSync()`][].

### `zipFile.closeSync()`

<!-- YAML
added: REPLACEME
-->

The synchronous version of [`zipFile.close()`][].

### `zipFile.comment`

<!-- YAML
added: REPLACEME
-->

* Type: {string}

The archive-level comment, preserved byte-for-byte across
[`zipFile.addEntry()`][]/[`zipFile.delete()`][] calls. The bytes are decoded
as UTF-8 when they are valid UTF-8 and as CP437 otherwise (the field carries
no encoding flag of its own).

### `zipFile.compact([comment])`

<!-- YAML
added: REPLACEME
-->

* `comment` {string} An archive comment. **Default:** [`zipFile.comment`][].
* Returns: {stream.Readable} A stream of the currently live entries,
  serialized as a fresh archive with no dead space left by prior
  [`zipFile.addEntry()`][]/[`zipFile.delete()`][] calls.

Does not modify the open file; pipe the result into a new one:

```mjs
import { createWriteStream } from 'node:fs';
zip.compact().pipe(createWriteStream('compacted.zip'));
```

### `zipFile.compactSync([comment])`

<!-- YAML
added: REPLACEME
-->

* `comment` {string} An archive comment. **Default:** [`zipFile.comment`][].
* Returns: {Buffer} The currently live entries, serialized as a fresh
  archive with no dead space left by prior
  [`zipFile.addEntry()`][]/[`zipFile.delete()`][] calls.

The synchronous version of [`zipFile.compact()`][]. Does not modify the
open file.

### `zipFile.delete(name)`

<!-- YAML
added: REPLACEME
-->

* `name` {string}
* Returns: {Promise} Fulfilled with `true` if an entry named `name` existed
  and was removed, `false` otherwise.

Rewrites the central directory without writing any new content - the
archive does not grow. Throws [`ERR_ZIP_NOT_WRITABLE`][] if the `ZipFile` was
not opened with `{ writable: true }`.

### `zipFile.deleteSync(name)`

<!-- YAML
added: REPLACEME
-->

* `name` {string}
* Returns: {boolean} `true` if an entry named `name` existed and was
  removed, `false` otherwise.

The synchronous version of [`zipFile.delete()`][].

### `zipFile.entries()`

<!-- YAML
added: REPLACEME
-->

* Returns: {Iterator} of `[name, entry]` pairs, where `entry` is a
  {Promise} fulfilled with a [`ZipEntry`][].

### `zipFile.entriesSync()`

<!-- YAML
added: REPLACEME
-->

* Returns: {Iterator} of `[name, entry]` pairs, where `entry` is a resolved
  [`ZipEntry`][] (not a `Promise`).

The synchronous version of [`zipFile.entries()`][].

### `zipFile.forEach(callback[, thisArg])`

<!-- YAML
added: REPLACEME
-->

* `callback` {Function}
* `thisArg` {any}

### `zipFile.forEachSync(callback[, thisArg])`

<!-- YAML
added: REPLACEME
-->

* `callback` {Function}
* `thisArg` {any}

The synchronous version of [`zipFile.forEach()`][]: `callback` is invoked
with a resolved [`ZipEntry`][] instead of a `Promise`.

### `zipFile.get(name)`

<!-- YAML
added: REPLACEME
-->

* `name` {string}
* Returns: {Promise} Fulfilled with a {ZipEntry}.

Returns a lazy, file-backed [`ZipEntry`][] for `name`. Nothing is read from
disk here and no content is buffered: the returned entry reads (and, for
[`zipEntry.content()`][], decompresses) its member straight from the file on
each access, and the `ZipFile` retains no member content. The entry is valid
only while this `ZipFile` is open. Reading its content later may throw
[`ERR_ZIP_ENTRY_TOO_LARGE`][] if the member is too large to hold in a single
buffer; use [`zipEntry.contentIterator()`][] (or [`zipFile.stream()`][])
instead. Throws [`ERR_ZIP_ENTRY_NOT_FOUND`][] if the archive has no entry
named `name`.

### `zipFile.getSync(name)`

<!-- YAML
added: REPLACEME
-->

* `name` {string}
* Returns: {ZipEntry}

The synchronous version of [`zipFile.get()`][]. Like `get()`, it reads
nothing up front and only builds the lazy handle, so it does not itself block
on I/O - but reads performed later through the returned entry (such as
[`zipEntry.contentSync()`][]) do; see the note above on synchronous methods.

### `zipFile.has(name)`

<!-- YAML
added: REPLACEME
-->

* `name` {string}
* Returns: {boolean}

### `zipFile.keys()`

<!-- YAML
added: REPLACEME
-->

* Returns: {Iterator} of entry names.

### `zipFile.size`

<!-- YAML
added: REPLACEME
-->

* Type: {number}

The number of entries in the archive.

### `zipFile.stream(name[, options])`

<!-- YAML
added: REPLACEME
-->

* `name` {string}
* `options` {Object}
  * `verify` {boolean} Verify the entry's CRC-32 checksum. **Default:** `true`.
  * `maxSize` {number} Reject content declaring more than this many
    uncompressed bytes. **Default:** no limit.
* Returns: {Promise} Fulfilled with a {stream.Readable} of the member's
  decompressed content, without buffering the whole member in memory.

Convenience wrapper that resolves to a `Readable` over
[`zipEntry.contentIterator()`][] of [`zipFile.get()`][]`(name)`; the
compressed bytes are read from disk as the stream is consumed. The returned
promise rejects with [`ERR_ZIP_ENTRY_NOT_FOUND`][] if the archive has no entry
named `name`.

### `zipFile.values()`

<!-- YAML
added: REPLACEME
-->

* Returns: {Iterator} of {Promise} objects, each fulfilled with a
  [`ZipEntry`][].

### `zipFile.valuesSync()`

<!-- YAML
added: REPLACEME
-->

* Returns: {Iterator} of resolved [`ZipEntry`][] values (not `Promise`s).

The synchronous version of [`zipFile.values()`][].

### `zipFile.writable`

<!-- YAML
added: REPLACEME
-->

* Type: {boolean}

Whether this `ZipFile` was opened with `{ writable: true }`.

## Class: `zlib.ZlibBase`

<!-- YAML
added: v0.5.8
changes:
  - version:
     - v11.7.0
     - v10.16.0
    pr-url: https://github.com/nodejs/node/pull/24939
    description: This class was renamed from `Zlib` to `ZlibBase`.
-->

* Extends: [`stream.Transform`][]

Not exported by the `node:zlib` module. It is documented here because it is the
base class of the compressor/decompressor classes.

This class inherits from [`stream.Transform`][], allowing `node:zlib` objects to
be used in pipes and similar stream operations.

### `zlib.bytesWritten`

<!-- YAML
added: v10.0.0
-->

* Type: {number}

The `zlib.bytesWritten` property specifies the number of bytes written to
the engine, before the bytes are processed (compressed or decompressed,
as appropriate for the derived class).

### `zlib.close([callback])`

<!-- YAML
added: v0.9.4
-->

* `callback` {Function}

Close the underlying handle.

### `zlib.flush([kind, ]callback)`

<!-- YAML
added: v0.5.8
-->

* `kind` **Default:** `zlib.constants.Z_FULL_FLUSH` for zlib-based streams,
  `zlib.constants.BROTLI_OPERATION_FLUSH` for Brotli-based streams.
* `callback` {Function}

Flush pending data. Don't call this frivolously, premature flushes negatively
impact the effectiveness of the compression algorithm.

Calling this only flushes data from the internal `zlib` state, and does not
perform flushing of any kind on the streams level. Rather, it behaves like a
normal call to `.write()`, i.e. it will be queued up behind other pending
writes and will only produce output when data is being read from the stream.

### `zlib.params(level, strategy, callback)`

<!-- YAML
added: v0.11.4
-->

* `level` {integer}
* `strategy` {integer}
* `callback` {Function}

This function is only available for zlib-based streams, i.e. not Brotli.

Dynamically update the compression level and compression strategy.
Only applicable to deflate algorithm.

### `zlib.reset()`

<!-- YAML
added: v0.7.0
-->

Reset the compressor/decompressor to factory defaults. Only applicable to
the inflate and deflate algorithms.

## Class: `ZstdOptions`

> Stability: 1 - Experimental

<!-- YAML
added:
  - v23.8.0
  - v22.15.0
changes:
  - version: REPLACEME
    pr-url: https://github.com/nodejs/node/pull/64599
    description: The `dictionary` option can be a `TypedArray`, `DataView`, or
                 `ArrayBuffer`.
  - version: v26.5.0
    pr-url: https://github.com/nodejs/node/pull/64023
    description: The `rejectGarbageAfterEnd` option was added.
-->

<!--type=misc-->

Each Zstd-based class takes an `options` object. All options are optional.

* `flush` {integer} **Default:** `zlib.constants.ZSTD_e_continue`
* `finishFlush` {integer} **Default:** `zlib.constants.ZSTD_e_end`
* `chunkSize` {integer} **Default:** `16 * 1024`
* `params` {Object} Key-value object containing indexed [Zstd parameters][].
* `maxOutputLength` {integer} Limits output size when using
  [convenience methods][]. **Default:** [`buffer.kMaxLength`][]
* `info` {boolean} If `true`, returns an object with `buffer` and `engine`. **Default:** `false`
* `dictionary` {Buffer|TypedArray|DataView|ArrayBuffer} Optional dictionary used
  to improve compression efficiency when compressing or decompressing data that
  shares common patterns with the dictionary.
* `rejectGarbageAfterEnd` {boolean} If `true`, decompression fails when
  input remains after the first complete compressed stream. **Default:** `false`

For example:

```js
const stream = zlib.createZstdCompress({
  chunkSize: 32 * 1024,
  params: {
    [zlib.constants.ZSTD_c_compressionLevel]: 10,
    [zlib.constants.ZSTD_c_checksumFlag]: 1,
  },
});
```

## Class: `zlib.ZstdCompress`

> Stability: 1 - Experimental

<!-- YAML
added:
  - v23.8.0
  - v22.15.0
-->

Compress data using the Zstd algorithm.

## Class: `zlib.ZstdDecompress`

> Stability: 1 - Experimental

<!-- YAML
added:
  - v23.8.0
  - v22.15.0
-->

Decompress data using the Zstd algorithm.

## `zlib.constants`

<!-- YAML
added: v7.0.0
-->

Provides an object enumerating Zlib-related constants.

## `zlib.crc32(data[, value])`

<!-- YAML
added:
  - v22.2.0
  - v20.15.0
-->

* `data` {string|Buffer|TypedArray|DataView} When `data` is a string,
  it will be encoded as UTF-8 before being used for computation.
* `value` {integer} An optional starting value. It must be a 32-bit unsigned
  integer. **Default:** `0`
* Returns: {integer} A 32-bit unsigned integer containing the checksum.

Computes a 32-bit [Cyclic Redundancy Check][] checksum of `data`. If
`value` is specified, it is used as the starting value of the checksum,
otherwise, 0 is used as the starting value.

The CRC algorithm is designed to compute checksums and to detect error
in data transmission. It's not suitable for cryptographic authentication.

To be consistent with other APIs, if the `data` is a string, it will
be encoded with UTF-8 before being used for computation. If users only
use Node.js to compute and match the checksums, this works well with
other APIs that uses the UTF-8 encoding by default.

Some third-party JavaScript libraries compute the checksum on a
string based on `str.charCodeAt()` so that it can be run in browsers.
If users want to match the checksum computed with this kind of library
in the browser, it's better to use the same library in Node.js
if it also runs in Node.js. If users have to use `zlib.crc32()` to
match the checksum produced by such a third-party library:

1. If the library accepts `Uint8Array` as input, use `TextEncoder`
   in the browser to encode the string into a `Uint8Array` with UTF-8
   encoding, and compute the checksum based on the UTF-8 encoded string
   in the browser.
2. If the library only takes a string and compute the data based on
   `str.charCodeAt()`, on the Node.js side, convert the string into
   a buffer using `Buffer.from(str, 'utf16le')`.

```mjs
import zlib from 'node:zlib';
import { Buffer } from 'node:buffer';

let crc = zlib.crc32('hello');  // 907060870
crc = zlib.crc32('world', crc);  // 4192936109

crc = zlib.crc32(Buffer.from('hello', 'utf16le'));  // 1427272415
crc = zlib.crc32(Buffer.from('world', 'utf16le'), crc);  // 4150509955
```

```cjs
const zlib = require('node:zlib');
const { Buffer } = require('node:buffer');

let crc = zlib.crc32('hello');  // 907060870
crc = zlib.crc32('world', crc);  // 4192936109

crc = zlib.crc32(Buffer.from('hello', 'utf16le'));  // 1427272415
crc = zlib.crc32(Buffer.from('world', 'utf16le'), crc);  // 4150509955
```

## `zlib.createBrotliCompress([options])`

<!-- YAML
added:
 - v11.7.0
 - v10.16.0
-->

* `options` {brotli options}

Creates and returns a new [`BrotliCompress`][] object.

## `zlib.createBrotliDecompress([options])`

<!-- YAML
added:
 - v11.7.0
 - v10.16.0
-->

* `options` {brotli options}

Creates and returns a new [`BrotliDecompress`][] object.

## `zlib.createDeflate([options])`

<!-- YAML
added: v0.5.8
-->

* `options` {zlib options}

Creates and returns a new [`Deflate`][] object.

## `zlib.createDeflateRaw([options])`

<!-- YAML
added: v0.5.8
-->

* `options` {zlib options}

Creates and returns a new [`DeflateRaw`][] object.

An upgrade of zlib from 1.2.8 to 1.2.11 changed behavior when `windowBits`
is set to 8 for raw deflate streams. zlib would automatically set `windowBits`
to 9 if was initially set to 8. Newer versions of zlib will throw an exception,
so Node.js restored the original behavior of upgrading a value of 8 to 9,
since passing `windowBits = 9` to zlib actually results in a compressed stream
that effectively uses an 8-bit window only.

## `zlib.createGunzip([options])`

<!-- YAML
added: v0.5.8
-->

* `options` {zlib options}

Creates and returns a new [`Gunzip`][] object.

## `zlib.createGzip([options])`

<!-- YAML
added: v0.5.8
-->

* `options` {zlib options}

Creates and returns a new [`Gzip`][] object.
See [example][zlib.createGzip example].

## `zlib.createInflate([options])`

<!-- YAML
added: v0.5.8
-->

* `options` {zlib options}

Creates and returns a new [`Inflate`][] object.

## `zlib.createInflateRaw([options])`

<!-- YAML
added: v0.5.8
-->

* `options` {zlib options}

Creates and returns a new [`InflateRaw`][] object.

## `zlib.createUnzip([options])`

<!-- YAML
added: v0.5.8
-->

* `options` {zlib options}

Creates and returns a new [`Unzip`][] object.

## `zlib.createZipArchive(entries[, options])`

<!-- YAML
added: REPLACEME
-->

> Stability: 1 - Experimental

The ZIP archive API is experimental. The first access of any ZIP export on
`node:zlib` (this function among them) emits an experimental warning.

* `entries` {Iterable|AsyncIterable} of [`ZipEntry`][].
* `options` {string|Object} An archive comment, as a shorthand for
  `{ comment: options }`.
  * `comment` {string} An archive comment.
  * `baseOffset` {number} Shifts every local/central header offset the
    archive records by this many bytes, so the emitted stream is
    self-describing even when something else is written before it - for
    example, appending the archive after `baseOffset` bytes already written to
    the same file, rather than at its start. **Default:** `0`.
* Returns: {stream.Readable} A byte stream of the serialized archive.

Serializes `entries` into a ZIP archive, switching to Zip64 structures
automatically once the entry count, or any offset or size, exceeds what the
classic 32-/16-bit ZIP fields can hold. The returned `Readable` is also an
`AsyncIterable` of the same {Buffer} chunks it streams.

Entries are written in iteration order and nothing deduplicates names: an
iterable that yields two entries with the same name produces an archive
containing both, and most extraction tools keep the one that appears later.
[`ZipBuffer`][] and [`ZipFile`][] `add()` methods replace entries by name
instead.

The entries are owned by the returned stream: each is consumed as the archive
is produced and must not be reused afterwards. This matters for streaming
entries (from [`zlib.ZipEntry.createStream()`][]), which hold an underlying
source such as a file read stream. If the returned stream is destroyed before
it is fully consumed - for example, the destination of a [`pipeline()`][]
fails - it disposes the entry it was serializing and every entry still queued
behind it, destroying their sources so no descriptor leaks. Consume the stream
to the end, or destroy it (directly, through a failed `pipeline()`, or with
`await using`), to guarantee this cleanup; a stream that is neither consumed
nor destroyed cannot release anything. A [`ZipEntry`][] that is never handed to
an archive can be released directly with `Symbol.dispose` / `Symbol.asyncDispose`.

Throws an [`ERR_ZIP_ARCHIVE_TOO_LARGE`][] error if the archive comment
exceeds 65,535 bytes when encoded as UTF-8.

```mjs
import { createWriteStream } from 'node:fs';
import { pipeline } from 'node:stream/promises';
import { Buffer } from 'node:buffer';
import { ZipEntry, createZipArchive } from 'node:zlib';

const entries = [
  await ZipEntry.create('hello.txt', Buffer.from('Hello, world!')),
  await ZipEntry.create('data/', Buffer.alloc(0)),
];
await pipeline(
  createZipArchive(entries, 'created by node:zlib'),
  createWriteStream('archive.zip'),
);
```

```cjs
const { createWriteStream } = require('node:fs');
const { pipeline } = require('node:stream/promises');
const { ZipEntry, createZipArchive } = require('node:zlib');

async function main() {
  const entries = [
    await ZipEntry.create('hello.txt', Buffer.from('Hello, world!')),
    await ZipEntry.create('data/', Buffer.alloc(0)),
  ];
  await pipeline(
    createZipArchive(entries, 'created by node:zlib'),
    createWriteStream('archive.zip'),
  );
}
main();
```

Passing `options.baseOffset` produces an archive that is valid immediately
when placed after other content in the same file, without relying on a
reader's self-extracting-archive detection to compensate for the shift:

```mjs
import { createWriteStream } from 'node:fs';
import { Buffer } from 'node:buffer';
import { ZipEntry, createZipArchive } from 'node:zlib';

const prefix = Buffer.from('#!/bin/sh\nexit 0\n');
const entries = [await ZipEntry.create('hello.txt', Buffer.from('Hello, world!'))];
const out = createWriteStream('self-extracting.zip');
out.write(prefix);
createZipArchive(entries, { baseOffset: prefix.byteLength }).pipe(out);
```

## `zlib.createZipArchiveSync(entries[, options])`

<!-- YAML
added: REPLACEME
-->

> Stability: 1 - Experimental

The ZIP archive API is experimental. The first access of any ZIP export on
`node:zlib` (this function among them) emits an experimental warning.

* `entries` {Iterable} of [`ZipEntry`][].
* `options` {string|Object} See [`zlib.createZipArchive()`][].
* Returns: {Iterator} of {Buffer} chunks making up the serialized archive.

The synchronous version of [`zlib.createZipArchive()`][]. Blocks the
Node.js event loop and further JavaScript execution until the whole
archive (including any deflate passes) has been produced; use only where
synchronous execution is appropriate (for example, short-lived scripts or
startup code), not in code that must stay responsive. `entries` must be a
plain (synchronous) `Iterable` - a streaming entry created with
[`zlib.ZipEntry.createStream()`][] throws when its turn to serialize comes
up, since draining its asynchronous source has no synchronous equivalent.

As with [`zlib.createZipArchive()`][], the entries are owned by the returned
iterator and must not be reused. If iteration stops early - including the
throw on a streaming entry - the entry that stopped it and every entry still
queued behind it are disposed, releasing any sources they hold.

## `zlib.zipFiles(files[, options])`

<!-- YAML
added: REPLACEME
-->

> Stability: 1 - Experimental

The ZIP archive API is experimental. The first access of any ZIP export on
`node:zlib` (this function among them) emits an experimental warning.

* `files` {Iterable} of `[sourcePath, entryName]` string pairs. Any iterable
  works — an array, a `Map`, the result of `Object.entries()`, a generator.
* `options` {string|Object}
  * `followSymlinks` {boolean} Resolve a symbolic link and archive the file it
    points to, rather than storing the link itself. **Default:** `true`.
  * `comment` {string} An archive comment; a string `options` is shorthand for
    `{ comment: options }`.
  * `baseOffset` {number} See [`zlib.createZipArchive()`][].
* Returns: {stream.Readable} of {Buffer} chunks making up the serialized
  archive.

Builds an archive from files on disk. For each `[sourcePath, entryName]` pair
it reads `sourcePath` and adds an entry named `entryName`, capturing the file's
Unix mode and modification time. A directory becomes a directory entry; a
regular file's contents are streamed in (as a [`zlib.ZipEntry.createStream()`][]
entry) without being buffered in memory. Directory contents are not walked
recursively — list each path you want included.

When `followSymlinks` is `true` (the default) a symbolic link is resolved and
archived as its target file; when it is `false` the link itself is stored as a
symbolic-link entry whose content is the target path (see
[`zlib.ZipEntry.createSymlink()`][]).

```mjs
import { zipFiles } from 'node:zlib';
import { createWriteStream } from 'node:fs';
import { pipeline } from 'node:stream/promises';

await pipeline(
  zipFiles([
    ['/data/report.pdf', 'report.pdf'],
    ['/data/notes.txt', 'docs/notes.txt'],
  ]),
  createWriteStream('archive.zip'),
);
```

## `zlib.createZstdCompress([options])`

> Stability: 1 - Experimental

<!-- YAML
added:
  - v23.8.0
  - v22.15.0
-->

* `options` {zstd options}

Creates and returns a new [`ZstdCompress`][] object.

## `zlib.createZstdDecompress([options])`

> Stability: 1 - Experimental

<!-- YAML
added:
  - v23.8.0
  - v22.15.0
-->

* `options` {zstd options}

Creates and returns a new [`ZstdDecompress`][] object.

## `zlib.getMaxZipContentSize()`

<!-- YAML
added: REPLACEME
-->

> Stability: 1 - Experimental

The ZIP archive API is experimental. The first access of any ZIP export on
`node:zlib` (this function among them) emits an experimental warning.

* Returns: {number}

The current default ceiling, in bytes, applied by [`zipEntry.content()`][]
when no explicit `maxSize` is given. **Default:** `268435456` (256 MiB).

## `zlib.setMaxZipContentSize(size)`

<!-- YAML
added: REPLACEME
-->

> Stability: 1 - Experimental

The ZIP archive API is experimental. The first access of any ZIP export on
`node:zlib` (this function among them) emits an experimental warning.

* `size` {number}

Sets the default ceiling used by [`zipEntry.content()`][] when no explicit
`maxSize` option is given. This is a guard against zip bombs: an archive
whose central directory declares a member larger than this is rejected
before allocating memory for it. Streaming reads
([`zipEntry.contentIterator()`][], [`zipFile.stream()`][]) are bounded-memory
by design and are not affected by this setting.

## Convenience methods

<!--type=misc-->

All of these take a {Buffer}, {TypedArray}, {DataView}, {ArrayBuffer}, or string
as the first argument, an optional second argument
to supply options to the `zlib` classes and will call the supplied callback
with `callback(error, result)`.

Every method has a `*Sync` counterpart, which accept the same arguments, but
without a callback.

### `zlib.brotliCompress(buffer[, options], callback)`

<!-- YAML
added:
 - v11.7.0
 - v10.16.0
-->

* `buffer` {Buffer|TypedArray|DataView|ArrayBuffer|string}
* `options` {brotli options}
* `callback` {Function}

### `zlib.brotliCompressSync(buffer[, options])`

<!-- YAML
added:
 - v11.7.0
 - v10.16.0
-->

* `buffer` {Buffer|TypedArray|DataView|ArrayBuffer|string}
* `options` {brotli options}

Compress a chunk of data with [`BrotliCompress`][].

### `zlib.brotliDecompress(buffer[, options], callback)`

<!-- YAML
added:
 - v11.7.0
 - v10.16.0
-->

* `buffer` {Buffer|TypedArray|DataView|ArrayBuffer|string}
* `options` {brotli options}
* `callback` {Function}

### `zlib.brotliDecompressSync(buffer[, options])`

<!-- YAML
added:
 - v11.7.0
 - v10.16.0
-->

* `buffer` {Buffer|TypedArray|DataView|ArrayBuffer|string}
* `options` {brotli options}

Decompress a chunk of data with [`BrotliDecompress`][].

### `zlib.deflate(buffer[, options], callback)`

<!-- YAML
added: v0.6.0
changes:
  - version: v9.4.0
    pr-url: https://github.com/nodejs/node/pull/16042
    description: The `buffer` parameter can be an `ArrayBuffer`.
  - version: v8.0.0
    pr-url: https://github.com/nodejs/node/pull/12223
    description: The `buffer` parameter can be any `TypedArray` or `DataView`.
  - version: v8.0.0
    pr-url: https://github.com/nodejs/node/pull/12001
    description: The `buffer` parameter can be an `Uint8Array` now.
-->

* `buffer` {Buffer|TypedArray|DataView|ArrayBuffer|string}
* `options` {zlib options}
* `callback` {Function}

### `zlib.deflateSync(buffer[, options])`

<!-- YAML
added: v0.11.12
changes:
  - version: v9.4.0
    pr-url: https://github.com/nodejs/node/pull/16042
    description: The `buffer` parameter can be an `ArrayBuffer`.
  - version: v8.0.0
    pr-url: https://github.com/nodejs/node/pull/12223
    description: The `buffer` parameter can be any `TypedArray` or `DataView`.
  - version: v8.0.0
    pr-url: https://github.com/nodejs/node/pull/12001
    description: The `buffer` parameter can be an `Uint8Array` now.
-->

* `buffer` {Buffer|TypedArray|DataView|ArrayBuffer|string}
* `options` {zlib options}

Compress a chunk of data with [`Deflate`][].

### `zlib.deflateRaw(buffer[, options], callback)`

<!-- YAML
added: v0.6.0
changes:
  - version: v8.0.0
    pr-url: https://github.com/nodejs/node/pull/12223
    description: The `buffer` parameter can be any `TypedArray` or `DataView`.
  - version: v8.0.0
    pr-url: https://github.com/nodejs/node/pull/12001
    description: The `buffer` parameter can be an `Uint8Array` now.
-->

* `buffer` {Buffer|TypedArray|DataView|ArrayBuffer|string}
* `options` {zlib options}
* `callback` {Function}

### `zlib.deflateRawSync(buffer[, options])`

<!-- YAML
added: v0.11.12
changes:
  - version: v9.4.0
    pr-url: https://github.com/nodejs/node/pull/16042
    description: The `buffer` parameter can be an `ArrayBuffer`.
  - version: v8.0.0
    pr-url: https://github.com/nodejs/node/pull/12223
    description: The `buffer` parameter can be any `TypedArray` or `DataView`.
  - version: v8.0.0
    pr-url: https://github.com/nodejs/node/pull/12001
    description: The `buffer` parameter can be an `Uint8Array` now.
-->

* `buffer` {Buffer|TypedArray|DataView|ArrayBuffer|string}
* `options` {zlib options}

Compress a chunk of data with [`DeflateRaw`][].

### `zlib.gunzip(buffer[, options], callback)`

<!-- YAML
added: v0.6.0
changes:
  - version: v9.4.0
    pr-url: https://github.com/nodejs/node/pull/16042
    description: The `buffer` parameter can be an `ArrayBuffer`.
  - version: v8.0.0
    pr-url: https://github.com/nodejs/node/pull/12223
    description: The `buffer` parameter can be any `TypedArray` or `DataView`.
  - version: v8.0.0
    pr-url: https://github.com/nodejs/node/pull/12001
    description: The `buffer` parameter can be an `Uint8Array` now.
-->

* `buffer` {Buffer|TypedArray|DataView|ArrayBuffer|string}
* `options` {zlib options}
* `callback` {Function}

### `zlib.gunzipSync(buffer[, options])`

<!-- YAML
added: v0.11.12
changes:
  - version: v9.4.0
    pr-url: https://github.com/nodejs/node/pull/16042
    description: The `buffer` parameter can be an `ArrayBuffer`.
  - version: v8.0.0
    pr-url: https://github.com/nodejs/node/pull/12223
    description: The `buffer` parameter can be any `TypedArray` or `DataView`.
  - version: v8.0.0
    pr-url: https://github.com/nodejs/node/pull/12001
    description: The `buffer` parameter can be an `Uint8Array` now.
-->

* `buffer` {Buffer|TypedArray|DataView|ArrayBuffer|string}
* `options` {zlib options}

Decompress a chunk of data with [`Gunzip`][].

### `zlib.gzip(buffer[, options], callback)`

<!-- YAML
added: v0.6.0
changes:
  - version: v9.4.0
    pr-url: https://github.com/nodejs/node/pull/16042
    description: The `buffer` parameter can be an `ArrayBuffer`.
  - version: v8.0.0
    pr-url: https://github.com/nodejs/node/pull/12223
    description: The `buffer` parameter can be any `TypedArray` or `DataView`.
  - version: v8.0.0
    pr-url: https://github.com/nodejs/node/pull/12001
    description: The `buffer` parameter can be an `Uint8Array` now.
-->

* `buffer` {Buffer|TypedArray|DataView|ArrayBuffer|string}
* `options` {zlib options}
* `callback` {Function}

### `zlib.gzipSync(buffer[, options])`

<!-- YAML
added: v0.11.12
changes:
  - version: v9.4.0
    pr-url: https://github.com/nodejs/node/pull/16042
    description: The `buffer` parameter can be an `ArrayBuffer`.
  - version: v8.0.0
    pr-url: https://github.com/nodejs/node/pull/12223
    description: The `buffer` parameter can be any `TypedArray` or `DataView`.
  - version: v8.0.0
    pr-url: https://github.com/nodejs/node/pull/12001
    description: The `buffer` parameter can be an `Uint8Array` now.
-->

* `buffer` {Buffer|TypedArray|DataView|ArrayBuffer|string}
* `options` {zlib options}

Compress a chunk of data with [`Gzip`][].

### `zlib.inflate(buffer[, options], callback)`

<!-- YAML
added: v0.6.0
changes:
  - version: v9.4.0
    pr-url: https://github.com/nodejs/node/pull/16042
    description: The `buffer` parameter can be an `ArrayBuffer`.
  - version: v8.0.0
    pr-url: https://github.com/nodejs/node/pull/12223
    description: The `buffer` parameter can be any `TypedArray` or `DataView`.
  - version: v8.0.0
    pr-url: https://github.com/nodejs/node/pull/12001
    description: The `buffer` parameter can be an `Uint8Array` now.
-->

* `buffer` {Buffer|TypedArray|DataView|ArrayBuffer|string}
* `options` {zlib options}
* `callback` {Function}

### `zlib.inflateSync(buffer[, options])`

<!-- YAML
added: v0.11.12
changes:
  - version: v9.4.0
    pr-url: https://github.com/nodejs/node/pull/16042
    description: The `buffer` parameter can be an `ArrayBuffer`.
  - version: v8.0.0
    pr-url: https://github.com/nodejs/node/pull/12223
    description: The `buffer` parameter can be any `TypedArray` or `DataView`.
  - version: v8.0.0
    pr-url: https://github.com/nodejs/node/pull/12001
    description: The `buffer` parameter can be an `Uint8Array` now.
-->

* `buffer` {Buffer|TypedArray|DataView|ArrayBuffer|string}
* `options` {zlib options}

Decompress a chunk of data with [`Inflate`][].

### `zlib.inflateRaw(buffer[, options], callback)`

<!-- YAML
added: v0.6.0
changes:
  - version: v9.4.0
    pr-url: https://github.com/nodejs/node/pull/16042
    description: The `buffer` parameter can be an `ArrayBuffer`.
  - version: v8.0.0
    pr-url: https://github.com/nodejs/node/pull/12223
    description: The `buffer` parameter can be any `TypedArray` or `DataView`.
  - version: v8.0.0
    pr-url: https://github.com/nodejs/node/pull/12001
    description: The `buffer` parameter can be an `Uint8Array` now.
-->

* `buffer` {Buffer|TypedArray|DataView|ArrayBuffer|string}
* `options` {zlib options}
* `callback` {Function}

### `zlib.inflateRawSync(buffer[, options])`

<!-- YAML
added: v0.11.12
changes:
  - version: v9.4.0
    pr-url: https://github.com/nodejs/node/pull/16042
    description: The `buffer` parameter can be an `ArrayBuffer`.
  - version: v8.0.0
    pr-url: https://github.com/nodejs/node/pull/12223
    description: The `buffer` parameter can be any `TypedArray` or `DataView`.
  - version: v8.0.0
    pr-url: https://github.com/nodejs/node/pull/12001
    description: The `buffer` parameter can be an `Uint8Array` now.
-->

* `buffer` {Buffer|TypedArray|DataView|ArrayBuffer|string}
* `options` {zlib options}

Decompress a chunk of data with [`InflateRaw`][].

### `zlib.unzip(buffer[, options], callback)`

<!-- YAML
added: v0.6.0
changes:
  - version: v9.4.0
    pr-url: https://github.com/nodejs/node/pull/16042
    description: The `buffer` parameter can be an `ArrayBuffer`.
  - version: v8.0.0
    pr-url: https://github.com/nodejs/node/pull/12223
    description: The `buffer` parameter can be any `TypedArray` or `DataView`.
  - version: v8.0.0
    pr-url: https://github.com/nodejs/node/pull/12001
    description: The `buffer` parameter can be an `Uint8Array` now.
-->

* `buffer` {Buffer|TypedArray|DataView|ArrayBuffer|string}
* `options` {zlib options}
* `callback` {Function}

### `zlib.unzipSync(buffer[, options])`

<!-- YAML
added: v0.11.12
changes:
  - version: v9.4.0
    pr-url: https://github.com/nodejs/node/pull/16042
    description: The `buffer` parameter can be an `ArrayBuffer`.
  - version: v8.0.0
    pr-url: https://github.com/nodejs/node/pull/12223
    description: The `buffer` parameter can be any `TypedArray` or `DataView`.
  - version: v8.0.0
    pr-url: https://github.com/nodejs/node/pull/12001
    description: The `buffer` parameter can be an `Uint8Array` now.
-->

* `buffer` {Buffer|TypedArray|DataView|ArrayBuffer|string}
* `options` {zlib options}

Decompress a chunk of data with [`Unzip`][].

### `zlib.zstdCompress(buffer[, options], callback)`

> Stability: 1 - Experimental

<!-- YAML
added:
  - v23.8.0
  - v22.15.0
-->

* `buffer` {Buffer|TypedArray|DataView|ArrayBuffer|string}
* `options` {zstd options}
* `callback` {Function}

### `zlib.zstdCompressSync(buffer[, options])`

> Stability: 1 - Experimental

<!-- YAML
added:
  - v23.8.0
  - v22.15.0
-->

* `buffer` {Buffer|TypedArray|DataView|ArrayBuffer|string}
* `options` {zstd options}

Compress a chunk of data with [`ZstdCompress`][].

### `zlib.zstdDecompress(buffer[, options], callback)`

<!-- YAML
added:
  - v23.8.0
  - v22.15.0
-->

* `buffer` {Buffer|TypedArray|DataView|ArrayBuffer|string}
* `options` {zstd options}
* `callback` {Function}

### `zlib.zstdDecompressSync(buffer[, options])`

> Stability: 1 - Experimental

<!-- YAML
added:
  - v23.8.0
  - v22.15.0
-->

* `buffer` {Buffer|TypedArray|DataView|ArrayBuffer|string}
* `options` {zstd options}

Decompress a chunk of data with [`ZstdDecompress`][].

## Iterable Compression

<!-- YAML
added: v25.9.0
-->

> Stability: 1 - Experimental

The `node:zlib/iter` module provides compression and decompression transforms
for use with the [`node:stream/iter`][] iterable streams API.

This module is available only when the `--experimental-stream-iter` CLI flag
is enabled.

Each algorithm has both an async variant (stateful async generator, for use
with [`pull()`][] and [`pipeTo()`][]) and a sync variant (stateful sync
generator, for use with `pullSync()` and `pipeToSync()`).

The async transforms run compression on the libuv threadpool, overlapping
I/O with JavaScript execution. The sync transforms run compression directly
on the main thread.

> Note: The defaults for these transforms are tuned for streaming throughput,
> and differ from the defaults in `node:zlib`. In particular, gzip/deflate
> default to level 4 (not 6) and memLevel 9 (not 8), and Brotli defaults to
> quality 6 (not 11). These choices match common HTTP server configurations
> and provide significantly faster compression with only a small reduction in
> compression ratio. All defaults can be overridden via options.

```mjs
import { from, pull, bytes, text } from 'node:stream/iter';
import { compressGzip, decompressGzip } from 'node:zlib/iter';

// Async round-trip
const compressed = await bytes(pull(from('hello'), compressGzip()));
const original = await text(pull(from(compressed), decompressGzip()));
console.log(original); // 'hello'
```

```cjs
const { from, pull, bytes, text } = require('node:stream/iter');
const { compressGzip, decompressGzip } = require('node:zlib/iter');

async function run() {
  const compressed = await bytes(pull(from('hello'), compressGzip()));
  const original = await text(pull(from(compressed), decompressGzip()));
  console.log(original); // 'hello'
}

run().catch(console.error);
```

```mjs
import { fromSync, pullSync, textSync } from 'node:stream/iter';
import { compressGzipSync, decompressGzipSync } from 'node:zlib/iter';

// Sync round-trip
const compressed = pullSync(fromSync('hello'), compressGzipSync());
const original = textSync(pullSync(compressed, decompressGzipSync()));
console.log(original); // 'hello'
```

```cjs
const { fromSync, pullSync, textSync } = require('node:stream/iter');
const { compressGzipSync, decompressGzipSync } = require('node:zlib/iter');

const compressed = pullSync(fromSync('hello'), compressGzipSync());
const original = textSync(pullSync(compressed, decompressGzipSync()));
console.log(original); // 'hello'
```

### `compressBrotli([options])`

### `compressBrotliSync([options])`

<!-- YAML
added: v25.9.0
-->

* `options` {Object}
  * `chunkSize` {number} Output buffer size. **Default:** `65536` (64 KB).
  * `params` {Object} Key-value object where keys and values are
    `zlib.constants` entries. The most important compressor parameters are:
    * `BROTLI_PARAM_MODE` -- `BROTLI_MODE_GENERIC` (default),
      `BROTLI_MODE_TEXT`, or `BROTLI_MODE_FONT`.
    * `BROTLI_PARAM_QUALITY` -- ranges from `BROTLI_MIN_QUALITY` to
      `BROTLI_MAX_QUALITY`. **Default:** `6` (not `BROTLI_DEFAULT_QUALITY`
      which is 11). Quality 6 is appropriate for streaming; quality 11 is
      intended for offline/build-time compression.
    * `BROTLI_PARAM_SIZE_HINT` -- expected input size. **Default:** `0`
      (unknown).
    * `BROTLI_PARAM_LGWIN` -- window size (log2). **Default:** `20` (1 MB).
      The Brotli library default is 22 (4 MB); the reduced default saves
      memory without significant compression impact for streaming workloads.
    * `BROTLI_PARAM_LGBLOCK` -- input block size (log2).
      See the [Brotli compressor options][] in the zlib documentation for the
      full list.
  * `dictionary` {Buffer|TypedArray|DataView}
* Returns: {Object} A stateful transform.

Create a Brotli compression transform. Output is compatible with
`zlib.brotliDecompress()` and `decompressBrotli()`/`decompressBrotliSync()`.

### `compressDeflate([options])`

### `compressDeflateSync([options])`

<!-- YAML
added: v25.9.0
-->

* `options` {Object}
  * `chunkSize` {number} Output buffer size. **Default:** `65536` (64 KB).
  * `level` {number} Compression level (`0`-`9`). **Default:** `4`.
  * `windowBits` {number} **Default:** `Z_DEFAULT_WINDOWBITS` (15).
  * `memLevel` {number} **Default:** `9`.
  * `strategy` {number} **Default:** `Z_DEFAULT_STRATEGY`.
  * `dictionary` {Buffer|TypedArray|DataView}
* Returns: {Object} A stateful transform.

Create a deflate compression transform. Output is compatible with
`zlib.inflate()` and `decompressDeflate()`/`decompressDeflateSync()`.

### `compressGzip([options])`

### `compressGzipSync([options])`

<!-- YAML
added: v25.9.0
-->

* `options` {Object}
  * `chunkSize` {number} Output buffer size. **Default:** `65536` (64 KB).
  * `level` {number} Compression level (`0`-`9`). **Default:** `4`.
  * `windowBits` {number} **Default:** `Z_DEFAULT_WINDOWBITS` (15).
  * `memLevel` {number} **Default:** `9`.
  * `strategy` {number} **Default:** `Z_DEFAULT_STRATEGY`.
  * `dictionary` {Buffer|TypedArray|DataView}
* Returns: {Object} A stateful transform.

Create a gzip compression transform. Output is compatible with `zlib.gunzip()`
and `decompressGzip()`/`decompressGzipSync()`.

### `compressZstd([options])`

### `compressZstdSync([options])`

<!-- YAML
added: v25.9.0
-->

* `options` {Object}
  * `chunkSize` {number} Output buffer size. **Default:** `65536` (64 KB).
  * `params` {Object} Key-value object where keys and values are
    `zlib.constants` entries. The most important compressor parameters are:
    * `ZSTD_c_compressionLevel` -- **Default:** `ZSTD_CLEVEL_DEFAULT` (3).
    * `ZSTD_c_checksumFlag` -- generate a checksum. **Default:** `0`.
    * `ZSTD_c_strategy` -- compression strategy. Values include
      `ZSTD_fast`, `ZSTD_dfast`, `ZSTD_greedy`, `ZSTD_lazy`,
      `ZSTD_lazy2`, `ZSTD_btlazy2`, `ZSTD_btopt`, `ZSTD_btultra`,
      `ZSTD_btultra2`.
      See the [Zstd compressor options][] in the zlib documentation for the
      full list.
  * `pledgedSrcSize` {number} Expected uncompressed size as a non-negative safe
    integer (optional hint).
  * `dictionary` {Buffer|TypedArray|DataView}
* Returns: {Object} A stateful transform.

Create a Zstandard compression transform. Output is compatible with
`zlib.zstdDecompress()` and `decompressZstd()`/`decompressZstdSync()`.

### `decompressBrotli([options])`

### `decompressBrotliSync([options])`

<!-- YAML
added: v25.9.0
-->

* `options` {Object}
  * `chunkSize` {number} Output buffer size. **Default:** `65536` (64 KB).
  * `params` {Object} Key-value object where keys and values are
    `zlib.constants` entries. Available decompressor parameters:
    * `BROTLI_DECODER_PARAM_DISABLE_RING_BUFFER_REALLOCATION` -- boolean
      flag affecting internal memory allocation.
    * `BROTLI_DECODER_PARAM_LARGE_WINDOW` -- boolean flag enabling "Large
      Window Brotli" mode (not compatible with [RFC 7932][]).
      See the [Brotli decompressor options][] in the zlib documentation for
      details.
  * `dictionary` {Buffer|TypedArray|DataView}
* Returns: {Object} A stateful transform.

Create a Brotli decompression transform.

### `decompressDeflate([options])`

### `decompressDeflateSync([options])`

<!-- YAML
added: v25.9.0
-->

* `options` {Object}
  * `chunkSize` {number} Output buffer size. **Default:** `65536` (64 KB).
  * `windowBits` {number} **Default:** `Z_DEFAULT_WINDOWBITS` (15).
  * `dictionary` {Buffer|TypedArray|DataView}
* Returns: {Object} A stateful transform.

Create a deflate decompression transform.

### `decompressGzip([options])`

### `decompressGzipSync([options])`

<!-- YAML
added: v25.9.0
-->

* `options` {Object}
  * `chunkSize` {number} Output buffer size. **Default:** `65536` (64 KB).
  * `windowBits` {number} **Default:** `Z_DEFAULT_WINDOWBITS` (15).
  * `dictionary` {Buffer|TypedArray|DataView}
* Returns: {Object} A stateful transform.

Create a gzip decompression transform.

### `decompressZstd([options])`

### `decompressZstdSync([options])`

<!-- YAML
added: v25.9.0
-->

* `options` {Object}
  * `chunkSize` {number} Output buffer size. **Default:** `65536` (64 KB).
  * `params` {Object} Key-value object where keys and values are
    `zlib.constants` entries. Available decompressor parameters:
    * `ZSTD_d_windowLogMax` -- maximum window size (log2) the decompressor
      will allocate. Limits memory usage against malicious input.
      See the [Zstd decompressor options][] in the zlib documentation for
      details.
  * `dictionary` {Buffer|TypedArray|DataView}
* Returns: {Object} A stateful transform.

Create a Zstandard decompression transform.

[Brotli compressor options]: #compressor-options
[Brotli decompressor options]: #decompressor-options
[Brotli parameters]: #brotli-constants
[Cyclic redundancy check]: https://en.wikipedia.org/wiki/Cyclic_redundancy_check
[Memory usage tuning]: #memory-usage-tuning
[RFC 7932]: https://www.rfc-editor.org/rfc/rfc7932.html
[Streams API]: stream.md
[Zstd compressor options]: #compressor-options-1
[Zstd decompressor options]: #decompressor-options-1
[Zstd parameters]: #zstd-constants
[`.flush()`]: #zlibflushkind-callback
[`Accept-Encoding`]: https://www.w3.org/Protocols/rfc2616/rfc2616-sec14.html#sec14.3
[`BrotliCompress`]: #class-zlibbrotlicompress
[`BrotliDecompress`]: #class-zlibbrotlidecompress
[`Content-Encoding`]: https://www.w3.org/Protocols/rfc2616/rfc2616-sec14.html#sec14.11
[`DeflateRaw`]: #class-zlibdeflateraw
[`Deflate`]: #class-zlibdeflate
[`ERR_INVALID_STATE`]: errors.md#err_invalid_state
[`ERR_ZIP_ARCHIVE_TOO_LARGE`]: errors.md#err_zip_archive_too_large
[`ERR_ZIP_ENTRY_CORRUPT`]: errors.md#err_zip_entry_corrupt
[`ERR_ZIP_ENTRY_NOT_FOUND`]: errors.md#err_zip_entry_not_found
[`ERR_ZIP_ENTRY_TOO_LARGE`]: errors.md#err_zip_entry_too_large
[`ERR_ZIP_INVALID_ARCHIVE`]: errors.md#err_zip_invalid_archive
[`ERR_ZIP_NOT_WRITABLE`]: errors.md#err_zip_not_writable
[`ERR_ZIP_UNSUPPORTED_FEATURE`]: errors.md#err_zip_unsupported_feature
[`Gunzip`]: #class-zlibgunzip
[`Gzip`]: #class-zlibgzip
[`InflateRaw`]: #class-zlibinflateraw
[`Inflate`]: #class-zlibinflate
[`Unzip`]: #class-zlibunzip
[`ZipBuffer`]: #class-zlibzipbuffer
[`ZipEntry`]: #class-zlibzipentry
[`ZipFile`]: #class-zlibzipfile
[`ZlibBase`]: #class-zlibzlibbase
[`ZstdCompress`]: #class-zlibzstdcompress
[`ZstdDecompress`]: #class-zlibzstddecompress
[`buffer.kMaxLength`]: buffer.md#bufferkmaxlength
[`deflateInit2` and `inflateInit2`]: https://zlib.net/manual.html#Advanced
[`node:stream/iter`]: stream_iter.md
[`pipeTo()`]: stream_iter.md#pipetosource-transforms-writer-options
[`pipeline()`]: stream.md#streampipelinesource-transforms-destination-callback
[`pull()`]: stream_iter.md#pullsource-transforms-options
[`stream.Transform`]: stream.md#class-streamtransform
[`zipBuffer.add()`]: #zipbufferaddfilename-data-options
[`zipBuffer.addSync()`]: #zipbufferaddsyncfilename-data-options
[`zipBuffer.comment`]: #zipbuffercomment
[`zipBuffer.toBuffer()`]: #zipbuffertobufferoptions
[`zipBuffer.toBufferSync()`]: #zipbuffertobuffersyncoptions
[`zipEntry.content()`]: #zipentrycontentoptions
[`zipEntry.contentIterator()`]: #zipentrycontentiteratoroptions
[`zipEntry.contentSync()`]: #zipentrycontentsyncoptions
[`zipEntry.isDirectory`]: #zipentryisdirectory
[`zipEntry.isSymlink`]: #zipentryissymlink
[`zipEntry.modified`]: #zipentrymodified
[`zipEntry.nameBuffer`]: #zipentrynamebuffer
[`zipEntry.name`]: #zipentryname
[`zipEntry.rawContent`]: #zipentryrawcontent
[`zipFile.add()`]: #zipfileaddfilename-data-options
[`zipFile.addEntry()`]: #zipfileaddentryentry
[`zipFile.close()`]: #zipfileclose
[`zipFile.closeSync()`]: #zipfileclosesync
[`zipFile.comment`]: #zipfilecomment
[`zipFile.compact()`]: #zipfilecompactcomment
[`zipFile.delete()`]: #zipfiledeletename
[`zipFile.entries()`]: #zipfileentries
[`zipFile.forEach()`]: #zipfileforeachcallback-thisarg
[`zipFile.get()`]: #zipfilegetname
[`zipFile.stream()`]: #zipfilestreamname-options
[`zipFile.values()`]: #zipfilevalues
[`zlib.ZipEntry.create()`]: #static-method-zlibzipentrycreatefilename-data-options
[`zlib.ZipEntry.createStream()`]: #static-method-zlibzipentrycreatestreamfilename-source-options
[`zlib.ZipEntry.createSymlink()`]: #static-method-zlibzipentrycreatesymlinkfilename-target-options
[`zlib.ZipEntry.createSync()`]: #static-method-zlibzipentrycreatesyncfilename-data-options
[`zlib.ZipFile.open()`]: #static-method-zlibzipfileopenfilename-options
[`zlib.createZipArchive()`]: #zlibcreateziparchiveentries-options
[`zlib.createZipArchiveSync()`]: #zlibcreateziparchivesyncentries-options
[`zlib.getMaxZipContentSize()`]: #zlibgetmaxzipcontentsize
[convenience methods]: #convenience-methods
[zlib documentation]: https://zlib.net/manual.html#Constants
[zlib.createGzip example]: #zlib

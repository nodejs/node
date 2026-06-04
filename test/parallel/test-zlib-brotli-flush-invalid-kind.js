'use strict';
// Regression test for https://github.com/nodejs/node/issues/63701
// BrotliCompress/BrotliDecompress.flush(kind) used to spin at 100% CPU when
// kind was outside the brotli operation range (0..3) — e.g. Z_FINISH (4) or
// Z_BLOCK (5). It must now throw ERR_OUT_OF_RANGE before reaching the native
// layer.

require('../common');
const assert = require('assert');
const zlib = require('zlib');

const {
  BROTLI_OPERATION_PROCESS,
  BROTLI_OPERATION_FLUSH,
  BROTLI_OPERATION_FINISH,
  BROTLI_OPERATION_EMIT_METADATA,
  Z_BLOCK,
  Z_FINISH,
} = zlib.constants;

// Brotli operations 0..3 are valid and must not throw synchronously from
// flush(). Some operations (e.g. FINISH on a decoder with no input) emit a
// Z_BUF_ERROR asynchronously — that's expected and unrelated to the input
// validation under test, so we attach a noop error handler.
for (const validKind of [
  BROTLI_OPERATION_PROCESS,
  BROTLI_OPERATION_FLUSH,
  BROTLI_OPERATION_FINISH,
  BROTLI_OPERATION_EMIT_METADATA,
]) {
  const c = zlib.createBrotliCompress();
  c.on('error', () => {});
  c.flush(validKind);

  const d = zlib.createBrotliDecompress();
  d.on('error', () => {});
  d.flush(validKind);
}

// Values outside [0, 3] must throw ERR_OUT_OF_RANGE for both compress and
// decompress streams. Z_FINISH (4) and Z_BLOCK (5) previously hung at 100% CPU.
const outOfRange = [-1, Z_FINISH, Z_BLOCK, 6, 7, 100];

for (const factory of [zlib.createBrotliCompress, zlib.createBrotliDecompress]) {
  for (const kind of outOfRange) {
    assert.throws(
      () => factory().flush(kind),
      { code: 'ERR_OUT_OF_RANGE', name: 'RangeError' },
    );
  }
}

// Non-number kinds must also throw, instead of silently triggering the
// fake-chunk path with an undefined buffer.
for (const factory of [zlib.createBrotliCompress, zlib.createBrotliDecompress]) {
  for (const kind of ['foobar', null, {}, NaN]) {
    assert.throws(
      () => factory().flush(kind),
      { code: 'ERR_OUT_OF_RANGE' },
    );
  }
}

// flush() with no arguments (or with only a callback) must still work —
// it uses the stream's _defaultFullFlushFlag, which is a valid brotli op.
zlib.createBrotliCompress().flush();
zlib.createBrotliCompress().flush(() => {});
zlib.createBrotliDecompress().flush();
zlib.createBrotliDecompress().flush(() => {});

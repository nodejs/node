'use strict';
// Regression test for https://github.com/nodejs/node/issues/63701.
// Invalid Brotli flush kinds used to spin in native code. flush(kind) should
// reject invalid kinds before writing the fake flush chunk.

require('../common');
const assert = require('assert');
const zlib = require('zlib');

const {
  BROTLI_OPERATION_PROCESS,
  BROTLI_OPERATION_FLUSH,
  BROTLI_OPERATION_FINISH,
  BROTLI_OPERATION_EMIT_METADATA,
  Z_NO_FLUSH,
  Z_BLOCK,
  Z_FINISH,
  ZSTD_e_continue,
  ZSTD_e_flush,
  ZSTD_e_end,
} = zlib.constants;

const noop = () => {};

const flushKindTestCases = [
  {
    factories: [zlib.createGzip],
    validKinds: [Z_NO_FLUSH, Z_FINISH, Z_BLOCK],
    invalidKinds: [-1, 6, 100],
  },
  {
    factories: [zlib.createBrotliCompress, zlib.createBrotliDecompress],
    validKinds: [
      BROTLI_OPERATION_PROCESS,
      BROTLI_OPERATION_FLUSH,
      BROTLI_OPERATION_FINISH,
      BROTLI_OPERATION_EMIT_METADATA,
    ],
    invalidKinds: [-1, Z_FINISH, Z_BLOCK, 6, 100],
  },
  {
    factories: [zlib.createZstdCompress, zlib.createZstdDecompress],
    validKinds: [ZSTD_e_continue, ZSTD_e_flush, ZSTD_e_end],
    invalidKinds: [-1, 3, Z_FINISH, Z_BLOCK, 100],
  },
];

for (const { factories, validKinds } of flushKindTestCases) {
  for (const factory of factories) {
    for (const kind of validKinds) {
      const stream = factory();
      stream.on('error', noop);
      stream.flush(kind);
    }
  }
}

for (const { factories, invalidKinds } of flushKindTestCases) {
  for (const factory of factories) {
    for (const kind of invalidKinds) {
      assert.throws(
        () => factory().flush(kind),
        { code: 'ERR_OUT_OF_RANGE', name: 'RangeError' },
      );
    }
  }
}

for (const { factories } of flushKindTestCases) {
  for (const factory of factories) {
    for (const kind of ['foobar', null, {}]) {
      assert.throws(
        () => factory().flush(kind),
        { code: 'ERR_INVALID_ARG_TYPE', name: 'TypeError' },
      );
    }
  }
}

for (const { factories } of flushKindTestCases) {
  for (const factory of factories) {
    for (const kind of [undefined, NaN]) {
      const stream = factory();
      stream.on('error', noop);
      stream.flush(kind);
    }

    const stream = factory();
    stream.on('error', noop);
    stream.flush(noop);
  }
}

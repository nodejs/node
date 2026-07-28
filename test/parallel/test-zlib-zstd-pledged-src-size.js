'use strict';
const common = require('../common');
const assert = require('assert');
const zlib = require('zlib');

function compressWithPledgedSrcSize({ pledgedSrcSize, actualSrcSize }) {
  return new Promise((resolve, reject) => {
    const compressor = zlib.createZstdCompress({ pledgedSrcSize });
    compressor.on('error', (e) => {
      reject(e);
    });
    compressor.on('end', resolve);
    compressor.write('x'.repeat(actualSrcSize), () => {
      compressor.end();
      compressor.resume();
    });
  }).then(() => {
    // Compression should only succeed if sizes match
    assert.strictEqual(pledgedSrcSize, actualSrcSize);
  }, (error) => {
    assert.strictEqual(error.code, 'ZSTD_error_srcSize_wrong');
    // Size error should only happen when sizes do not match
    assert.notStrictEqual(pledgedSrcSize, actualSrcSize);
  }).then(common.mustCall());
}

compressWithPledgedSrcSize({ pledgedSrcSize: 0, actualSrcSize: 0 });

compressWithPledgedSrcSize({ pledgedSrcSize: 0, actualSrcSize: 42 });

compressWithPledgedSrcSize({ pledgedSrcSize: 13, actualSrcSize: 42 });

compressWithPledgedSrcSize({ pledgedSrcSize: 42, actualSrcSize: 0 });

compressWithPledgedSrcSize({ pledgedSrcSize: 42, actualSrcSize: 13 });

compressWithPledgedSrcSize({ pledgedSrcSize: 42, actualSrcSize: 42 });

function assertInvalidPledgedSrcSize(pledgedSrcSize, expected) {
  assert.throws(
    () => zlib.createZstdCompress({ pledgedSrcSize }),
    expected,
  );
  assert.throws(
    () => zlib.zstdCompressSync('', { pledgedSrcSize }),
    expected,
  );
}

for (const pledgedSrcSize of ['1', null]) {
  assertInvalidPledgedSrcSize(pledgedSrcSize, {
    name: 'TypeError',
    code: 'ERR_INVALID_ARG_TYPE',
  });
}

for (const pledgedSrcSize of [
  NaN,
  Infinity,
  -Infinity,
  1.9,
  -1,
  Number.MAX_SAFE_INTEGER + 1,
]) {
  assertInvalidPledgedSrcSize(pledgedSrcSize, {
    name: 'RangeError',
    code: 'ERR_OUT_OF_RANGE',
  });
}

zlib.createZstdCompress({
  pledgedSrcSize: Number.MAX_SAFE_INTEGER,
}).destroy();

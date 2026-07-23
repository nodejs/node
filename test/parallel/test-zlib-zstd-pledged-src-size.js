'use strict';
const common = require('../common');
const assert = require('assert');
const zlib = require('zlib');

const pledgedSrcSizeError = {
  code: 'ZSTD_error_srcSize_wrong',
  errno: zlib.constants.ZSTD_error_srcSize_wrong,
};

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
    assert.strictEqual(error.code, pledgedSrcSizeError.code);
    assert.strictEqual(error.errno, pledgedSrcSizeError.errno);
    // Size error should only happen when sizes do not match
    assert.notStrictEqual(pledgedSrcSize, actualSrcSize);
  }).then(common.mustCall());
}

function compressSyncWithPledgedSrcSize({ pledgedSrcSize, actualSrcSize }) {
  const compress = () => zlib.zstdCompressSync(
    'x'.repeat(actualSrcSize),
    { pledgedSrcSize },
  );

  if (pledgedSrcSize === actualSrcSize) {
    compress();
  } else {
    assert.throws(compress, pledgedSrcSizeError);
  }
}

const testCases = [
  { pledgedSrcSize: 0, actualSrcSize: 0 },
  { pledgedSrcSize: 0, actualSrcSize: 42 },
  { pledgedSrcSize: 1, actualSrcSize: 42 },
  { pledgedSrcSize: 13, actualSrcSize: 42 },
  { pledgedSrcSize: 42, actualSrcSize: 0 },
  { pledgedSrcSize: 42, actualSrcSize: 13 },
  { pledgedSrcSize: 42, actualSrcSize: 42 },
];

for (const testCase of testCases) {
  compressWithPledgedSrcSize(testCase);
  compressSyncWithPledgedSrcSize(testCase);
}

const retryInput = Buffer.allocUnsafe(256 * 1024);
let randomState = 0x12345678;
for (let i = 0; i < retryInput.length; i++) {
  randomState = (Math.imul(randomState, 1664525) + 1013904223) | 0;
  retryInput[i] = randomState >>> 24;
}

const compressed = zlib.zstdCompressSync(retryInput, {
  pledgedSrcSize: retryInput.length,
  chunkSize: 64,
});
assert.ok(compressed.length > 64);
assert.deepStrictEqual(zlib.zstdDecompressSync(compressed), retryInput);

assert.throws(() => zlib.zstdCompressSync(retryInput, {
  pledgedSrcSize: retryInput.length - 1,
  chunkSize: 64,
}), pledgedSrcSizeError);

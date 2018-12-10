'use strict';
// Test unzipping a gzip file that has trailing garbage

const common = require('../common');
const assert = require('assert');
const zlib = require('zlib');

// should ignore trailing null-bytes
let data = Buffer.concat([
  zlib.gzipSync('abc'),
  zlib.gzipSync('def'),
  Buffer.alloc(10)
]);

assert.strictEqual(zlib.gunzipSync(data).toString(), 'abcdef');

zlib.gunzip(data, common.mustCall((err, result) => {
  assert.ifError(err);
  assert.strictEqual(
    result.toString(),
    'abcdef',
    `result '${result.toString()}' should match original string`
  );
}));

// If the trailing garbage happens to look like a gzip header, it should
// throw an error.
data = Buffer.concat([
  zlib.gzipSync('abc'),
  zlib.gzipSync('def'),
  Buffer.from([0x1f, 0x8b, 0xff, 0xff]),
  Buffer.alloc(10)
]);

assert.throws(
  () => zlib.gunzipSync(data),
  /^Error: unknown compression method$/
);

zlib.gunzip(data, common.mustCall((err, result) => {
  common.expectsError({
    code: 'Z_DATA_ERROR',
    type: Error,
    message: 'unknown compression method'
  })(err);
  assert.strictEqual(result, undefined);
}));

// In this case the trailing junk is too short to be a gzip segment
// So we ignore it and decompression succeeds.
data = Buffer.concat([
  zlib.gzipSync('abc'),
  zlib.gzipSync('def'),
  Buffer.from([0x1f, 0x8b, 0xff, 0xff])
]);

assert.throws(
  () => zlib.gunzipSync(data),
  /^Error: unknown compression method$/
);

zlib.gunzip(data, common.mustCall((err, result) => {
  assert(err instanceof Error);
  assert.strictEqual(err.code, 'Z_DATA_ERROR');
  assert.strictEqual(err.message, 'unknown compression method');
  assert.strictEqual(result, undefined);
}));

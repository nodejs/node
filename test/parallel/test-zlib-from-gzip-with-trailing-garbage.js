'use strict';
// test unzipping a gzip file that has trailing garbage

const common = require('../common');
const assert = require('assert');
const zlib = require('zlib');

// should ignore trailing null-bytes
let data = Buffer.concat([
  zlib.gzipSync('abc'),
  zlib.gzipSync('def'),
  Buffer(10).fill(0)
]);

assert.equal(zlib.gunzipSync(data).toString(), 'abcdef');

zlib.gunzip(data, common.mustCall((err, result) => {
  assert.ifError(err);
  assert.equal(result, 'abcdef', 'result should match original string');
}));

// if the trailing garbage happens to look like a gzip header, it should
// throw an error.
data = Buffer.concat([
  zlib.gzipSync('abc'),
  zlib.gzipSync('def'),
  Buffer([0x1f, 0x8b, 0xff, 0xff]),
  Buffer(10).fill(0)
]);

assert.throws(() => zlib.gunzipSync(data));

zlib.gunzip(data, common.mustCall((err, result) => {
  assert(err);
}));

// In this case the trailing junk is too short to be a gzip segment
// So we ignore it and decompression succeeds.
data = Buffer.concat([
  zlib.gzipSync('abc'),
  zlib.gzipSync('def'),
  Buffer([0x1f, 0x8b, 0xff, 0xff])
]);

assert.throws(() => zlib.gunzipSync(data));

zlib.gunzip(data, common.mustCall((err, result) => {
  assert(err);
}));

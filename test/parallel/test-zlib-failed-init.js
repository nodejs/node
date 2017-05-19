'use strict';

require('../common');

const assert = require('assert');
const zlib = require('zlib');

// For raw deflate encoding, requests for 256-byte windows are rejected as
// invalid by zlib.
// (http://zlib.net/manual.html#Advanced)
assert.throws(() => {
  zlib.createDeflateRaw({ windowBits: 8 });
}, /^Error: Init error$/);

// Regression tests for bugs in the validation logic.

assert.throws(() => {
  zlib.createGzip({ chunkSize: 0 });
}, /^RangeError: Invalid chunk size: 0$/);

assert.throws(() => {
  zlib.createGzip({ windowBits: 0 });
}, /^RangeError: Invalid windowBits: 0$/);

assert.throws(() => {
  zlib.createGzip({ memLevel: 0 });
}, /^RangeError: Invalid memLevel: 0$/);

{
  const stream = zlib.createGzip({ level: NaN });
  assert.ok(!Number.isNaN(stream._level));
}

{
  const stream = zlib.createGzip({ strategy: NaN });
  assert.ok(!Number.isNaN(stream._strategy));
}

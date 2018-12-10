'use strict';
require('../common');

// This test ensures that zlib throws a RangeError if the final buffer needs to
// be larger than kMaxLength and concatenation fails.
// https://github.com/nodejs/node/pull/1811

const assert = require('assert');

// Change kMaxLength for zlib to trigger the error without having to allocate
// large Buffers.
const buffer = require('buffer');
const oldkMaxLength = buffer.kMaxLength;
buffer.kMaxLength = 128;
const zlib = require('zlib');
buffer.kMaxLength = oldkMaxLength;

const encoded = Buffer.from('G38A+CXCIrFAIAM=', 'base64');

// Async
zlib.brotliDecompress(encoded, function(err) {
  assert.ok(err instanceof RangeError);
});

// Sync
assert.throws(function() {
  zlib.brotliDecompressSync(encoded);
}, RangeError);

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
buffer.kMaxLength = 64;
const zlib = require('zlib');
buffer.kMaxLength = oldkMaxLength;

const encoded = Buffer.from('H4sIAAAAAAAAA0tMHFgAAIw2K/GAAAAA', 'base64');

// Async
zlib.gunzip(encoded, function(err) {
  assert.ok(err instanceof RangeError);
});

// Sync
assert.throws(function() {
  zlib.gunzipSync(encoded);
}, RangeError);

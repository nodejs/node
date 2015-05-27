'use strict';

const assert = require('assert');

// Change kMaxLength for zlib to trigger the error
// without having to allocate 1GB of buffers
const smalloc = process.binding('smalloc');
smalloc.kMaxLength = 128;
const zlib = require('zlib');
smalloc.kMaxLength = 0x3fffffff;

const encoded = new Buffer('H4sIAAAAAAAAA0tMHFgAAIw2K/GAAAAA', 'base64');

// Async
zlib.gunzip(encoded, function(err) {
  assert.ok(err instanceof RangeError);
});

// Sync
assert.throws(function() {
  zlib.gunzipSync(encoded);
}, RangeError);

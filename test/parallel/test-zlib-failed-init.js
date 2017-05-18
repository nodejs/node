'use strict';

const common = require('../common');

const assert = require('assert');
const zlib = require('zlib');

// For raw deflate or gzip encoding, a request for a 256-byte window is
// rejected as invalid, since only zlib headers provide means of transmitting
// the window size to the decompressor.
// (http://zlib.net/manual.html#Advanced)
const deflate = zlib.createDeflateRaw({ windowBits: 8 });
deflate.on('error', common.mustCall((error) => {
  assert.ok(/Init error/.test(error));
}));

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

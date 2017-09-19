'use strict';

const common = require('../common');

const assert = require('assert');
const zlib = require('zlib');

if (process.config.variables.node_shared_zlib &&
    /^1\.2\.[0-8]$/.test(process.versions.zlib)) {
  common.skip("older versions of shared zlib don't throw on create");
}

// For raw deflate encoding, requests for 256-byte windows are rejected as
// invalid by zlib.
// (http://zlib.net/manual.html#Advanced)
assert.throws(() => {
  zlib.createDeflateRaw({ windowBits: 8 });
}, /^Error: Init error$/);

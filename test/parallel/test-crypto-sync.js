// test synchronous methods
// https://github.com/iojs/io.js/pull/632
var common = require('../common');
var assert = require('assert');

if (!common.hasCrypto) {
  console.log('1..0 # Skipped: missing crypto');
  process.exit();
}
var crypto = require('crypto');

var buf = crypto.randomBytesSync(10);
assert(Buffer.isBuffer(buf));
assert.equal(10, buf.length);

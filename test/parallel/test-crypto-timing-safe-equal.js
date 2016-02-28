'use strict';
const common = require('../common');
const assert = require('assert');

if (!common.hasCrypto) {
  console.log('1..0 # Skipped: missing crypto');
  return;
}
var crypto = require('crypto');

assert.ok(crypto.timingSafeEqual(new Buffer('alpha'), new Buffer('alpha')),
  'equal strings not equal');
assert.ok(!crypto.timingSafeEqual(new Buffer('alpha'), new Buffer('beta')),
  'inequal strings considered equal');

'use strict';
const common = require('../common');
const assert = require('assert');

if (!common.hasCrypto) {
  console.log('1..0 # Skipped: missing crypto');
  return;
}
const crypto = require('crypto');

assert.ok(crypto.timingSafeEqual(Buffer.from('alpha'), Buffer.from('alpha')),
          'equal strings not equal');
assert.ok(!crypto.timingSafeEqual(Buffer.from('alpha'), Buffer.from('beta')),
          'inequal strings considered equal');

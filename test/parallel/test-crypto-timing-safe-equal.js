'use strict';
var common = require('../common');
var assert = require('assert');

if (!common.hasCrypto) {
  console.log('1..0 # Skipped: missing crypto');
  return;
}
var crypto = require('crypto');

assert.ok(crypto.timingSafeEqual('alpha', 'alpha'), 'equal strings not equal');
assert.ok(!crypto.timingSafeEqual('alpha', 'beta'),
  'inequal strings considered equal');

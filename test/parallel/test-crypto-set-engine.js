'use strict';
const common = require('../common');

if (!common.hasCrypto) {
  common.skip('missing crypto');
  return;
}

const assert = require('assert');
const crypto = require('crypto');

assert.throws(() => {
  crypto.setEngine(0, 100);
}, /^TypeError: "id" argument should be a string$/);

assert.throws(() => {
  crypto.setEngine('id', '100');
}, /^TypeError: "flags" argument should be a number, if present$/);

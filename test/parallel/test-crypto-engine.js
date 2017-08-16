'use strict';
const common = require('../common');

if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const crypto = require('crypto');

assert.throws(function() {
  crypto.setEngine(true);
}, /^TypeError: "id" argument should be a string$/);

assert.throws(function() {
  crypto.setEngine('/path/to/engine', 'notANumber');
}, /^TypeError: "flags" argument should be a number, if present$/);

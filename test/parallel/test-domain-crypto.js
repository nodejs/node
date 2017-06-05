'use strict';

const common = require('../common');

let crypto;
try {
  crypto = require('crypto');
} catch (e) {
  common.skip('node compiled without OpenSSL.');
  return;
}

// Pollution of global is intentional as part of test.
common.globalCheck = false;
// See https://github.com/nodejs/node/commit/d1eff9ab
global.domain = require('domain');

// should not throw a 'TypeError: undefined is not a function' exception
crypto.randomBytes(8);
crypto.randomBytes(8, common.mustCall());
crypto.pseudoRandomBytes(8);
crypto.pseudoRandomBytes(8, common.mustCall());
crypto.pbkdf2('password', 'salt', 8, 8, 'sha1', common.mustCall());

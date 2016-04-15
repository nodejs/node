/* eslint-disable required-modules */
'use strict';

try {
  var crypto = require('crypto');
} catch (e) {
  console.log('1..0 # Skipped: node compiled without OpenSSL.');
  return;
}

// Pollution of global is intentional as part of test.
// See https://github.com/nodejs/node/commit/d1eff9ab
global.domain = require('domain');

// should not throw a 'TypeError: undefined is not a function' exception
crypto.randomBytes(8);
crypto.randomBytes(8, function() {});
crypto.pseudoRandomBytes(8);
crypto.pseudoRandomBytes(8, function() {});
crypto.pbkdf2('password', 'salt', 8, 8, function() {});

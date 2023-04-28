'use strict';
const common = require('../common');
if (!common.hasCrypto) { common.skip('missing crypto'); }
const assert = require('assert');
const { generateKeyPair } = require('crypto');

if (common.isWindows) {
  // Remove this conditional once the libuv change is in Node.js.
  common.skip('crashing due to https://github.com/libuv/libuv/pull/2983');
}

// Regression test for a race condition: process.exit() might lead to OpenSSL
// cleaning up state from the exit() call via calling its destructor, but
// running OpenSSL operations on another thread might lead to them attempting
// to initialize OpenSSL, leading to a crash.
// This test crashed consistently on x64 Linux on Node v14.9.0.

generateKeyPair('rsa', {
  modulusLength: 2048,
  privateKeyEncoding: {
    type: 'pkcs1',
    format: 'pem'
  }
}, (err/* , publicKey, privateKey */) => {
  assert.ifError(err);
});

setTimeout(() => process.exit(), common.platformTimeout(10));

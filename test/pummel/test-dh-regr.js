'use strict';
var common = require('../common');
var assert = require('assert');

if (!common.hasCrypto) {
  console.log('1..0 # Skipped: missing crypto');
  process.exit();
}
var crypto = require('crypto');

var p = crypto.createDiffieHellman(256).getPrime();

for (var i = 0; i < 2000; i++) {
  var a = crypto.createDiffieHellman(p),
      b = crypto.createDiffieHellman(p);

  a.generateKeys();
  b.generateKeys();

  assert.deepEqual(
    a.computeSecret(b.getPublicKey()),
    b.computeSecret(a.getPublicKey()),
    'secrets should be equal!'
  );
}

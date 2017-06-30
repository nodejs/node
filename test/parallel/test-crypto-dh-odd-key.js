'use strict';
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const crypto = require('crypto');

function test() {
  const odd = Buffer.alloc(39, 'A');

  const c = crypto.createDiffieHellman(32);
  c.setPrivateKey(odd);
  c.generateKeys();
}

// FIPS requires a length of at least 1024
if (!common.hasFipsCrypto) {
  assert.doesNotThrow(function() { test(); });
} else {
  assert.throws(function() { test(); }, /key size too small/);
}

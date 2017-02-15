'use strict';
const common = require('../common');
const assert = require('assert');

if (!common.hasCrypto) {
  common.skip('missing crypto');
  return;
}
const crypto = require('crypto');

const p = crypto.createDiffieHellman(1024).getPrime();

for (let i = 0; i < 2000; i++) {
  const a = crypto.createDiffieHellman(p);
  const b = crypto.createDiffieHellman(p);

  a.generateKeys();
  b.generateKeys();

  assert.deepStrictEqual(
    a.computeSecret(b.getPublicKey()),
    b.computeSecret(a.getPublicKey()),
    'secrets should be equal!'
  );
}

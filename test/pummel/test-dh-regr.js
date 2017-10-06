'use strict';
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const crypto = require('crypto');

const p = crypto.createDiffieHellman(1024).getPrime();

for (let i = 0; i < 2000; i++) {
  const a = crypto.createDiffieHellman(p);
  const b = crypto.createDiffieHellman(p);

  a.generateKeys();
  b.generateKeys();

  const aSecret = a.computeSecret(b.getPublicKey());
  const bSecret = b.computeSecret(a.getPublicKey());

  assert.deepStrictEqual(
    aSecret,
    bSecret,
    'Secrets should be equal.\n' +
    `aSecret: ${aSecret.toString('base64')}\n` +
    `bSecret: ${bSecret.toString('base64')}`
  );
}

'use strict';
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const crypto = require('crypto');
const { modp2buf } = require('../common/crypto');
const modp2 = crypto.createDiffieHellmanGroup('modp2');

{
  // Ensure specific generator (buffer) works as expected.
  const exmodp2 = crypto.createDiffieHellman(modp2buf, Buffer.from([2]));
  modp2.generateKeys();
  exmodp2.generateKeys();
  const modp2Secret = modp2.computeSecret(exmodp2.getPublicKey())
      .toString('hex');
  const exmodp2Secret = exmodp2.computeSecret(modp2.getPublicKey())
      .toString('hex');
  assert.strictEqual(modp2Secret, exmodp2Secret);
}

{
  // Ensure specific generator (string without encoding) works as expected.
  const exmodp2 = crypto.createDiffieHellman(modp2buf, '\x02');
  exmodp2.generateKeys();
  const modp2Secret = modp2.computeSecret(exmodp2.getPublicKey())
      .toString('hex');
  const exmodp2Secret = exmodp2.computeSecret(modp2.getPublicKey())
      .toString('hex');
  assert.strictEqual(modp2Secret, exmodp2Secret);
}

{
  // Ensure specific generator (numeric) works as expected.
  const exmodp2 = crypto.createDiffieHellman(modp2buf, 2);
  exmodp2.generateKeys();
  const modp2Secret = modp2.computeSecret(exmodp2.getPublicKey())
      .toString('hex');
  const exmodp2Secret = exmodp2.computeSecret(modp2.getPublicKey())
      .toString('hex');
  assert.strictEqual(modp2Secret, exmodp2Secret);
}

'use strict';
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const crypto = require('crypto');
const { modp2buf } = require('../common/crypto');

const modp2 = crypto.createDiffieHellmanGroup('modp2');

const views = common.getArrayBufferViews(modp2buf);
for (const buf of [modp2buf, ...views]) {
  // Ensure specific generator (string with encoding) works as expected with
  // any ArrayBufferViews as the first argument to createDiffieHellman().
  const exmodp2 = crypto.createDiffieHellman(buf, '02', 'hex');
  modp2.generateKeys();
  exmodp2.generateKeys();
  const modp2Secret = modp2.computeSecret(exmodp2.getPublicKey())
      .toString('hex');
  const exmodp2Secret = exmodp2.computeSecret(modp2.getPublicKey())
      .toString('hex');
  assert.strictEqual(modp2Secret, exmodp2Secret);
}

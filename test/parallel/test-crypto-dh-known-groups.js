'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('node:assert');
const crypto = require('node:crypto');
const { hasFIPS, isBoringSSL } = require('../common/crypto');

if (isBoringSSL)
  common.skip('OpenSSL named-group validation');

const groups = ['modp14', 'modp15', 'modp16', 'modp17', 'modp18'];
if (!hasFIPS(3))
  groups.unshift('modp5');

const privateKey = Buffer.from('01020304', 'hex');
const generator = Buffer.from([2]);

for (const name of groups) {
  const peer = crypto.getDiffieHellman(name);
  const prime = peer.getPrime();
  const peerPublicKey = peer.generateKeys();

  // SSH encodes positive integers with a leading zero when necessary.
  for (const [p, g] of [
    [prime, 2],
    [Buffer.concat([Buffer.from([0]), prime]), Buffer.from([0, 2])],
  ]) {
    const dh = crypto.createDiffieHellman(p, g);
    assert.strictEqual(dh.verifyError, 0);
    assert.deepStrictEqual(dh.getPrime(), prime);
    assert.deepStrictEqual(dh.getGenerator(), generator);

    dh.setPrivateKey(privateKey);
    const publicKey = dh.generateKeys();
    assert.deepStrictEqual(dh.generateKeys(), publicKey);
    assert.deepStrictEqual(dh.getPrivateKey(), privateKey);
    assert.deepStrictEqual(dh.computeSecret(peerPublicKey),
                           peer.computeSecret(publicKey));
  }
}

// A known modulus alone must not bypass validation of different parameters.
{
  const prime = crypto.getDiffieHellman('modp14').getPrime();
  const notPrime = Buffer.from(prime);
  notPrime[notPrime.length - 1] &= 0xfe;
  assert.strictEqual(
    crypto.createDiffieHellman(notPrime, 2).verifyError &
      crypto.constants.DH_CHECK_P_NOT_PRIME,
    crypto.constants.DH_CHECK_P_NOT_PRIME);

  const notGenerator = Buffer.from(prime);
  notGenerator[notGenerator.length - 1]--;
  assert.strictEqual(
    crypto.createDiffieHellman(prime, notGenerator).verifyError &
      crypto.constants.DH_NOT_SUITABLE_GENERATOR,
    crypto.constants.DH_NOT_SUITABLE_GENERATOR);
}

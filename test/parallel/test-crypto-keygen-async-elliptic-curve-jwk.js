'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const {
  generateKeyPair,
} = require('crypto');

// Test async elliptic curve key generation with 'jwk' encoding.
{
  [
    'ed25519',
    'ed448',
    'x25519',
    'x448',
  ].forEach((type) => {
    generateKeyPair(type, {
      publicKeyEncoding: {
        format: 'jwk'
      },
      privateKeyEncoding: {
        format: 'jwk'
      }
    }, common.mustSucceed((publicKey, privateKey) => {
      assert.strictEqual(typeof publicKey, 'object');
      assert.strictEqual(typeof privateKey, 'object');
      assert.strictEqual(publicKey.x, privateKey.x);
      assert(!publicKey.d);
      assert(privateKey.d);
      assert.strictEqual(publicKey.kty, 'OKP');
      assert.strictEqual(publicKey.kty, privateKey.kty);
      const expectedCrv = `${type.charAt(0).toUpperCase()}${type.slice(1)}`;
      assert.strictEqual(publicKey.crv, expectedCrv);
      assert.strictEqual(publicKey.crv, privateKey.crv);
    }));
  });
}

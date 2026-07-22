'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const {
  generateKeyPair,
} = require('crypto');

// Test async elliptic curve key generation with 'jwk' encoding and named
// curve.
['P-384', 'P-256', 'P-521', 'secp256k1'].forEach((curve) => {
  generateKeyPair('ec', {
    namedCurve: curve,
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
    assert.strictEqual(publicKey.y, privateKey.y);
    assert(!publicKey.d);
    assert(privateKey.d);
    assert.strictEqual(publicKey.kty, 'EC');
    assert.strictEqual(publicKey.kty, privateKey.kty);
    assert.strictEqual(publicKey.crv, curve);
    assert.strictEqual(publicKey.crv, privateKey.crv);
  }));
});

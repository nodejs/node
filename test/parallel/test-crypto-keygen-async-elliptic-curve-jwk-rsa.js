'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const {
  generateKeyPair,
} = require('crypto');

// Test async elliptic curve key generation with 'jwk' encoding and RSA.
{
  generateKeyPair('rsa', {
    modulusLength: 1024,
    publicKeyEncoding: {
      format: 'jwk'
    },
    privateKeyEncoding: {
      format: 'jwk'
    }
  }, common.mustSucceed((publicKey, privateKey) => {
    assert.strictEqual(typeof publicKey, 'object');
    assert.strictEqual(typeof privateKey, 'object');
    assert.strictEqual(publicKey.kty, 'RSA');
    assert.strictEqual(publicKey.kty, privateKey.kty);
    assert.strictEqual(typeof publicKey.n, 'string');
    assert.strictEqual(publicKey.n, privateKey.n);
    assert.strictEqual(typeof publicKey.e, 'string');
    assert.strictEqual(publicKey.e, privateKey.e);
    assert.strictEqual(typeof privateKey.d, 'string');
    assert.strictEqual(typeof privateKey.p, 'string');
    assert.strictEqual(typeof privateKey.q, 'string');
    assert.strictEqual(typeof privateKey.dp, 'string');
    assert.strictEqual(typeof privateKey.dq, 'string');
    assert.strictEqual(typeof privateKey.qi, 'string');
  }));
}

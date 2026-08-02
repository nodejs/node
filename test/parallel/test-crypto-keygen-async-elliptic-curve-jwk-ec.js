'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const {
  generateKeyPair,
} = require('crypto');
const { hasFIPS } = require('../common/crypto');

// Test async elliptic curve key generation with 'jwk' encoding and named
// curve.
for (const curve of ['P-384', 'P-256', 'P-521', 'secp256k1']) {
  if (process.features.openssl_is_boringssl && curve === 'secp256k1') {
    common.printSkipMessage(`Skipping unsupported ${curve} test case`);
    continue;
  }
  generateKeyPair('ec', {
    namedCurve: curve,
    publicKeyEncoding: {
      format: 'jwk'
    },
    privateKeyEncoding: {
      format: 'jwk'
    }
  }, common.mustCall((err, publicKey, privateKey) => {
    if (hasFIPS(3) && curve === 'secp256k1') {
      assert.strictEqual(err?.code, 'ERR_OSSL_EC_UNKNOWN_GROUP');
      return;
    }
    assert.ifError(err);
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
};

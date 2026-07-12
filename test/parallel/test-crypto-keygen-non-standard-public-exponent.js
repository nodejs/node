'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const {
  generateKeyPairSync,
} = require('crypto');
const { hasFIPS } = require('../common/crypto');

// Test sync key generation with key objects with a non-standard
// publicExponent
{
  if (hasFIPS(3)) {
    assert.throws(() => generateKeyPairSync('rsa', {
      publicExponent: 3,
      modulusLength: 2048,
    }), {
      code: 'ERR_OSSL_RSA_PUB_EXPONENT_OUT_OF_RANGE',
    });
  } else {
    const { publicKey, privateKey } = generateKeyPairSync('rsa', {
      publicExponent: 3,
      modulusLength: 512
    });

    assert.strictEqual(typeof publicKey, 'object');
    assert.strictEqual(publicKey.type, 'public');
    assert.strictEqual(publicKey.asymmetricKeyType, 'rsa');
    assert.deepStrictEqual(publicKey.asymmetricKeyDetails, {
      modulusLength: 512,
      publicExponent: 3n
    });

    assert.strictEqual(typeof privateKey, 'object');
    assert.strictEqual(privateKey.type, 'private');
    assert.strictEqual(privateKey.asymmetricKeyType, 'rsa');
    assert.deepStrictEqual(privateKey.asymmetricKeyDetails, {
      modulusLength: 512,
      publicExponent: 3n
    });
  }
}

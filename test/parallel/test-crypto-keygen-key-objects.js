'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const {
  generateKeyPairSync,
} = require('crypto');
const { hasFIPS } = require('../common/crypto');

// Test sync key generation with key objects.
{
  const modulusLength = hasFIPS(3) ? 2048 : 512;
  const { publicKey, privateKey } = generateKeyPairSync('rsa', {
    modulusLength
  });

  assert.strictEqual(typeof publicKey, 'object');
  assert.strictEqual(publicKey.type, 'public');
  assert.strictEqual(publicKey.asymmetricKeyType, 'rsa');
  assert.deepStrictEqual(publicKey.asymmetricKeyDetails, {
    modulusLength,
    publicExponent: 65537n
  });

  assert.strictEqual(typeof privateKey, 'object');
  assert.strictEqual(privateKey.type, 'private');
  assert.strictEqual(privateKey.asymmetricKeyType, 'rsa');
  assert.deepStrictEqual(privateKey.asymmetricKeyDetails, {
    modulusLength,
    publicExponent: 65537n
  });
}

'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const {
  generateKeyPair,
} = require('crypto');
const {
  assertApproximateSize,
  hasFIPS,
  testEncryptDecrypt,
  testSignVerify,
  pkcs1PubExp,
  pkcs1PrivExp,
} = require('../common/crypto');
const { promisify } = require('util');

// Test the util.promisified API with async RSA key generation.
{
  const isFips = hasFIPS(3);
  promisify(generateKeyPair)('rsa', {
    publicExponent: 0x10001,
    modulusLength: isFips ? 2048 : 512,
    publicKeyEncoding: {
      type: 'pkcs1',
      format: 'pem'
    },
    privateKeyEncoding: {
      type: 'pkcs1',
      format: 'pem'
    }
  }).then(common.mustCall((keys) => {
    const { publicKey, privateKey } = keys;
    assert.strictEqual(typeof publicKey, 'string');
    assert.match(publicKey, pkcs1PubExp);
    assertApproximateSize(publicKey, isFips ? 426 : 180);

    assert.strictEqual(typeof privateKey, 'string');
    assert.match(privateKey, pkcs1PrivExp);
    assertApproximateSize(privateKey, isFips ? 1675 : 512);

    testEncryptDecrypt(publicKey, privateKey);
    testSignVerify(publicKey, privateKey);
  }));
}

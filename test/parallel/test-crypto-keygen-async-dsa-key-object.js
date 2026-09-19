'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const { isBoringSSL } = require('../common/crypto');

if (isBoringSSL)
  common.skip('not supported by BoringSSL');

const assert = require('assert');
const {
  generateKeyPair,
} = require('crypto');

// Test async DSA key object generation.
{
  generateKeyPair('dsa', {
    modulusLength: 2048,
    divisorLength: 256
  }, common.mustSucceed((publicKey, privateKey) => {
    assert.strictEqual(publicKey.type, 'public');
    assert.strictEqual(publicKey.asymmetricKeyType, 'dsa');
    assert.deepStrictEqual(publicKey.asymmetricKeyDetails, {
      modulusLength: 2048,
      divisorLength: 256
    });

    assert.strictEqual(privateKey.type, 'private');
    assert.strictEqual(privateKey.asymmetricKeyType, 'dsa');
    assert.deepStrictEqual(privateKey.asymmetricKeyDetails, {
      modulusLength: 2048,
      divisorLength: 256
    });
  }));
}

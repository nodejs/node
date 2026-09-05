'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const { isBoringSSL, hasOpenSSL } = require('../common/crypto');

if (isBoringSSL)
  common.skip('not supported by BoringSSL');

const assert = require('assert');
const {
  generateKeyPair,
} = require('crypto');

// Test async DSA key object generation.
{
  generateKeyPair('dsa', {
    modulusLength: hasOpenSSL(3) ? 2048 : 512,
    divisorLength: 256
  }, common.mustSucceed((publicKey, privateKey) => {
    assert.strictEqual(publicKey.type, 'public');
    assert.strictEqual(publicKey.asymmetricKeyType, 'dsa');
    assert.deepStrictEqual(publicKey.asymmetricKeyDetails, {
      modulusLength: hasOpenSSL(3) ? 2048 : 512,
      divisorLength: 256
    });

    assert.strictEqual(privateKey.type, 'private');
    assert.strictEqual(privateKey.asymmetricKeyType, 'dsa');
    assert.deepStrictEqual(privateKey.asymmetricKeyDetails, {
      modulusLength: hasOpenSSL(3) ? 2048 : 512,
      divisorLength: 256
    });
  }));
}

'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const {
  generateKeyPairSync,
} = require('crypto');
const {
  assertApproximateSize,
  testEncryptDecrypt,
  testSignVerify,
  pkcs1PubExp,
  pkcs8Exp,
} = require('../common/crypto');

// To make the test faster, we will only test sync key generation once and
// with a relatively small key.
{
  const ret = generateKeyPairSync('rsa', {
    publicExponent: 3,
    modulusLength: 512,
    publicKeyEncoding: {
      type: 'pkcs1',
      format: 'pem'
    },
    privateKeyEncoding: {
      type: 'pkcs8',
      format: 'pem'
    }
  });

  assert.strictEqual(Object.keys(ret).length, 2);
  const { publicKey, privateKey } = ret;

  assert.strictEqual(typeof publicKey, 'string');
  assert.match(publicKey, pkcs1PubExp);
  assertApproximateSize(publicKey, 162);
  assert.strictEqual(typeof privateKey, 'string');
  assert.match(privateKey, pkcs8Exp);
  assertApproximateSize(privateKey, 512);

  testEncryptDecrypt(publicKey, privateKey);
  testSignVerify(publicKey, privateKey);
}

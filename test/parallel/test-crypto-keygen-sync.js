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
  hasFIPS,
  testEncryptDecrypt,
  testSignVerify,
  pkcs1PubExp,
  pkcs8Exp,
} = require('../common/crypto');

// To make the test faster, we will only test sync key generation once and
// with a relatively small key.
{
  const isFips = hasFIPS(3);
  const ret = generateKeyPairSync('rsa', {
    publicExponent: isFips ? 0x10001 : 3,
    modulusLength: isFips ? 2048 : 512,
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
  assertApproximateSize(publicKey, isFips ? 426 : 162);
  assert.strictEqual(typeof privateKey, 'string');
  assert.match(privateKey, pkcs8Exp);
  assertApproximateSize(privateKey, isFips ? 1704 : 512);

  testEncryptDecrypt(publicKey, privateKey);
  testSignVerify(publicKey, privateKey);
}

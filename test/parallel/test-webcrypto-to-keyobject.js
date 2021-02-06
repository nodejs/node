'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const { Buffer } = require('buffer');
const {
  createSecretKey,
  createPublicKey,
  createPrivateKey,
  webcrypto: { subtle }
} = require('crypto');

(async function() {
  const { publicKey, privateKey } = await subtle.generateKey({
    name: 'RSA-OAEP',
    modulusLength: 1024,
    publicExponent: Buffer.from([1, 0, 1]),
    hash: 'SHA-256'
  }, false, ['encrypt', 'decrypt']);

  const nodePublicKey = createPublicKey(publicKey);
  assert.strictEqual(nodePublicKey.type, 'public');
  assert.strictEqual(nodePublicKey.asymmetricKeyType, 'rsa');
  assert.strictEqual(nodePublicKey.asymmetricKeyDetails.modulusLength, 1024);

  const nodePublicKeyFromPrivate = createPublicKey(privateKey);
  assert.strictEqual(nodePublicKeyFromPrivate.type, 'public');
  assert.strictEqual(nodePublicKeyFromPrivate.asymmetricKeyType, 'rsa');
  assert.strictEqual(
    nodePublicKeyFromPrivate.asymmetricKeyDetails.modulusLength, 1024);

  const nodePrivateKey = createPrivateKey(privateKey);
  assert.strictEqual(nodePrivateKey.type, 'private');
  assert.strictEqual(nodePrivateKey.asymmetricKeyType, 'rsa');
  assert.strictEqual(nodePrivateKey.asymmetricKeyDetails.modulusLength, 1024);

  const aesKey = await subtle.generateKey({
    name: 'AES-GCM',
    length: 256
  }, false, ['encrypt', 'decrypt']);

  const nodeSecretKey = createSecretKey(aesKey);
  assert.strictEqual(nodeSecretKey.type, 'secret');
  assert.strictEqual(nodeSecretKey.symmetricKeySize, 32);
})().then(common.mustCall());

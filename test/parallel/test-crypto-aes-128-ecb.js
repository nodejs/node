'use strict';
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const crypto = require('crypto');

const { privateKey, publicKey } = crypto.generateKeyPairSync('rsa', {
  modulusLength: 2048,
  publicKeyEncoding: {
    type: 'spki',
    format: 'pem'
  },
  privateKeyEncoding: {
    type: 'pkcs8',
    format: 'pem',
    cipher: 'aes-128-ecb',
    passphrase: 'abcdef'
  }
});
assert.notStrictEqual(privateKey.toString(), '');

const msg = 'The quick brown fox jumps over the lazy dog';

const encryptedString = crypto.privateEncrypt({
  key: privateKey,
  passphrase: 'abcdef'
}, Buffer.from(msg)).toString('base64');
const decryptedString = crypto.publicDecrypt(publicKey, Buffer.from(encryptedString, 'base64')).toString();
console.log(`Encrypted: ${encryptedString}`);
console.log(`Decrypted: ${decryptedString}`);

assert.notStrictEqual(encryptedString, '');
assert.strictEqual(decryptedString, msg);

'use strict';

const common = require('../common');
const crypto = require('crypto');

if (!common.hasCrypto) {
  common.skip('missing crypto');
}

const keys = crypto.generateKeyPairSync('rsa', {
  modulusLength: 2048,
  publicKeyEncoding: { type: 'spki', format: 'pem' },
  privateKeyEncoding: { type: 'pkcs8', format: 'pem' }
});

const empty = '';

const ciphertext = crypto.publicEncrypt({
  oaepHash: 'sha1',
  padding: crypto.constants.RSA_PKCS1_OAEP_PADDING,
  key: keys.publicKey
}, Buffer.from(empty)).toString('base64');

const plaintext = crypto.privateDecrypt({
  oaepHash: 'sha1',
  padding: crypto.constants.RSA_PKCS1_OAEP_PADDING,
  key: keys.privateKey
}, Buffer.from(ciphertext, 'base64')).toString('utf8').trim();

console.log('Decrypt', plaintext);
console.assert(empty === plaintext, 'rsa-oaep `encrypt` empty string is success, but `decrypt` got unexpected string.');

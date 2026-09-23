'use strict';

const common = require('../common.js');
const fixtures = require('../../test/common/fixtures.js');
const { createPrivateKey, createPublicKey, subtle } = require('node:crypto');

const bench = common.createBenchmark(main, {
  keyType: ['hmac', 'aes-gcm', 'ecdsa-private', 'rsa-private', 'rsa-public'],
  n: [1e5],
});

async function createKey(keyType) {
  switch (keyType) {
    case 'hmac':
      return subtle.importKey(
        'raw', new Uint8Array(32), { name: 'HMAC', hash: 'SHA-256' },
        true, ['sign', 'verify']);
    case 'aes-gcm':
      return subtle.importKey(
        'raw', new Uint8Array(32), 'AES-GCM',
        true, ['encrypt', 'decrypt', 'wrapKey', 'unwrapKey']);
    case 'ecdsa-private':
      return createPrivateKey(fixtures.readKey('ec_p256_private.pem'))
        .toCryptoKey({ name: 'ECDSA', namedCurve: 'P-256' }, true, ['sign']);
    case 'rsa-private':
      return createPrivateKey(fixtures.readKey('rsa_private_2048.pem'))
        .toCryptoKey({ name: 'RSA-PSS', hash: 'SHA-256' }, true, ['sign']);
    case 'rsa-public':
      return createPublicKey(fixtures.readKey('rsa_private_2048.pem'))
        .toCryptoKey({ name: 'RSA-PSS', hash: 'SHA-256' }, true, ['verify']);
    default:
      throw new Error(`Unsupported key type: ${keyType}`);
  }
}

async function main({ n, keyType }) {
  const key = await createKey(keyType);
  let result;

  bench.start();
  for (let i = 0; i < n; i++)
    result = await subtle.exportKey('jwk', key);
  bench.end(n);

  if (result.kty === undefined)
    throw new Error('Missing key type');
}

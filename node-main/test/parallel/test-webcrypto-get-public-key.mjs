import * as common from '../common/index.mjs';

if (!common.hasCrypto) common.skip('missing crypto');

import * as assert from 'node:assert';
const { subtle } = globalThis.crypto;

const RSA_KEY_GEN = {
  modulusLength: 2048,
  publicExponent: new Uint8Array([1, 0, 1]),
  hash: 'SHA-256',
};

const publicUsages = {
  'ECDH': [],
  'ECDSA': ['verify'],
  'Ed25519': ['verify'],
  'RSA-OAEP': ['encrypt', 'wrapKey'],
  'RSA-PSS': ['verify'],
  'RSASSA-PKCS1-v1_5': ['verify'],
  'X25519': [],
};

for await (const { privateKey } of [
  subtle.generateKey({ name: 'ECDH', namedCurve: 'P-256' }, false, ['deriveBits']),
  subtle.generateKey({ name: 'ECDSA', namedCurve: 'P-256' }, false, ['sign']),
  subtle.generateKey('Ed25519', false, ['sign']),
  subtle.generateKey({ name: 'RSA-OAEP', ...RSA_KEY_GEN }, false, ['decrypt', 'unwrapKey']),
  subtle.generateKey({ name: 'RSA-PSS', ...RSA_KEY_GEN }, false, ['sign']),
  subtle.generateKey({ name: 'RSASSA-PKCS1-v1_5', ...RSA_KEY_GEN }, false, ['sign']),
  subtle.generateKey('X25519', false, ['deriveBits']),
]) {
  const { name } = privateKey.algorithm;
  const usages = publicUsages[name];
  const publicKey = await subtle.getPublicKey(privateKey, usages);
  assert.deepStrictEqual(publicKey.algorithm, privateKey.algorithm);
  assert.strictEqual(publicKey.type, 'public');
  assert.strictEqual(publicKey.extractable, true);

  await assert.rejects(() => subtle.getPublicKey(privateKey, ['deriveBits']), {
    name: 'SyntaxError',
    message: /Unsupported key usage/
  });

  await assert.rejects(() => subtle.getPublicKey(publicKey, publicKey.usages), {
    name: 'InvalidAccessError',
    message: 'key must be a private key'
  });
}

const secretKey = await subtle.generateKey(
  { name: 'AES-CBC', length: 128 }, true, ['encrypt', 'decrypt']);
await assert.rejects(() => subtle.getPublicKey(secretKey, ['encrypt', 'decrypt']), {
  name: 'NotSupportedError',
  message: 'key must be a private key'
});

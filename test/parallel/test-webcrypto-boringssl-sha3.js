'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const { isBoringSSL } = require('../common/crypto');
if (!isBoringSSL)
  common.skip('BoringSSL-only test');

const assert = require('assert');
const { subtle } = require('crypto');
const { SubtleCrypto } = globalThis;

const unsupportedHash = {
  name: 'NotSupportedError',
  message: 'SHA-3 is only supported for digest operations',
};

async function test() {
  const data = Buffer.from('abc');
  const secret = Buffer.alloc(32);
  const wrappingKey = await subtle.importKey(
    'raw', Buffer.alloc(16), 'AES-KW', false, ['wrapKey', 'unwrapKey']);
  const keyToWrap = await subtle.importKey(
    'raw', secret, { name: 'HMAC', hash: 'SHA-256' }, true, ['sign']);
  const wrapped = await subtle.wrapKey('raw', keyToWrap, wrappingKey, 'AES-KW');
  const bases = await Promise.all(['HKDF', 'PBKDF2'].map((name) =>
    subtle.importKey('raw', secret, name, false, ['deriveBits', 'deriveKey'])));

  for (const [hash, length] of [
    ['SHA3-256', 32],
    ['SHA3-384', 48],
    ['SHA3-512', 64],
  ]) {
    assert.strictEqual(SubtleCrypto.supports('digest', hash), true);
    assert.strictEqual((await subtle.digest(hash, data)).byteLength, length);

    for (const name of ['RSASSA-PKCS1-v1_5', 'RSA-PSS', 'RSA-OAEP']) {
      const algorithm = {
        name, hash, modulusLength: 2048,
        publicExponent: new Uint8Array([1, 0, 1]),
      };
      const usages = name === 'RSA-OAEP' ? ['encrypt', 'decrypt'] : ['sign', 'verify'];
      assert.strictEqual(SubtleCrypto.supports('generateKey', algorithm), false);
      assert.strictEqual(SubtleCrypto.supports('importKey', algorithm), false);
      await assert.rejects(subtle.generateKey(algorithm, true, usages), unsupportedHash);
    }

    const hmac = { name: 'HMAC', hash, length: 256 };
    assert.strictEqual(SubtleCrypto.supports('generateKey', hmac), false);
    assert.strictEqual(SubtleCrypto.supports('importKey', hmac), false);
    await assert.rejects(subtle.generateKey(hmac, true, ['sign']), unsupportedHash);
    await assert.rejects(subtle.importKey('raw', secret, hmac, false, ['sign']),
                         unsupportedHash);
    assert.strictEqual(SubtleCrypto.supports('unwrapKey', 'AES-KW', hmac), false);
    await assert.rejects(subtle.unwrapKey(
      'raw', wrapped, wrappingKey, 'AES-KW', hmac, false, ['sign']), unsupportedHash);

    for (const [index, algorithm] of [
      { name: 'HKDF', hash, salt: secret, info: data },
      { name: 'PBKDF2', hash, salt: secret, iterations: 1 },
    ].entries()) {
      const base = bases[index];
      const derivedKey = { name: 'AES-GCM', length: 128 };
      assert.strictEqual(SubtleCrypto.supports('deriveBits', algorithm, 128), false);
      assert.strictEqual(SubtleCrypto.supports('deriveKey', algorithm, derivedKey), false);
      await assert.rejects(subtle.deriveBits(algorithm, base, 128), unsupportedHash);
      await assert.rejects(subtle.deriveKey(
        algorithm, base, derivedKey, false, ['encrypt']), unsupportedHash);

      const sha256 = { ...algorithm, hash: 'SHA-256' };
      assert.strictEqual(SubtleCrypto.supports('deriveKey', sha256, hmac), false);
      await assert.rejects(subtle.deriveKey(
        sha256, base, hmac, false, ['sign']), unsupportedHash);
    }
  }
}

test().then(common.mustCall());

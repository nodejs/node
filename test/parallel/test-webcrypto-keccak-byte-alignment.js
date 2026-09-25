'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const { createSecretKey, subtle } = require('crypto');
const { SubtleCrypto } = globalThis;
const unsupported = { name: 'NotSupportedError' };

(async () => {
  const data = new Uint8Array([1, 2, 3]);
  for (const name of ['cSHAKE128', 'cSHAKE256']) {
    if (!SubtleCrypto.supports('digest', { name, outputLength: 128 }))
      continue;

    for (const customization of [undefined, new Uint8Array([1])]) {
      if (!SubtleCrypto.supports('digest', { name, outputLength: 128, customization }))
        continue;

      for (let remainder = 1; remainder < 8; remainder++) {
        const algorithm = { name, outputLength: 128 + remainder, customization };
        assert.strictEqual(SubtleCrypto.supports('digest', algorithm), false);
        await assert.rejects(subtle.digest(algorithm, data), unsupported);
      }

      const digest = await subtle.digest({ name, outputLength: 128, customization }, data);
      assert.strictEqual(digest.byteLength, 16);
    }

    // Check the uint32 boundary without allocating a large digest.
    assert.strictEqual(SubtleCrypto.supports('digest', { name, outputLength: 0xfffffff8 }), true);
    for (const outputLength of [0xfffffff9, 0xffffffff]) {
      assert.strictEqual(SubtleCrypto.supports('digest', { name, outputLength }), false);
      await assert.rejects(subtle.digest({ name, outputLength }, data), unsupported);
    }
  }

  const baseKey = await subtle.importKey('raw-secret', data, 'HKDF', false, ['deriveKey']);
  const derivation = { name: 'HKDF', hash: 'SHA-256', salt: data, info: data };
  const raw = Buffer.alloc(32, 1);
  const secretKey = createSecretKey(raw);

  for (const name of ['KMAC128', 'KMAC256']) {
    if (!SubtleCrypto.supports('importKey', name))
      continue;

    const key = await subtle.importKey('raw-secret', raw, name, false, ['sign', 'verify']);
    for (let remainder = 1; remainder < 8; remainder++) {
      const algorithm = { name, length: 248 + remainder };
      assert.strictEqual(SubtleCrypto.supports('generateKey', algorithm), false);
      assert.strictEqual(SubtleCrypto.supports('importKey', algorithm), false);
      assert.strictEqual(SubtleCrypto.supports('deriveKey', derivation, algorithm), false);
      await assert.rejects(subtle.generateKey(algorithm, false, ['sign']), unsupported);
      await assert.rejects(subtle.importKey('raw-secret', raw, algorithm, false, ['sign']), unsupported);
      await assert.rejects(subtle.importKey(
        'jwk', { kty: 'oct', k: raw.toString('base64url') }, algorithm, false, ['sign']), unsupported);
      await assert.rejects(subtle.deriveKey(derivation, baseKey, algorithm, false, ['sign']), unsupported);
      assert.throws(() => secretKey.toCryptoKey(algorithm, false, ['sign']), unsupported);

      const params = { name, outputLength: 248 + remainder };
      assert.strictEqual(SubtleCrypto.supports('sign', params), false);
      assert.strictEqual(SubtleCrypto.supports('verify', params), false);
      await assert.rejects(subtle.sign(params, key, data), unsupported);
      await assert.rejects(subtle.verify(params, key, raw, data), unsupported);
    }
  }

  if (SubtleCrypto.supports('generateKey', 'ML-KEM-768')) {
    const { publicKey, privateKey } = await subtle.generateKey(
      'ML-KEM-768', false, ['encapsulateBits', 'encapsulateKey', 'decapsulateKey']);
    const { ciphertext } = await subtle.encapsulateBits('ML-KEM-768', publicKey);
    for (const name of ['KMAC128', 'KMAC256']) {
      if (!SubtleCrypto.supports('importKey', name))
        continue;

      const algorithm = { name, length: 255 };
      assert.strictEqual(SubtleCrypto.supports('encapsulateKey', 'ML-KEM-768', algorithm), false);
      assert.strictEqual(SubtleCrypto.supports('decapsulateKey', 'ML-KEM-768', algorithm), false);
      await assert.rejects(subtle.encapsulateKey(
        'ML-KEM-768', publicKey, algorithm, false, ['sign']), unsupported);
      await assert.rejects(subtle.decapsulateKey(
        'ML-KEM-768', privateKey, ciphertext, algorithm, false, ['sign']), unsupported);

      assert.strictEqual(SubtleCrypto.supports('encapsulateKey', 'ML-KEM-768', { name, length: 256 }), true);
      assert.strictEqual(SubtleCrypto.supports('decapsulateKey', 'ML-KEM-768', { name, length: 256 }), true);
    }
  }
})().then(common.mustCall());

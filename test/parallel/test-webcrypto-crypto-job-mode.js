// Flags: --expose-internals
'use strict';

const common = require('../common');

if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const { hasOpenSSL } = require('../common/crypto');
const { types: { isCryptoKey } } = require('util');
const { internalBinding } = require('internal/test/binding');
const {
  getCryptoKeyHandle,
} = require('internal/crypto/keys');
const {
  getUsagesMask,
} = require('internal/crypto/util');
const {
  aesCipher,
} = require('internal/crypto/aes');

const {
  AESCipherJob,
  EcKeyPairGenJob,
  HashJob,
  SecretKeyGenJob,
  kCryptoJobWebCrypto,
  kKeyVariantAES_CBC_128,
  kWebCryptoCipherEncrypt,
} = internalBinding('crypto');

const { subtle } = globalThis.crypto;

// Defines Object.prototype setters that fail the test if native result objects
// carrying key or shared secret material use [[Set]].
async function withObjectPrototypeSetters(names, fn) {
  const descriptors = new Map();
  for (const name of names) {
    descriptors.set(name, Object.getOwnPropertyDescriptor(Object.prototype, name));
    Object.defineProperty(Object.prototype, name, {
      __proto__: null,
      configurable: true,
      get: common.mustNotCall(`Object.prototype.${name} getter`),
      set: common.mustNotCall(`Object.prototype.${name} setter`),
    });
  }

  try {
    return await fn();
  } finally {
    for (const name of names) {
      const descriptor = descriptors.get(name);
      if (descriptor === undefined) {
        delete Object.prototype[name];
      } else {
        Object.defineProperty(Object.prototype, name, descriptor);
      }
    }
  }
}

(async function() {
  {
    const promise = new HashJob(
      kCryptoJobWebCrypto,
      'sha256',
      Buffer.from('hello'),
      undefined).run();

    assert.strictEqual(Object.getPrototypeOf(promise), Promise.prototype);

    let settled = false;
    promise.then(common.mustCall(() => { settled = true; }));
    await Promise.resolve();
    assert.strictEqual(settled, false);

    const digest = await promise;
    assert(digest instanceof ArrayBuffer);
    assert.strictEqual(digest.byteLength, 32);
    assert.strictEqual(Object.hasOwn(digest, 'then'), false);
  }

  {
    const key = await new SecretKeyGenJob(
      kCryptoJobWebCrypto,
      128,
      { name: 'AES-CBC', length: 128 },
      getUsagesMask(new Set(['encrypt'])),
      true).run();

    assert(isCryptoKey(key));
    assert(key instanceof CryptoKey);
    assert.strictEqual(key.type, 'secret');
    assert.strictEqual(key.extractable, true);
    assert.deepStrictEqual(key.usages, ['encrypt']);
  }

  {
    const pair = await withObjectPrototypeSetters(
      ['publicKey', 'privateKey'],
      () => new EcKeyPairGenJob(
        kCryptoJobWebCrypto,
        'P-256',
        undefined,
        { name: 'ECDSA', namedCurve: 'P-256' },
        getUsagesMask(new Set(['verify'])),
        getUsagesMask(new Set(['sign'])),
        true).run());

    assert.strictEqual(Object.getPrototypeOf(pair), Object.prototype);
    assert.strictEqual(Object.hasOwn(pair, 'then'), false);
    assert(isCryptoKey(pair.publicKey));
    assert(isCryptoKey(pair.privateKey));
    assert(pair.publicKey instanceof CryptoKey);
    assert(pair.privateKey instanceof CryptoKey);
    assert.strictEqual(pair.publicKey.type, 'public');
    assert.strictEqual(pair.privateKey.type, 'private');
    assert.deepStrictEqual(pair.publicKey.usages, ['verify']);
    assert.deepStrictEqual(pair.privateKey.usages, ['sign']);
  }

  {
    const key = await subtle.generateKey(
      { name: 'AES-CBC', length: 128 },
      false,
      ['encrypt']);
    assert.throws(
      () => new AESCipherJob(
        kCryptoJobWebCrypto,
        kWebCryptoCipherEncrypt,
        getCryptoKeyHandle(key),
        Buffer.alloc(16),
        kKeyVariantAES_CBC_128,
        Buffer.alloc(15)),
      /Invalid initialization vector/);

    const promise = aesCipher(
      kWebCryptoCipherEncrypt,
      key,
      Buffer.alloc(16),
      { name: 'AES-CBC', iv: Buffer.alloc(15) });

    assert.strictEqual(Object.getPrototypeOf(promise), Promise.prototype);
    await assert.rejects(promise, (err) => {
      assert.strictEqual(err.name, 'OperationError');
      assert.strictEqual(
        err.message,
        'The operation failed for an operation-specific reason');
      assert(err.cause);
      assert.match(err.cause.message, /Invalid initialization vector/);
      return true;
    });
  }

  {
    const key = await subtle.generateKey(
      { name: 'AES-CBC', length: 128 },
      false,
      ['encrypt', 'decrypt']);
    const iv = crypto.getRandomValues(new Uint8Array(16));
    const ciphertext = new Uint8Array(await subtle.encrypt(
      { name: 'AES-CBC', iv },
      key,
      Buffer.alloc(16)));
    ciphertext[0] ^= 0xff;

    await assert.rejects(
      subtle.decrypt({ name: 'AES-CBC', iv }, key, ciphertext),
      (err) => {
        assert.strictEqual(err.name, 'OperationError');
        assert.strictEqual(
          err.message,
          'The operation failed for an operation-specific reason');
        assert(err.cause);
        assert.strictEqual(typeof err.cause.message, 'string');
        assert.notStrictEqual(err.cause.message, '');
        return true;
      });
  }

  {
    const key = await subtle.generateKey(
      { name: 'HMAC', hash: 'SHA-256' },
      false,
      ['sign', 'verify']);
    const data = Buffer.from('hello');
    const signature = await subtle.sign('HMAC', key, data);
    assert(signature instanceof ArrayBuffer);
    assert.strictEqual(
      typeof await subtle.verify('HMAC', key, signature, data),
      'boolean');
  }

  {
    Object.defineProperty(CryptoKey.prototype, 'then', {
      __proto__: null,
      configurable: true,
      get: common.mustNotCall('CryptoKey.prototype.then getter'),
    });

    try {
      const key = await subtle.generateKey(
        { name: 'AES-CBC', length: 128 },
        true,
        ['encrypt']);
      assert(isCryptoKey(key));
      assert.strictEqual(Object.hasOwn(key, 'then'), false);
    } finally {
      delete CryptoKey.prototype.then;
    }
  }

  if (hasOpenSSL(3, 5) || process.features.openssl_is_boringssl) {
    const pair = await subtle.generateKey(
      { name: 'ML-KEM-768' },
      true,
      ['encapsulateBits', 'decapsulateBits']);
    const result = await withObjectPrototypeSetters(
      ['sharedKey', 'ciphertext'],
      () => subtle.encapsulateBits({ name: 'ML-KEM-768' }, pair.publicKey));

    assert.strictEqual(Object.getPrototypeOf(result), Object.prototype);
    assert.strictEqual(Object.hasOwn(result, 'then'), false);
    assert(result.sharedKey instanceof ArrayBuffer);
    assert(result.ciphertext instanceof ArrayBuffer);
  }
})().then(common.mustCall());

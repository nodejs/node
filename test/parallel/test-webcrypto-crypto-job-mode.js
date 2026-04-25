// Flags: --expose-internals
'use strict';

const common = require('../common');

if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
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
    const pair = await new EcKeyPairGenJob(
      kCryptoJobWebCrypto,
      'P-256',
      undefined,
      { name: 'ECDSA', namedCurve: 'P-256' },
      getUsagesMask(new Set(['verify'])),
      getUsagesMask(new Set(['sign'])),
      true).run();

    assert.strictEqual(Object.getPrototypeOf(pair), Object.prototype);
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
      get() { throw new Error('resolve then getter'); },
    });

    try {
      await assert.rejects(
        subtle.generateKey(
          { name: 'AES-CBC', length: 128 },
          true,
          ['encrypt']),
        /resolve then getter/);
    } finally {
      delete CryptoKey.prototype.then;
    }
  }
})().then(common.mustCall());

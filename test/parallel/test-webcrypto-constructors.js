'use strict';

const common = require('../common');

if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const { hasFIPS } = require('../common/crypto');
const { subtle } = globalThis.crypto;
const fips4 = hasFIPS(4);

// Test CryptoKey constructor
{
  assert.throws(() => new CryptoKey(), {
    name: 'TypeError', message: 'Illegal constructor', code: 'ERR_ILLEGAL_CONSTRUCTOR'
  });
}

// Test SubtleCrypto constructor
{
  assert.throws(() => new SubtleCrypto(), {
    name: 'TypeError', message: 'Illegal constructor', code: 'ERR_ILLEGAL_CONSTRUCTOR'
  });
}

// Test Crypto constructor
{
  assert.throws(() => new Crypto(), {
    name: 'TypeError', message: 'Illegal constructor', code: 'ERR_ILLEGAL_CONSTRUCTOR'
  });
}

const notCrypto = Reflect.construct(function() {}, [], Crypto);
const notSubtle = Reflect.construct(function() {}, [], SubtleCrypto);

// Test Crypto.prototype.subtle
{
  assert.throws(() => notCrypto.subtle, {
    name: 'TypeError', code: 'ERR_INVALID_THIS',
  });
}

// Test Crypto.prototype.randomUUID
{
  assert.throws(() => notCrypto.randomUUID(), {
    name: 'TypeError', code: 'ERR_INVALID_THIS',
  });
}

// Test Crypto.prototype.getRandomValues
{
  assert.throws(() => notCrypto.getRandomValues(new Uint8Array(12)), {
    name: 'TypeError', code: 'ERR_INVALID_THIS',
  });
}

// Test SubtleCrypto.prototype.encrypt
{
  assert.rejects(() => notSubtle.encrypt(), {
    name: 'TypeError', code: 'ERR_INVALID_THIS',
  }).then(common.mustCall());
}

// Test SubtleCrypto.prototype.decrypt
{
  assert.rejects(() => notSubtle.decrypt(), {
    name: 'TypeError', code: 'ERR_INVALID_THIS',
  }).then(common.mustCall());
}

// Test SubtleCrypto.prototype.sign
{
  assert.rejects(() => notSubtle.sign(), {
    name: 'TypeError', code: 'ERR_INVALID_THIS',
  }).then(common.mustCall());
}

// Test SubtleCrypto.prototype.verify
{
  assert.rejects(() => notSubtle.verify(), {
    name: 'TypeError', code: 'ERR_INVALID_THIS',
  }).then(common.mustCall());
}

// Test SubtleCrypto.prototype.digest
{
  assert.rejects(() => notSubtle.digest(), {
    name: 'TypeError', code: 'ERR_INVALID_THIS',
  }).then(common.mustCall());
}

// Test SubtleCrypto.prototype.generateKey
{
  assert.rejects(() => notSubtle.generateKey(), {
    name: 'TypeError', code: 'ERR_INVALID_THIS',
  }).then(common.mustCall());
}

// Test SubtleCrypto.prototype.deriveKey
{
  assert.rejects(() => notSubtle.deriveKey(), {
    name: 'TypeError', code: 'ERR_INVALID_THIS',
  }).then(common.mustCall());
}

// Test SubtleCrypto.prototype.deriveBits
{
  assert.rejects(() => notSubtle.deriveBits(), {
    name: 'TypeError', code: 'ERR_INVALID_THIS',
  }).then(common.mustCall());
}

// Test SubtleCrypto.prototype.importKey
{
  assert.rejects(() => notSubtle.importKey(), {
    name: 'TypeError', code: 'ERR_INVALID_THIS',
  }).then(common.mustCall());
}

// Test SubtleCrypto.prototype.exportKey
{
  assert.rejects(() => notSubtle.exportKey(), {
    name: 'TypeError', code: 'ERR_INVALID_THIS',
  }).then(common.mustCall());
}

// Test SubtleCrypto.prototype.wrapKey
{
  assert.rejects(() => notSubtle.wrapKey(), {
    name: 'TypeError', code: 'ERR_INVALID_THIS',
  }).then(common.mustCall());
}

// Test SubtleCrypto.prototype.unwrapKey
{
  assert.rejects(() => notSubtle.unwrapKey(), {
    name: 'TypeError', code: 'ERR_INVALID_THIS',
  }).then(common.mustCall());
}

// Test SubtleCrypto.supports
{
  assert.throws(() => SubtleCrypto.supports.call(undefined), {
    name: 'TypeError', code: 'ERR_INVALID_THIS',
  });
}

// Test SubtleCrypto.prototype.getPublicKey
{
  assert.rejects(() => notSubtle.getPublicKey(), {
    name: 'TypeError', code: 'ERR_INVALID_THIS',
  }).then(common.mustCall());
}

{
  const keyData = globalThis.crypto.getRandomValues(
    new Uint8Array(fips4 ? 8 : 4));
  const importedKeys = [
    subtle.importKey('raw', keyData, 'PBKDF2', false, ['deriveKey']),
  ];
  if (fips4) {
    importedKeys.push(
      subtle.importKey(
        'raw',
        globalThis.crypto.getRandomValues(new Uint8Array(4)),
        'PBKDF2',
        false,
        ['deriveKey']));
  }

  Promise.all(importedKeys).then(async ([key, weakKey]) => {
    subtle.importKey = common.mustNotCall();
    if (fips4) {
      await assert.rejects(subtle.deriveKey({
        name: 'PBKDF2',
        hash: 'SHA-512',
        salt: new Uint8Array(),
        iterations: 5,
      }, weakKey, {
        name: 'AES-GCM',
        length: 256,
      }, true, ['encrypt', 'decrypt']), { name: 'OperationError' });
    }

    await subtle.deriveKey({
      name: 'PBKDF2',
      hash: 'SHA-512',
      salt: globalThis.crypto.getRandomValues(
        new Uint8Array(fips4 ? 16 : 0)),
      iterations: fips4 ? 1000 : 5,
    }, key, {
      name: 'AES-GCM',
      length: 256
    }, true, ['encrypt', 'decrypt']);
  }).then(common.mustCall());
}

'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const { KeyObject } = require('crypto');

(async () => {
  const key = await globalThis.crypto.subtle.generateKey(
    { name: 'AES-CBC', length: 128 },
    false,  // non-extractable
    ['encrypt'],
  );

  assert.throws(() => {
    KeyObject.from(key);
  }, {
    code: 'ERR_INVALID_ARG_VALUE',
    message: /must be an extractable CryptoKey/,
  });

  let proto = Object.getPrototypeOf(key);
  while (proto && !Object.getOwnPropertyDescriptor(proto, 'extractable')) {
    proto = Object.getPrototypeOf(proto);
  }
  assert.ok(proto, 'could not find CryptoKey.prototype');
  const descriptor = Object.getOwnPropertyDescriptor(proto, 'extractable');
  Object.defineProperty(proto, 'extractable', {
    __proto__: null,
    configurable: true,
    get() { return true; },
  });

  try {
    assert.strictEqual(key.extractable, true);
    assert.throws(() => {
      KeyObject.from(key);
    }, {
      code: 'ERR_INVALID_ARG_VALUE',
      message: /must be an extractable CryptoKey/,
    });
  } finally {
    Object.defineProperty(proto, 'extractable', descriptor);
  }

  const extractableKey = await globalThis.crypto.subtle.generateKey(
    { name: 'AES-CBC', length: 128 },
    true,
    ['encrypt'],
  );

  assert.strictEqual(KeyObject.from(extractableKey).type, 'secret');
})().then(common.mustCall());

'use strict';

// The four CryptoKey prototype getters (`type`, `extractable`,
// `algorithm`, `usages`) are user-configurable per Web IDL, so they
// can be invoked with an arbitrary `this`. The native callbacks that
// implement them must brand-check their receiver and throw cleanly
// (ERR_INVALID_THIS) rather than crashing the process or returning
// garbage. This test exercises four progressively more hostile
// receiver shapes, including subverting `instanceof` via
// `Symbol.hasInstance`, to make sure the C++ brand check holds.
//
// It also verifies that `util.types.isCryptoKey()` cannot be fooled
// by prototype spoofing.

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('node:assert');
const { types: { isCryptoKey } } = require('node:util');
const { subtle } = globalThis.crypto;

(async () => {
  const key = await subtle.generateKey(
    { name: 'HMAC', hash: 'SHA-256' },
    true,
    ['sign'],
  );

  const CryptoKey = key.constructor;

  // Capture the underlying prototype getters once, so that subsequent
  // tampering with `CryptoKey.prototype` cannot affect what we call.
  const getters = {
    type: Object.getOwnPropertyDescriptor(CryptoKey.prototype, 'type').get,
    extractable:
      Object.getOwnPropertyDescriptor(CryptoKey.prototype, 'extractable').get,
    algorithm:
      Object.getOwnPropertyDescriptor(CryptoKey.prototype, 'algorithm').get,
    usages:
      Object.getOwnPropertyDescriptor(CryptoKey.prototype, 'usages').get,
  };

  // Sanity: each getter works on a real CryptoKey.
  Object.entries(getters).forEach(([name, getter]) => {
    assert.notStrictEqual(getter.call(key), undefined, `baseline ${name}`);
  });
  assert.strictEqual(isCryptoKey(key), true);

  const invalidThis = { code: 'ERR_INVALID_THIS', name: 'TypeError' };

  // Plain object receiver.
  Object.entries(getters).forEach(([, getter]) => {
    assert.throws(() => getter.call({}), invalidThis);
  });

  // Null-prototype object receiver.
  Object.entries(getters).forEach(([, getter]) => {
    assert.throws(() => getter.call({ __proto__: null }), invalidThis);
  });

  // Primitive receiver.
  Object.entries(getters).forEach(([, getter]) => {
    assert.throws(() => getter.call(1), invalidThis);
  });

  // Null.
  Object.entries(getters).forEach(([, getter]) => {
    // eslint-disable-next-line no-useless-call
    assert.throws(() => getter.call(null), invalidThis);
  });

  // Undefined.
  Object.entries(getters).forEach(([, getter]) => {
    assert.throws(() => getter.call(), invalidThis);
  });

  // Function
  Object.entries(getters).forEach(([, getter]) => {
    assert.throws(() => getter.call(function() {}), invalidThis);
  });

  // Prototype spoofing with InternalCryptoKey.prototype must not pass
  // util.types.isCryptoKey().
  const spoofed = {};
  Object.setPrototypeOf(spoofed, Object.getPrototypeOf(key));
  assert.strictEqual(spoofed instanceof CryptoKey, true);
  assert.strictEqual(isCryptoKey(spoofed), false);
  await assert.rejects(
    subtle.sign('HMAC', spoofed, Buffer.from('payload')),
    invalidThis);
  await assert.rejects(
    subtle.exportKey('jwk', spoofed),
    invalidThis);

  // Subvert `instanceof CryptoKey` via Symbol.hasInstance, then
  // invoke the native getters on a forged object. The C++ tag
  // check must reject the receiver even though `instanceof`
  // reports true.
  Object.defineProperty(CryptoKey, Symbol.hasInstance, {
    configurable: true,
    value: () => true,
  });
  const fake = { foo: 'bar' };
  assert.strictEqual(fake instanceof CryptoKey, true);
  assert.strictEqual(isCryptoKey(fake), false);
  Object.entries(getters).forEach(([, getter]) => {
    assert.throws(() => getter.call(fake), invalidThis);
  });

  // Subverted `instanceof` plus a real BaseObject of a different
  // kind (a Buffer) as the receiver. Without the C++ tag check
  // this would type-confuse `Unwrap<NativeCryptoKey>`.
  const buf = Buffer.alloc(16);
  assert.strictEqual(buf instanceof CryptoKey, true);
  assert.strictEqual(isCryptoKey(buf), false);
  Object.entries(getters).forEach(([, getter]) => {
    assert.throws(() => getter.call(buf), invalidThis);
  });

  // The real CryptoKey continues to work after all of the above.
  assert.strictEqual(getters.type.call(key), 'secret');
  assert.strictEqual(getters.extractable.call(key), true);
  assert.strictEqual(getters.algorithm.call(key).name, 'HMAC');
  assert.deepStrictEqual(getters.usages.call(key), ['sign']);
})().then(common.mustCall());

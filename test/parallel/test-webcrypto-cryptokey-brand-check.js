'use strict';

// CryptoKey prototype getters and methods can be invoked with an
// arbitrary `this`. They must brand-check their receiver and throw
// cleanly (ERR_INVALID_THIS) rather than crashing the process or
// returning garbage. This test exercises invalid receiver shapes,
// including subverting `instanceof` via `Symbol.hasInstance`.
//
// It also verifies that `util.types.isCryptoKey()` cannot be fooled
// by prototype spoofing.

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('node:assert');
const { inspect, types: { isCryptoKey } } = require('node:util');
const { subtle } = globalThis.crypto;

(async () => {
  const key = await subtle.generateKey(
    { name: 'HMAC', hash: 'SHA-256' },
    true,
    ['sign'],
  );

  const CryptoKey = key.constructor;

  // Capture the underlying prototype members once, so that subsequent
  // tampering with `CryptoKey.prototype` cannot affect what we call.
  const descriptors = Object.getOwnPropertyDescriptors(CryptoKey.prototype);

  // Sanity: each getter works on a real CryptoKey.
  for (const name of Reflect.ownKeys(descriptors)) {
    const { get } = descriptors[name];
    if (get !== undefined)
      Reflect.apply(get, key, []);
  }
  assert.strictEqual(isCryptoKey(key), true);
  assert.strictEqual(Object.hasOwn(CryptoKey, 'getSlots'), false);
  const internalProto = Object.getPrototypeOf(key);
  assert.strictEqual(Object.hasOwn(internalProto, 'getSlots'), false);
  assert.strictEqual('getSlots' in internalProto, false);
  assert.strictEqual(internalProto.constructor, CryptoKey);
  assert.strictEqual(Object.getPrototypeOf(internalProto), CryptoKey.prototype);

  const invalidThis = { code: 'ERR_INVALID_THIS', name: 'TypeError' };
  const invalidArgType = { code: 'ERR_INVALID_ARG_TYPE', name: 'TypeError' };

  async function assertInvalidReceiver(receiver) {
    for (const name of Reflect.ownKeys(descriptors)) {
      if (name === 'constructor') continue;
      const descriptor = descriptors[name];
      const args = name === inspect.custom ? [0, {}] : [];
      for (const kind of ['get', 'set', 'value']) {
        const member = descriptor[kind];
        if (typeof member !== 'function') continue;
        await assert.rejects(
          async () => Reflect.apply(member, receiver, args),
          invalidThis,
          `CryptoKey.${String(name)} (${kind})`,
        );
      }
    }
  }

  // Plain object receiver.
  await assertInvalidReceiver({});

  // Null-prototype object receiver.
  await assertInvalidReceiver({ __proto__: null });

  // Primitive receiver.
  await assertInvalidReceiver(1);

  // Null.
  await assertInvalidReceiver(null);

  // Undefined.
  await assertInvalidReceiver(undefined);

  // Function
  await assertInvalidReceiver(function() {});

  const revoked = Proxy.revocable(key, {});
  revoked.revoke();
  for (const receiver of [
    { __proto__: CryptoKey.prototype },
    { __proto__: key },
    new Proxy(key, {}),
    revoked.proxy,
  ]) {
    await assertInvalidReceiver(receiver);
  }

  // Prototype spoofing with InternalCryptoKey.prototype must not pass
  // util.types.isCryptoKey().
  const spoofed = {};
  Object.setPrototypeOf(spoofed, Object.getPrototypeOf(key));
  assert.strictEqual(spoofed instanceof CryptoKey, true);
  assert.strictEqual(isCryptoKey(spoofed), false);
  await assert.rejects(
    subtle.sign('HMAC', spoofed, Buffer.from('payload')),
    invalidArgType);
  await assert.rejects(
    subtle.exportKey('jwk', spoofed),
    invalidArgType);

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
  await assertInvalidReceiver(fake);

  // Subverted `instanceof` plus a real BaseObject of a different
  // kind (a Buffer) as the receiver. Without the C++ tag check
  // this would type-confuse `Unwrap<NativeCryptoKey>`.
  const buf = Buffer.alloc(16);
  assert.strictEqual(buf instanceof CryptoKey, true);
  assert.strictEqual(isCryptoKey(buf), false);
  await assertInvalidReceiver(buf);

  // The real CryptoKey continues to work after all of the above.
  assert.strictEqual(descriptors.type.get.call(key), 'secret');
  assert.strictEqual(descriptors.extractable.get.call(key), true);
  assert.strictEqual(descriptors.algorithm.get.call(key).name, 'HMAC');
  assert.deepStrictEqual(descriptors.usages.get.call(key), ['sign']);
})().then(common.mustCall());

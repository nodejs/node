'use strict';

// KeyObject instances are backed by NativeKeyObject and must be
// recognized by native brand, not by public prototype shape or
// forgeable own properties.

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('node:assert');
const {
  createHmac,
  createSecretKey,
  generateKeyPairSync,
  KeyObject,
} = require('node:crypto');
const { types: { isKeyObject } } = require('node:util');

const invalidThis = { code: 'ERR_INVALID_THIS', name: 'TypeError' };

function getter(proto, name) {
  return Object.getOwnPropertyDescriptor(proto, name).get;
}

{
  const secret = createSecretKey(Buffer.alloc(16));
  const { publicKey } = generateKeyPairSync('rsa', { modulusLength: 1024 });

  const type = getter(KeyObject.prototype, 'type');
  const symmetricKeySize =
    getter(Object.getPrototypeOf(secret), 'symmetricKeySize');
  const asymmetricProto = Object.getPrototypeOf(Object.getPrototypeOf(publicKey));
  const asymmetricKeyType = getter(asymmetricProto, 'asymmetricKeyType');
  const asymmetricKeyDetails = getter(asymmetricProto, 'asymmetricKeyDetails');

  assert.strictEqual(isKeyObject(secret), true);
  assert.strictEqual(isKeyObject(publicKey), true);
  assert.strictEqual(Object.hasOwn(KeyObject, 'getSlots'), false);
  for (const key of [secret, publicKey]) {
    for (let proto = Object.getPrototypeOf(key);
      proto !== null;
      proto = Object.getPrototypeOf(proto)) {
      assert.strictEqual(Object.hasOwn(proto, 'getSlots'), false);
      assert.strictEqual('getSlots' in proto, false);
      if (Object.hasOwn(proto, 'constructor')) {
        assert.strictEqual(Object.hasOwn(proto.constructor, 'getSlots'), false);
        assert.strictEqual(proto.constructor.getSlots, undefined);
      }
    }
  }

  for (const value of [{}, { __proto__: null }, 1, null, undefined,
                       Buffer.alloc(1), function() {}]) {
    assert.throws(() => type.call(value), invalidThis);
    assert.throws(() => symmetricKeySize.call(value), invalidThis);
    assert.throws(() => asymmetricKeyType.call(value), invalidThis);
    assert.throws(() => asymmetricKeyDetails.call(value), invalidThis);
  }

  assert.throws(() => symmetricKeySize.call(publicKey), invalidThis);
  assert.throws(() => asymmetricKeyType.call(secret), invalidThis);
  assert.throws(() => asymmetricKeyDetails.call(secret), invalidThis);

  const spoofed = {};
  Object.setPrototypeOf(spoofed, Object.getPrototypeOf(secret));
  assert.strictEqual(spoofed instanceof KeyObject, true);
  assert.strictEqual(isKeyObject(spoofed), false);
  assert.throws(() => type.call(spoofed), invalidThis);
  assert.throws(() => symmetricKeySize.call(spoofed), invalidThis);
  assert.throws(() => createHmac('sha256', spoofed), {
    code: 'ERR_INVALID_ARG_TYPE',
  });

  const originalHasInstance =
    Object.getOwnPropertyDescriptor(KeyObject, Symbol.hasInstance);
  Object.defineProperty(KeyObject, Symbol.hasInstance, {
    configurable: true,
    value: () => true,
  });
  try {
    const buf = Buffer.alloc(16);
    assert.strictEqual(buf instanceof KeyObject, true);
    assert.strictEqual(isKeyObject(buf), false);
    assert.throws(() => type.call(buf), invalidThis);
    assert.throws(() => symmetricKeySize.call(buf), invalidThis);
    assert.throws(() => asymmetricKeyType.call(buf), invalidThis);
    assert.throws(() => asymmetricKeyDetails.call(buf), invalidThis);
  } finally {
    if (originalHasInstance === undefined) {
      delete KeyObject[Symbol.hasInstance];
    } else {
      Object.defineProperty(KeyObject, Symbol.hasInstance, originalHasInstance);
    }
  }
}

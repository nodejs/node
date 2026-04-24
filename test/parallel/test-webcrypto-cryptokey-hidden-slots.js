'use strict';

// CryptoKey prototype getters (`type`, `extractable`,
// `algorithm`, `usages`) are configurable in the Web Crypto IDL and
// can therefore be replaced by user code. Internal consumers of those
// attributes, and the custom inspect output, must NOT go through the
// public prototype getters, they must read the underlying native
// internal slots directly. This test mutates the prototype getters
// (and mutates the per-instance `algorithm`/`usages` caches returned
// by those getters) and asserts that:
//
//   1. util.inspect() shows the real internal values, unaffected by
//      the replacement getters.
//   2. Internal Web Crypto and Node.js crypto bridge operations that
//      receive the mutated CryptoKey still succeed and observe the real
//      internal state, not the replaced one.

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('node:assert');
const {
  createHmac,
  KeyObject,
  sign: cryptoSign,
  verify: cryptoVerify,
} = require('node:crypto');
const { inspect } = require('node:util');
const { subtle } = globalThis.crypto;

common.expectWarning({
  DeprecationWarning: {
    DEP0203: 'Passing a CryptoKey to node:crypto functions is deprecated.',
  },
});

(async () => {
  const key = await subtle.generateKey(
    { name: 'HMAC', hash: 'SHA-256' },
    true,
    ['sign', 'verify'],
  );
  const { publicKey: ecPublicKey, privateKey: ecPrivateKey } =
    await subtle.generateKey(
      { name: 'ECDSA', namedCurve: 'P-256' },
      false,
      ['sign', 'verify'],
    );

  // Snapshot the real values BEFORE tampering.
  const realType = key.type;
  const realExtractable = key.extractable;
  const realAlgorithm = { ...key.algorithm, hash: { ...key.algorithm.hash } };
  const realUsages = [...key.usages];

  // 1) Replace all four prototype getters.
  let proto = Object.getPrototypeOf(key);
  while (proto && !Object.getOwnPropertyDescriptor(proto, 'type')) {
    proto = Object.getPrototypeOf(proto);
  }
  assert.ok(proto, 'could not find CryptoKey.prototype');
  const forgedDescriptors = {
    type: {
      configurable: true,
      enumerable: true,
      get() { return 'FORGED-TYPE'; },
    },
    extractable: {
      configurable: true,
      enumerable: true,
      get() { return 'FORGED-EXTRACTABLE'; },
    },
    algorithm: {
      configurable: true,
      enumerable: true,
      get() { return { name: 'FORGED-ALGORITHM', hash: { name: 'FORGED-HASH' } }; },
    },
    usages: {
      configurable: true,
      enumerable: true,
      get() { return ['forged-usage']; },
    },
  };
  const originalDescriptors = {};
  for (const [name, descriptor] of Object.entries(forgedDescriptors)) {
    originalDescriptors[name] = Object.getOwnPropertyDescriptor(proto, name);
    Object.defineProperty(proto, name, descriptor);
  }

  try {
    // Confirm the forgeries are in effect from user-code perspective.
    assert.strictEqual(key.type, 'FORGED-TYPE');
    assert.strictEqual(key.extractable, 'FORGED-EXTRACTABLE');
    assert.strictEqual(key.algorithm.name, 'FORGED-ALGORITHM');
    assert.deepStrictEqual(key.usages, ['forged-usage']);

    // 2) util.inspect() must not be influenced by the forged getters,
    //    it must read the real internal slots directly.
    const rendered = inspect(key, { depth: 4 });
    assert.match(rendered, /type: 'secret'/);
    assert.match(rendered, /extractable: true/);
    assert.match(rendered, /name: 'HMAC'/);
    assert.match(rendered, /name: 'SHA-256'/);
    assert.match(rendered, /'sign'/);
    assert.match(rendered, /'verify'/);
    assert.doesNotMatch(rendered, /FORGED/);

    // 3) Internal consumers that receive this CryptoKey must see the
    //    real internal slots. exportKey('jwk') reads [[type]],
    //    [[extractable]], [[algorithm]], and [[usages]]; if any
    //    went through the user-visible getter the call would either
    //    throw or produce forged output.
    const jwk = await subtle.exportKey('jwk', key);
    assert.strictEqual(jwk.kty, 'oct');
    assert.strictEqual(jwk.alg, 'HS256');
    assert.strictEqual(jwk.ext, true);
    assert.deepStrictEqual(jwk.key_ops.sort(), ['sign', 'verify']);

    // 4) The Node.js crypto bridge must also read the real native
    //    slots directly, both for KeyObject.from() and for deprecated
    //    direct CryptoKey consumption.
    const keyObject = KeyObject.from(key);
    assert.strictEqual(keyObject.type, 'secret');
    assert.deepStrictEqual(keyObject.export(), Buffer.from(jwk.k, 'base64url'));

    const payload = Buffer.from('payload');
    const digest = createHmac('sha256', key).update(payload).digest('hex');
    const expectedDigest =
      createHmac('sha256', keyObject).update(payload).digest('hex');
    assert.strictEqual(digest, expectedDigest);

    const signature = cryptoSign('sha256', payload, ecPrivateKey);
    assert.strictEqual(
      cryptoVerify('sha256', payload, ecPublicKey, signature),
      true);

    // 5) Importing back from the exported JWK must yield an equivalent
    //    key, i.e. the real algorithm and usages round-trip.
    const reimported = await subtle.importKey('jwk', jwk,
                                              { name: 'HMAC', hash: 'SHA-256' },
                                              true, ['sign', 'verify']);
    // Reimported's prototype is the same mutated prototype, so we must
    // also inspect() the reimported key to check real slots.
    const rerendered = inspect(reimported, { depth: 4 });
    assert.match(rerendered, /type: 'secret'/);
    assert.match(rerendered, /name: 'HMAC'/);
    assert.doesNotMatch(rerendered, /FORGED/);
  } finally {
    // Restore the original getters so subsequent tests are unaffected.
    for (const [name, descriptor] of Object.entries(originalDescriptors)) {
      Object.defineProperty(proto, name, descriptor);
    }
  }

  // After restoration, the real values come back through the public API.
  assert.strictEqual(key.type, realType);
  assert.strictEqual(key.extractable, realExtractable);
  assert.deepStrictEqual(key.algorithm, realAlgorithm);
  assert.deepStrictEqual(key.usages, realUsages);
})().then(common.mustCall());

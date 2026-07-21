// Flags: --expose-internals

import * as common from '../common/index.mjs';
import assert from 'node:assert';
import { createRequire } from 'node:module';

if (!common.hasCrypto) common.skip('missing crypto');

// WebCrypto subtle methods must not leak intermediate values through
// Promise.prototype.then, constructor/species, thenable assimilation, or
// inherited accessors on internally-created JWK/result objects.
// Regression test for https://github.com/nodejs/node/pull/61492
// and https://github.com/nodejs/node/issues/59699.
//
// When adding WebCrypto algorithms:
// - Add a fixture with addFixture() below. Prefer the shared fixture builders
//   unless an algorithm needs operation-specific parameters.
// - Add new operation names to operationOrder and implement that operation on
//   every affected fixture.
// - Add new "get key length" algorithms to keyLengthTargets, unless the
//   algorithm is itself a KDF whose getKeyLength() result is null; those belong
//   in nullKeyLengthAlgorithms.
// The registry assertions at the bottom make missing updates fail loudly.

const require = createRequire(import.meta.url);
const { kSupportedAlgorithms } = require('internal/crypto/util');
const { subtle } = globalThis.crypto;

Promise.prototype.then = common.mustNotCall('Promise.prototype.then');

const data = new TextEncoder().encode('prototype pollution');

// WebCrypto methods return native promises. Re-wrapping a promise with
// PromiseResolve() or chaining it with Promise.prototype.then can read
// user-mutated constructor/species accessors.
function assertNoPromiseConstructorAccess(name, fn) {
  const constructorDescriptor =
    Object.getOwnPropertyDescriptor(Promise.prototype, 'constructor');
  const speciesDescriptor =
    Object.getOwnPropertyDescriptor(Promise, Symbol.species);
  let promise;
  Object.defineProperty(Promise.prototype, 'constructor', {
    __proto__: null,
    configurable: true,
    get: common.mustNotCall(
      `${name} Promise.prototype.constructor getter`),
  });
  Object.defineProperty(Promise, Symbol.species, {
    __proto__: null,
    configurable: true,
    get: common.mustNotCall(`${name} Promise[Symbol.species] getter`),
  });
  try {
    promise = fn();
  } finally {
    Object.defineProperty(
      Promise.prototype,
      'constructor',
      constructorDescriptor);
    Object.defineProperty(Promise, Symbol.species, speciesDescriptor);
  }
  return promise;
}

// Exercise each export format through the same promise-constructor guard.
function assertExportKeyNoPromiseConstructorAccess(name, format, key) {
  return assertNoPromiseConstructorAccess(`exportKey ${name}`, () =>
    subtle.exportKey(format, key));
}

// Non-promise object results must be fulfilled without thenable assimilation
// observing inherited then accessors on the returned object.
async function assertNoInheritedThenAccess(name, prototype, prototypeName, fn) {
  const descriptor = Object.getOwnPropertyDescriptor(prototype, 'then');
  Object.defineProperty(prototype, 'then', {
    __proto__: null,
    configurable: true,
    get: common.mustNotCall(`${name} ${prototypeName}.prototype.then`),
  });
  try {
    return await fn();
  } finally {
    if (descriptor === undefined) {
      delete prototype.then;
    } else {
      Object.defineProperty(prototype, 'then', descriptor);
    }
  }
}

function assertNoInheritedArrayBufferThenAccess(name, fn) {
  return assertNoInheritedThenAccess(
    name,
    ArrayBuffer.prototype,
    'ArrayBuffer',
    fn);
}

function assertNoInheritedCryptoKeyThenAccess(name, fn) {
  return assertNoInheritedThenAccess(
    name,
    CryptoKey.prototype,
    'CryptoKey',
    fn);
}

function assertNoInheritedObjectThenAccess(name, fn) {
  return assertNoInheritedThenAccess(
    name,
    Object.prototype,
    'Object',
    fn);
}

function assertCryptoKeyResult(name, fn) {
  return assertNoInheritedCryptoKeyThenAccess(name, () =>
    assertNoPromiseConstructorAccess(name, fn));
}

// wrapKey('jwk') stringifies an internally exported JWK. The spec does this
// in a fresh global, so inherited toJSON hooks from the current global must
// not observe or replace key material.
async function assertNoInheritedToJSONAccess(name, fn) {
  const objectDescriptor =
    Object.getOwnPropertyDescriptor(Object.prototype, 'toJSON');
  const arrayDescriptor =
    Object.getOwnPropertyDescriptor(Array.prototype, 'toJSON');
  Object.defineProperty(Object.prototype, 'toJSON', {
    __proto__: null,
    configurable: true,
    value: common.mustNotCall(`${name} Object.prototype.toJSON`),
  });
  Object.defineProperty(Array.prototype, 'toJSON', {
    __proto__: null,
    configurable: true,
    value: common.mustNotCall(`${name} Array.prototype.toJSON`),
  });
  try {
    return await fn();
  } finally {
    if (objectDescriptor === undefined) {
      delete Object.prototype.toJSON;
    } else {
      Object.defineProperty(Object.prototype, 'toJSON', objectDescriptor);
    }
    if (arrayDescriptor === undefined) {
      delete Array.prototype.toJSON;
    } else {
      Object.defineProperty(Array.prototype, 'toJSON', arrayDescriptor);
    }
  }
}

// JWK export creates and fills a result object. The exported members must be
// own data properties, not writes that can observe inherited accessors. Keep
// this list complete: if exportKey('jwk') starts returning a new JWK member,
// this helper must poison that member on Object.prototype too.
async function assertNoInheritedJwkPropertyAccess(name, fn) {
  const properties = [
    'alg',
    'crv',
    'd',
    'dp',
    'dq',
    'e',
    'ext',
    'k',
    'key_ops',
    'kty',
    'n',
    'p',
    'priv',
    'pub',
    'q',
    'qi',
    'x',
    'y',
  ];
  const handledProperties = new Set(properties);
  const descriptors = new Map();
  for (const property of properties) {
    descriptors.set(
      property,
      Object.getOwnPropertyDescriptor(Object.prototype, property));
    Object.defineProperty(Object.prototype, property, {
      __proto__: null,
      configurable: true,
      get: common.mustNotCall(`${name} Object.prototype.${property} getter`),
      set: common.mustNotCall(`${name} Object.prototype.${property} setter`),
    });
  }
  let result;
  try {
    result = await fn();
  } finally {
    for (const property of properties) {
      const descriptor = descriptors.get(property);
      if (descriptor === undefined) {
        delete Object.prototype[property];
      } else {
        Object.defineProperty(Object.prototype, property, descriptor);
      }
    }
  }

  if (result !== null && typeof result === 'object') {
    for (const property of Object.keys(result)) {
      assert(
        handledProperties.has(property),
        `${name} returned unhandled JWK property ${property}`);
    }
  }

  return result;
}

// unwrapKey('jwk') parses a JWK and then converts it to the JsonWebKey IDL
// dictionary. The parsed JWK must provide its own kty member; an inherited
// Object.prototype.kty must not satisfy that required WebCrypto step.
async function assertMissingJwkKtyIgnoresPrototype(fn) {
  const descriptor = Object.getOwnPropertyDescriptor(Object.prototype, 'kty');
  Object.defineProperty(Object.prototype, 'kty', {
    __proto__: null,
    configurable: true,
    value: 'oct',
  });
  try {
    await assert.rejects(fn(), { name: 'DataError' });
  } finally {
    if (descriptor === undefined) {
      delete Object.prototype.kty;
    } else {
      Object.defineProperty(Object.prototype, 'kty', descriptor);
    }
  }
}

// wrapKey('jwk') UTF-8 encodes the JSON string. That step must not rely on
// user-mutable encoding APIs such as TextEncoder or Buffer.
async function assertNoUserMutableEncodeAccess(name, fn) {
  const textEncoderDescriptor =
    Object.getOwnPropertyDescriptor(TextEncoder.prototype, 'encode');
  const bufferFromDescriptor = Object.getOwnPropertyDescriptor(Buffer, 'from');
  Object.defineProperty(TextEncoder.prototype, 'encode', {
    __proto__: null,
    configurable: true,
    value: common.mustNotCall(`${name} TextEncoder.prototype.encode`),
  });
  Object.defineProperty(Buffer, 'from', {
    __proto__: null,
    configurable: true,
    value: common.mustNotCall(`${name} Buffer.from`),
  });
  try {
    return await fn();
  } finally {
    Object.defineProperty(
      TextEncoder.prototype,
      'encode',
      textEncoderDescriptor);
    Object.defineProperty(Buffer, 'from', bufferFromDescriptor);
  }
}

// unwrapKey('jwk') decodes the wrapped bytes as UTF-8. That step must not
// rely on user-mutable encoding APIs such as TextDecoder or Buffer.
async function assertNoUserMutableDecodeAccess(name, fn) {
  const textDecoderDescriptor =
    Object.getOwnPropertyDescriptor(TextDecoder.prototype, 'decode');
  const bufferFromDescriptor = Object.getOwnPropertyDescriptor(Buffer, 'from');
  const bufferToStringDescriptor =
    Object.getOwnPropertyDescriptor(Buffer.prototype, 'toString');
  Object.defineProperty(TextDecoder.prototype, 'decode', {
    __proto__: null,
    configurable: true,
    value: common.mustNotCall(`${name} TextDecoder.prototype.decode`),
  });
  Object.defineProperty(Buffer, 'from', {
    __proto__: null,
    configurable: true,
    value: common.mustNotCall(`${name} Buffer.from`),
  });
  Object.defineProperty(Buffer.prototype, 'toString', {
    __proto__: null,
    configurable: true,
    value: common.mustNotCall(`${name} Buffer.prototype.toString`),
  });
  try {
    return await fn();
  } finally {
    Object.defineProperty(
      TextDecoder.prototype,
      'decode',
      textDecoderDescriptor);
    Object.defineProperty(Buffer, 'from', bufferFromDescriptor);
    Object.defineProperty(
      Buffer.prototype,
      'toString',
      bufferToStringDescriptor);
  }
}

// encapsulateKey() first resolves an internal encapsulateBits job whose result
// object contains a raw sharedKey. The final method result is also an object,
// but its sharedKey is a CryptoKey and is intentionally returned to the caller.
async function assertNoRawSharedKeyObjectThenAccess(name, fn) {
  const descriptor = Object.getOwnPropertyDescriptor(Object.prototype, 'then');
  Object.defineProperty(Object.prototype, 'then', {
    __proto__: null,
    configurable: true,
    get() {
      if (Object.hasOwn(this, 'sharedKey') &&
          this.sharedKey instanceof ArrayBuffer) {
        assert.fail(`${name} Object.prototype.then observed raw sharedKey`);
      }
      return undefined;
    },
  });
  try {
    return await fn();
  } finally {
    if (descriptor === undefined) {
      delete Object.prototype.then;
    } else {
      Object.defineProperty(Object.prototype, 'then', descriptor);
    }
  }
}

function algorithm(name, params = {}) {
  return { name, ...params };
}

function rsaAlgorithm(name) {
  return algorithm(name, {
    modulusLength: 1024,
    publicExponent: new Uint8Array([1, 0, 1]),
    hash: 'SHA-256',
  });
}

function importRsaAlgorithm(name) {
  return algorithm(name, { hash: 'SHA-256' });
}

function exportArrayBuffer(name, format, key) {
  return assertNoInheritedArrayBufferThenAccess(`exportKey ${format} ${name}`, () =>
    assertExportKeyNoPromiseConstructorAccess(`${format} ${name}`, format, key));
}

function exportJwk(name, key) {
  return assertNoInheritedJwkPropertyAccess(`exportKey jwk ${name}`, () =>
    assertNoInheritedObjectThenAccess(`exportKey jwk ${name}`, () =>
      assertExportKeyNoPromiseConstructorAccess(`jwk ${name}`, 'jwk', key)));
}

function importCryptoKey(name, format, keyData, importAlgorithm, extractable, usages) {
  return assertCryptoKeyResult(`importKey ${format} ${name}`, () =>
    subtle.importKey(format, keyData, importAlgorithm, extractable, usages));
}

async function getKeyToWrap() {
  if (getKeyToWrap.key === undefined) {
    getKeyToWrap.key = await subtle.generateKey(
      algorithm('AES-CBC', { length: 128 }),
      true,
      ['encrypt']);
  }
  return getKeyToWrap.key;
}

function addCommonKeyExportTests(fixture) {
  fixture.exportKey = async (ctx) => {
    if (fixture.rawFormat !== undefined)
      ctx.raw = await exportArrayBuffer(fixture.name, fixture.rawFormat, ctx.key);
    ctx.jwk = await exportJwk(fixture.name, ctx.key);
  };
  fixture.importKey = async (ctx) => {
    if (fixture.rawFormat !== undefined) {
      ctx.importedRaw = await importCryptoKey(
        fixture.name,
        fixture.rawFormat,
        ctx.raw,
        fixture.importAlgorithm,
        true,
        fixture.usages);
    }
    ctx.importedJwk = await importCryptoKey(
      fixture.name,
      'jwk',
      ctx.jwk,
      fixture.importAlgorithm,
      true,
      fixture.usages);
  };
}

function secretKeyFixture(options) {
  const fixture = {
    ...options,
    generateKey: async (ctx) => {
      ctx.key = await assertNoPromiseConstructorAccess(`generateKey ${options.name}`, () =>
        subtle.generateKey(options.generateAlgorithm, true, options.usages));
    },
  };

  addCommonKeyExportTests(fixture);

  if (options.encryptAlgorithm !== undefined) {
    fixture.encrypt = async (ctx) => {
      ctx.ciphertext = await assertNoPromiseConstructorAccess(`encrypt ${options.name}`, () =>
        subtle.encrypt(options.encryptAlgorithm, ctx.key, data));
    };
    fixture.decrypt = async (ctx) => {
      await assertNoPromiseConstructorAccess(`decrypt ${options.name}`, () =>
        subtle.decrypt(options.encryptAlgorithm, ctx.key, ctx.ciphertext));
    };
  }

  if (options.signAlgorithm !== undefined) {
    fixture.sign = async (ctx) => {
      ctx.signature = await assertNoPromiseConstructorAccess(`sign ${options.name}`, () =>
        subtle.sign(options.signAlgorithm, ctx.key, data));
    };
    fixture.verify = async (ctx) => {
      await assertNoPromiseConstructorAccess(`verify ${options.name}`, () =>
        subtle.verify(options.signAlgorithm, ctx.key, ctx.signature, data));
    };
  }

  if (options.wrapAlgorithm !== undefined) {
    fixture.wrapKey = async (ctx) => {
      const keyToWrap = await getKeyToWrap();
      ctx.wrappedRawSecret = await assertNoPromiseConstructorAccess(
        `wrapKey raw-secret ${options.name}`,
        () => subtle.wrapKey('raw-secret', keyToWrap, ctx.key, options.wrapAlgorithm));
      ctx.wrappedJwk = await assertNoInheritedJwkPropertyAccess(
        `wrapKey jwk ${options.name}`,
        () => assertNoInheritedToJSONAccess(
          `wrapKey jwk ${options.name}`,
          () => assertNoUserMutableEncodeAccess(
            `wrapKey jwk ${options.name}`, () =>
              assertNoPromiseConstructorAccess(`wrapKey jwk ${options.name}`, () =>
                subtle.wrapKey('jwk', keyToWrap, ctx.key, options.wrapAlgorithm)))));
    };
    fixture.unwrapKey = async (ctx) => {
      await assertNoInheritedArrayBufferThenAccess(`unwrapKey raw-secret ${options.name}`, () =>
        assertCryptoKeyResult(`unwrapKey raw-secret ${options.name} result`, () =>
          subtle.unwrapKey(
            'raw-secret',
            ctx.wrappedRawSecret,
            ctx.key,
            options.wrapAlgorithm,
            algorithm('AES-CBC', { length: 128 }),
            true,
            ['encrypt'])));
      await assertNoUserMutableDecodeAccess(`unwrapKey jwk ${options.name}`, () =>
        assertNoInheritedArrayBufferThenAccess(`unwrapKey jwk ${options.name}`, () =>
          assertCryptoKeyResult(`unwrapKey jwk ${options.name} result`, () =>
            subtle.unwrapKey(
              'jwk',
              ctx.wrappedJwk,
              ctx.key,
              options.wrapAlgorithm,
              algorithm('AES-CBC', { length: 128 }),
              true,
              ['encrypt']))));
    };
  }

  return fixture;
}

function pairKeyFixture(options) {
  const fixture = {
    ...options,
    generateKey: async (ctx) => {
      ctx.keyPair = await assertNoPromiseConstructorAccess(`generateKey ${options.name}`, () =>
        subtle.generateKey(options.generateAlgorithm, true, options.usages));
    },
    exportKey: async (ctx) => {
      if (options.spki !== false) {
        ctx.spki = await exportArrayBuffer(`${options.name} public`, 'spki', ctx.keyPair.publicKey);
        ctx.pkcs8 = await exportArrayBuffer(`${options.name} private`, 'pkcs8', ctx.keyPair.privateKey);
      }
      if (options.rawPublic === true) {
        ctx.rawPublic = await exportArrayBuffer(
          `${options.name} public`,
          'raw-public',
          ctx.keyPair.publicKey);
      }
      if (options.rawSeed === true) {
        ctx.rawSeed = await exportArrayBuffer(
          `${options.name} private`,
          'raw-seed',
          ctx.keyPair.privateKey);
      }
      ctx.publicJwk = await exportJwk(`${options.name} public`, ctx.keyPair.publicKey);
      ctx.privateJwk = await exportJwk(`${options.name} private`, ctx.keyPair.privateKey);
    },
    importKey: async (ctx) => {
      if (options.spki !== false) {
        ctx.importedSpki = await importCryptoKey(
          `${options.name} public`,
          'spki',
          ctx.spki,
          options.importAlgorithm,
          true,
          options.publicUsages);
        ctx.importedPkcs8 = await importCryptoKey(
          `${options.name} private`,
          'pkcs8',
          ctx.pkcs8,
          options.importAlgorithm,
          true,
          options.privateUsages);
      }
      if (options.rawPublic === true) {
        ctx.importedRawPublic = await importCryptoKey(
          `${options.name} public`,
          'raw-public',
          ctx.rawPublic,
          options.importAlgorithm,
          true,
          options.publicUsages);
      }
      if (options.rawSeed === true) {
        ctx.importedRawSeed = await importCryptoKey(
          `${options.name} private`,
          'raw-seed',
          ctx.rawSeed,
          options.importAlgorithm,
          true,
          options.privateUsages);
      }
      ctx.importedPublicJwk = await importCryptoKey(
        `${options.name} public`,
        'jwk',
        ctx.publicJwk,
        options.importAlgorithm,
        true,
        options.publicUsages);
      ctx.importedPrivateJwk = await importCryptoKey(
        `${options.name} private`,
        'jwk',
        ctx.privateJwk,
        options.importAlgorithm,
        true,
        options.privateUsages);
    },
  };

  if (options.signAlgorithm !== undefined) {
    fixture.sign = async (ctx) => {
      ctx.signature = await assertNoPromiseConstructorAccess(`sign ${options.name}`, () =>
        subtle.sign(options.signAlgorithm, ctx.keyPair.privateKey, data));
    };
    fixture.verify = async (ctx) => {
      await assertNoPromiseConstructorAccess(`verify ${options.name}`, () =>
        subtle.verify(options.signAlgorithm, ctx.keyPair.publicKey, ctx.signature, data));
    };
  }

  if (options.encryptAlgorithm !== undefined) {
    fixture.encrypt = async (ctx) => {
      ctx.ciphertext = await assertNoPromiseConstructorAccess(`encrypt ${options.name}`, () =>
        subtle.encrypt(options.encryptAlgorithm, ctx.keyPair.publicKey, data));
    };
    fixture.decrypt = async (ctx) => {
      await assertNoPromiseConstructorAccess(`decrypt ${options.name}`, () =>
        subtle.decrypt(options.encryptAlgorithm, ctx.keyPair.privateKey, ctx.ciphertext));
    };
  }

  if (options.deriveAlgorithm !== undefined) {
    fixture.deriveBits = async (ctx) => {
      ctx.peerKeyPair ??= await subtle.generateKey(
        options.generateAlgorithm,
        true,
        options.usages);
      ctx.derivedBits = await assertNoPromiseConstructorAccess(`deriveBits ${options.name}`, () =>
        subtle.deriveBits(
          options.deriveAlgorithm(ctx.peerKeyPair.publicKey),
          ctx.keyPair.privateKey,
          options.deriveLength));
    };
    fixture.extra = async (ctx) => {
      ctx.peerKeyPair ??= await subtle.generateKey(
        options.generateAlgorithm,
        true,
        options.usages);
      await assertNoInheritedArrayBufferThenAccess(`deriveKey ${options.name}`, () =>
        assertCryptoKeyResult(`deriveKey ${options.name} result`, () =>
          subtle.deriveKey(
            options.deriveAlgorithm(ctx.peerKeyPair.publicKey),
            ctx.keyPair.privateKey,
            algorithm('AES-CBC', { length: 128 }),
            true,
            ['encrypt'])));
    };
  }

  fixture.getPublicKey = async (ctx) => {
    await assertCryptoKeyResult(`getPublicKey ${options.name}`, () =>
      subtle.getPublicKey(ctx.keyPair.privateKey, options.publicUsages));
  };

  return fixture;
}

function kdfFixture(options) {
  return {
    ...options,
    importKey: async (ctx) => {
      ctx.key = await importCryptoKey(
        options.name,
        options.rawFormat,
        new Uint8Array(32).fill(1),
        options.importAlgorithm,
        false,
        ['deriveBits', 'deriveKey']);
    },
    deriveBits: async (ctx) => {
      await assertNoPromiseConstructorAccess(`deriveBits ${options.name}`, () =>
        subtle.deriveBits(options.deriveAlgorithm, ctx.key, 256));
    },
    extra: async (ctx) => {
      await assertNoInheritedArrayBufferThenAccess(`deriveKey ${options.name}`, () =>
        assertCryptoKeyResult(`deriveKey ${options.name} result`, () =>
          subtle.deriveKey(
            options.deriveAlgorithm,
            ctx.key,
            algorithm('AES-CBC', { length: 128 }),
            true,
            ['encrypt'])));
    },
  };
}

function kemFixture(options) {
  const fixture = pairKeyFixture({
    ...options,
    usages: [
      'encapsulateKey',
      'encapsulateBits',
      'decapsulateKey',
      'decapsulateBits',
    ],
    publicUsages: ['encapsulateKey', 'encapsulateBits'],
    privateUsages: ['decapsulateKey', 'decapsulateBits'],
    rawPublic: true,
    rawSeed: true,
  });

  fixture.encapsulate = async (ctx) => {
    ctx.encapsulatedBits = await assertNoPromiseConstructorAccess(
      `encapsulateBits ${options.name}`, () =>
        subtle.encapsulateBits(algorithm(options.name), ctx.keyPair.publicKey));
    ctx.encapsulatedKey = await assertNoRawSharedKeyObjectThenAccess(
      `encapsulateKey ${options.name}`,
      () => assertNoPromiseConstructorAccess(`encapsulateKey ${options.name}`, () =>
        subtle.encapsulateKey(
          algorithm(options.name),
          ctx.keyPair.publicKey,
          'HKDF',
          false,
          ['deriveBits'])));
  };
  fixture.decapsulate = async (ctx) => {
    await assertNoPromiseConstructorAccess(`decapsulateBits ${options.name}`, () =>
      subtle.decapsulateBits(
        algorithm(options.name),
        ctx.keyPair.privateKey,
        ctx.encapsulatedBits.ciphertext));
    await assertNoInheritedArrayBufferThenAccess(`decapsulateKey ${options.name}`, () =>
      assertCryptoKeyResult(`decapsulateKey ${options.name} result`, () =>
        subtle.decapsulateKey(
          algorithm(options.name),
          ctx.keyPair.privateKey,
          ctx.encapsulatedKey.ciphertext,
          'HKDF',
          false,
          ['deriveBits'])));
  };

  return fixture;
}

// The fixture registry mirrors kSupportedAlgorithms by algorithm name. Each
// fixture supplies the public SubtleCrypto calls needed to exercise the
// registered operations for that algorithm.
const fixtures = new Map();

function addFixture(name, fixture) {
  fixtures.set(name, fixture);
}

for (const name of ['AES-CBC', 'AES-CTR', 'AES-GCM', 'AES-OCB']) {
  const encryptAlgorithms = {
    'AES-CBC': algorithm(name, { iv: new Uint8Array(16) }),
    'AES-CTR': algorithm(name, { counter: new Uint8Array(16), length: 64 }),
    'AES-GCM': algorithm(name, {
      iv: new Uint8Array(12),
      additionalData: new Uint8Array(1),
      tagLength: 128,
    }),
    'AES-OCB': algorithm(name, {
      iv: new Uint8Array(15),
      additionalData: new Uint8Array(1),
      tagLength: 128,
    }),
  };
  addFixture(name, secretKeyFixture({
    name,
    generateAlgorithm: algorithm(name, { length: 128 }),
    importAlgorithm: algorithm(name),
    usages: ['encrypt', 'decrypt'],
    rawFormat: 'raw-secret',
    encryptAlgorithm: encryptAlgorithms[name],
  }));
}

addFixture('AES-KW', secretKeyFixture({
  name: 'AES-KW',
  generateAlgorithm: algorithm('AES-KW', { length: 128 }),
  importAlgorithm: algorithm('AES-KW'),
  usages: ['wrapKey', 'unwrapKey'],
  rawFormat: 'raw-secret',
  wrapAlgorithm: 'AES-KW',
}));

addFixture('ChaCha20-Poly1305', secretKeyFixture({
  name: 'ChaCha20-Poly1305',
  generateAlgorithm: algorithm('ChaCha20-Poly1305'),
  importAlgorithm: algorithm('ChaCha20-Poly1305'),
  usages: ['encrypt', 'decrypt'],
  rawFormat: 'raw-secret',
  encryptAlgorithm: algorithm('ChaCha20-Poly1305', {
    iv: new Uint8Array(12),
    additionalData: new Uint8Array(1),
    tagLength: 128,
  }),
}));

addFixture('HMAC', secretKeyFixture({
  name: 'HMAC',
  generateAlgorithm: algorithm('HMAC', { hash: 'SHA-256', length: 256 }),
  importAlgorithm: algorithm('HMAC', { hash: 'SHA-256' }),
  usages: ['sign', 'verify'],
  rawFormat: 'raw-secret',
  signAlgorithm: 'HMAC',
}));

for (const name of ['KMAC128', 'KMAC256']) {
  addFixture(name, secretKeyFixture({
    name,
    generateAlgorithm: algorithm(name, {
      length: name === 'KMAC128' ? 128 : 256,
    }),
    importAlgorithm: algorithm(name),
    usages: ['sign', 'verify'],
    rawFormat: 'raw-secret',
    signAlgorithm: algorithm(name, { outputLength: 256 }),
  }));
}

for (const name of ['RSASSA-PKCS1-v1_5', 'RSA-PSS']) {
  addFixture(name, pairKeyFixture({
    name,
    generateAlgorithm: rsaAlgorithm(name),
    importAlgorithm: importRsaAlgorithm(name),
    usages: ['sign', 'verify'],
    publicUsages: ['verify'],
    privateUsages: ['sign'],
    signAlgorithm: name === 'RSA-PSS' ?
      algorithm(name, { saltLength: 32 }) :
      algorithm(name),
  }));
}

addFixture('RSA-OAEP', pairKeyFixture({
  name: 'RSA-OAEP',
  generateAlgorithm: rsaAlgorithm('RSA-OAEP'),
  importAlgorithm: importRsaAlgorithm('RSA-OAEP'),
  usages: ['encrypt', 'decrypt'],
  publicUsages: ['encrypt'],
  privateUsages: ['decrypt'],
  encryptAlgorithm: algorithm('RSA-OAEP', { label: new Uint8Array(1) }),
}));

addFixture('ECDSA', pairKeyFixture({
  name: 'ECDSA',
  generateAlgorithm: algorithm('ECDSA', { namedCurve: 'P-256' }),
  importAlgorithm: algorithm('ECDSA', { namedCurve: 'P-256' }),
  usages: ['sign', 'verify'],
  publicUsages: ['verify'],
  privateUsages: ['sign'],
  rawPublic: true,
  signAlgorithm: algorithm('ECDSA', { hash: 'SHA-256' }),
}));

addFixture('ECDH', pairKeyFixture({
  name: 'ECDH',
  generateAlgorithm: algorithm('ECDH', { namedCurve: 'P-256' }),
  importAlgorithm: algorithm('ECDH', { namedCurve: 'P-256' }),
  usages: ['deriveBits', 'deriveKey'],
  publicUsages: [],
  privateUsages: ['deriveBits', 'deriveKey'],
  rawPublic: true,
  deriveAlgorithm: (publicKey) => algorithm('ECDH', { public: publicKey }),
  deriveLength: 256,
}));

for (const name of ['Ed25519', 'Ed448']) {
  addFixture(name, pairKeyFixture({
    name,
    generateAlgorithm: algorithm(name),
    importAlgorithm: algorithm(name),
    usages: ['sign', 'verify'],
    publicUsages: ['verify'],
    privateUsages: ['sign'],
    rawPublic: true,
    signAlgorithm: algorithm(name),
  }));
}

for (const name of ['X25519', 'X448']) {
  addFixture(name, pairKeyFixture({
    name,
    generateAlgorithm: algorithm(name),
    importAlgorithm: algorithm(name),
    usages: ['deriveBits', 'deriveKey'],
    publicUsages: [],
    privateUsages: ['deriveBits', 'deriveKey'],
    rawPublic: true,
    deriveAlgorithm: (publicKey) => algorithm(name, { public: publicKey }),
    deriveLength: name === 'X25519' ? 256 : 448,
  }));
}

for (const name of ['ML-DSA-44', 'ML-DSA-65', 'ML-DSA-87']) {
  addFixture(name, pairKeyFixture({
    name,
    generateAlgorithm: algorithm(name),
    importAlgorithm: algorithm(name),
    usages: ['sign', 'verify'],
    publicUsages: ['verify'],
    privateUsages: ['sign'],
    rawPublic: true,
    rawSeed: true,
    signAlgorithm: algorithm(name),
  }));
}

for (const name of [
  'ML-KEM-512',
  'ML-KEM-768',
  'ML-KEM-1024',
]) {
  addFixture(name, kemFixture({
    name,
    generateAlgorithm: algorithm(name),
    importAlgorithm: algorithm(name),
  }));
}

for (const name of ['HKDF', 'PBKDF2']) {
  addFixture(name, kdfFixture({
    name,
    importAlgorithm: name,
    rawFormat: 'raw-secret',
    deriveAlgorithm: name === 'HKDF' ?
      algorithm(name, {
        hash: 'SHA-256',
        salt: new Uint8Array(8),
        info: new Uint8Array(8),
      }) :
      algorithm(name, {
        hash: 'SHA-256',
        salt: new Uint8Array(8),
        iterations: 1,
      }),
  }));
}

for (const name of ['Argon2d', 'Argon2i', 'Argon2id']) {
  addFixture(name, kdfFixture({
    name,
    importAlgorithm: name,
    rawFormat: 'raw-secret',
    deriveAlgorithm: algorithm(name, {
      memory: 32,
      passes: 1,
      parallelism: 1,
      nonce: new Uint8Array(16),
    }),
  }));
}

function digestAlgorithm(name) {
  if (name === 'cSHAKE128')
    return algorithm(name, { outputLength: 256 });
  if (name === 'cSHAKE256')
    return algorithm(name, { outputLength: 512 });
  if (name === 'TurboSHAKE128')
    return algorithm(name, { outputLength: 256 });
  if (name === 'TurboSHAKE256')
    return algorithm(name, { outputLength: 512 });
  if (name === 'KT128')
    return algorithm(name, { outputLength: 256 });
  if (name === 'KT256')
    return algorithm(name, { outputLength: 512 });
  return name;
}

for (const name of [
  'SHA-1',
  'SHA-256',
  'SHA-384',
  'SHA-512',
  'SHA3-256',
  'SHA3-384',
  'SHA3-512',
  'cSHAKE128',
  'cSHAKE256',
  'TurboSHAKE128',
  'TurboSHAKE256',
  'KT128',
  'KT256',
]) {
  addFixture(name, {
    name,
    digest: async () => {
      await assertNoPromiseConstructorAccess(`digest ${name}`, () =>
        subtle.digest(digestAlgorithm(name), data));
    },
  });
}

// deriveKey() is the only public API that performs the "get key length"
// registry operation. Keep this table in sync with algorithms that can be a
// concrete derived-key target.
const keyLengthTargets = {
  'AES-CBC': {
    algorithm: algorithm('AES-CBC', { length: 128 }),
    usages: ['encrypt'],
  },
  'AES-CTR': {
    algorithm: algorithm('AES-CTR', { length: 128 }),
    usages: ['encrypt'],
  },
  'AES-GCM': {
    algorithm: algorithm('AES-GCM', { length: 128 }),
    usages: ['encrypt'],
  },
  'AES-KW': {
    algorithm: algorithm('AES-KW', { length: 128 }),
    usages: ['wrapKey'],
  },
  'AES-OCB': {
    algorithm: algorithm('AES-OCB', { length: 128 }),
    usages: ['encrypt'],
  },
  'ChaCha20-Poly1305': {
    algorithm: algorithm('ChaCha20-Poly1305'),
    usages: ['encrypt'],
  },
  'HMAC': {
    algorithm: algorithm('HMAC', { hash: 'SHA-256', length: 256 }),
    usages: ['sign'],
  },
  'KMAC128': {
    algorithm: algorithm('KMAC128', { length: 128 }),
    usages: ['sign'],
  },
  'KMAC256': {
    algorithm: algorithm('KMAC256', { length: 256 }),
    usages: ['sign'],
  },
};

function getSupportedAlgorithmOperations() {
  const algorithms = new Map();
  for (const operation of Object.keys(kSupportedAlgorithms)) {
    if (operation === 'get key length')
      continue;
    for (const name of Object.keys(kSupportedAlgorithms[operation])) {
      if (!algorithms.has(name))
        algorithms.set(name, new Set());
      algorithms.get(name).add(operation);
    }
  }
  return algorithms;
}

// This is the list of supported public registry operations that this file
// exercises. A new operation name must be added here before the registry
// assertion below will pass.
const operationOrder = [
  'digest',
  'generateKey',
  'exportKey',
  'importKey',
  'encrypt',
  'decrypt',
  'sign',
  'verify',
  'deriveBits',
  'encapsulate',
  'decapsulate',
  'wrapKey',
  'unwrapKey',
];

const coveredOperations = new Set([
  ...operationOrder,
  'get key length',
]);

for (const operation of Object.keys(kSupportedAlgorithms)) {
  assert(
    coveredOperations.has(operation),
    `missing prototype pollution operation coverage for ${operation}`);
}

const supportedAlgorithms = getSupportedAlgorithmOperations();
for (const [name, operations] of supportedAlgorithms) {
  const fixture = fixtures.get(name);
  assert(fixture, `missing prototype pollution fixture for ${name}`);

  const ctx = { __proto__: null };
  for (const operation of operationOrder) {
    if (!operations.has(operation))
      continue;
    assert.strictEqual(
      typeof fixture[operation],
      'function',
      `missing prototype pollution coverage for ${name} ${operation}`);
    await fixture[operation](ctx);
  }

  if (typeof fixture.getPublicKey === 'function' &&
      ctx.keyPair?.privateKey !== undefined) {
    await fixture.getPublicKey(ctx);
  }
  if (typeof fixture.extra === 'function')
    await fixture.extra(ctx);
}

const getKeyLengthAlgorithms =
  Object.keys(kSupportedAlgorithms['get key length'] ?? {});
// KDF base algorithms return null from getKeyLength(). They still need to be
// listed explicitly so a newly registered get-key-length algorithm does not
// silently skip prototype-pollution coverage.
const nullKeyLengthAlgorithms = [
  'HKDF',
  'PBKDF2',
  'Argon2d',
  'Argon2i',
  'Argon2id',
];
const pbkdf2Key = await subtle.importKey(
  'raw-secret',
  new Uint8Array(32).fill(1),
  'PBKDF2',
  false,
  ['deriveKey']);
for (const name of getKeyLengthAlgorithms) {
  const target = keyLengthTargets[name];
  if (target === undefined) {
    assert(
      nullKeyLengthAlgorithms.includes(name),
      `missing get key length coverage for ${name}`);
    continue;
  }

  await assertCryptoKeyResult(`get key length ${name}`, () =>
    subtle.deriveKey(
      algorithm('PBKDF2', {
        hash: 'SHA-256',
        salt: new Uint8Array(8),
        iterations: 1,
      }),
      pbkdf2Key,
      target.algorithm,
      true,
      target.usages));
}

// Keep one explicit unwrapKey('jwk') negative case: the parsed object must not
// inherit kty from Object.prototype when the wrapped JSON does not have it.
{
  const jwkUnwrappingKey = await subtle.generateKey(
    algorithm('AES-CBC', { length: 128 }),
    true,
    ['encrypt', 'unwrapKey']);
  const iv = new Uint8Array(16);
  const missingKtyWrappedJwk = await subtle.encrypt(
    algorithm('AES-CBC', { iv }),
    jwkUnwrappingKey,
    new TextEncoder().encode('{"k":"AAAAAAAAAAAAAAAAAAAAAA"}'));

  await assertMissingJwkKtyIgnoresPrototype(() =>
    subtle.unwrapKey(
      'jwk',
      missingKtyWrappedJwk,
      jwkUnwrappingKey,
      algorithm('AES-CBC', { iv }),
      algorithm('AES-CBC', { length: 128 }),
      true,
      ['encrypt']));
}

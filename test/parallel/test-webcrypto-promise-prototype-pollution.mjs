import * as common from '../common/index.mjs';
import assert from 'node:assert';

if (!common.hasCrypto) common.skip('missing crypto');

// WebCrypto subtle methods must not leak intermediate values
// through Promise.prototype.then or constructor pollution.
// Regression test for https://github.com/nodejs/node/pull/61492
// and https://github.com/nodejs/node/issues/59699.

import { hasOpenSSL } from '../common/crypto.js';

const { subtle } = globalThis.crypto;

Promise.prototype.then = common.mustNotCall('Promise.prototype.then');

// WebCrypto methods return native promises. Re-wrapping a promise with
// PromiseResolve() or chaining it with Promise.prototype.then can read
// user-mutated constructor/species accessors.
async function assertNoPromiseConstructorAccess(name, fn) {
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
  return await promise;
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
// own data properties, not writes that can observe inherited accessors.
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
  try {
    return await fn();
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

await assertNoPromiseConstructorAccess('digest', () =>
  subtle.digest('SHA-256', new Uint8Array([1, 2, 3])));

const secretKey = await assertNoPromiseConstructorAccess(
  'generateKey secret',
  () => subtle.generateKey(
    { name: 'AES-CBC', length: 256 },
    true,
    ['encrypt', 'decrypt']));

const extractableKeyPair = await assertNoPromiseConstructorAccess('generateKey pair', () =>
  subtle.generateKey(
    { name: 'ECDSA', namedCurve: 'P-256' },
    true,
    ['sign', 'verify']));

const rawKey = globalThis.crypto.getRandomValues(new Uint8Array(32));

const importedKey = await assertNoPromiseConstructorAccess('importKey', () =>
  subtle.importKey(
    'raw',
    rawKey,
    { name: 'AES-CBC', length: 256 },
    false,
    ['encrypt', 'decrypt']));

await assertNoInheritedCryptoKeyThenAccess('importKey', () =>
  subtle.importKey(
    'raw',
    rawKey,
    { name: 'AES-CBC', length: 256 },
    false,
    ['encrypt', 'decrypt']));

await assertNoInheritedJwkPropertyAccess('exportKey jwk secret', () =>
  assertExportKeyNoPromiseConstructorAccess(
    'jwk secret',
    'jwk',
    secretKey));
await assertNoInheritedObjectThenAccess('exportKey jwk secret', () =>
  subtle.exportKey('jwk', secretKey));
await assertNoInheritedJwkPropertyAccess('exportKey jwk public', () =>
  assertExportKeyNoPromiseConstructorAccess(
    'jwk public',
    'jwk',
    extractableKeyPair.publicKey));
await assertNoInheritedObjectThenAccess('exportKey jwk public', () =>
  subtle.exportKey('jwk', extractableKeyPair.publicKey));
await assertNoInheritedJwkPropertyAccess('exportKey jwk private', () =>
  assertExportKeyNoPromiseConstructorAccess(
    'jwk private',
    'jwk',
    extractableKeyPair.privateKey));
await assertNoInheritedObjectThenAccess('exportKey jwk private', () =>
  subtle.exportKey('jwk', extractableKeyPair.privateKey));
await assertNoInheritedArrayBufferThenAccess('exportKey raw secret', () =>
  subtle.exportKey('raw', secretKey));
await assertExportKeyNoPromiseConstructorAccess(
  'raw secret',
  'raw',
  secretKey);
await assertNoInheritedArrayBufferThenAccess('exportKey spki', () =>
  subtle.exportKey('spki', extractableKeyPair.publicKey));
await assertExportKeyNoPromiseConstructorAccess(
  'spki',
  'spki',
  extractableKeyPair.publicKey);
await assertNoInheritedArrayBufferThenAccess('exportKey pkcs8', () =>
  subtle.exportKey('pkcs8', extractableKeyPair.privateKey));
await assertExportKeyNoPromiseConstructorAccess(
  'pkcs8',
  'pkcs8',
  extractableKeyPair.privateKey);
await assertNoInheritedArrayBufferThenAccess('exportKey raw-public', () =>
  subtle.exportKey('raw-public', extractableKeyPair.publicKey));
await assertExportKeyNoPromiseConstructorAccess(
  'raw-public',
  'raw-public',
  extractableKeyPair.publicKey);

const iv = globalThis.crypto.getRandomValues(new Uint8Array(16));
const plaintext = new TextEncoder().encode('Hello, world!');

const ciphertext = await assertNoPromiseConstructorAccess('encrypt', () =>
  subtle.encrypt({ name: 'AES-CBC', iv }, importedKey, plaintext));

await assertNoPromiseConstructorAccess('decrypt', () =>
  subtle.decrypt({ name: 'AES-CBC', iv }, importedKey, ciphertext));

const signingKey = await subtle.generateKey(
  { name: 'HMAC', hash: 'SHA-256' },
  false,
  ['sign', 'verify']);

const data = new TextEncoder().encode('test data');

const signature = await assertNoPromiseConstructorAccess('sign', () =>
  subtle.sign('HMAC', signingKey, data));

await assertNoPromiseConstructorAccess('verify', () =>
  subtle.verify('HMAC', signingKey, signature, data));

const pbkdf2Key = await subtle.importKey(
  'raw', rawKey, 'PBKDF2', false, ['deriveBits', 'deriveKey']);

await assertNoPromiseConstructorAccess('deriveBits', () =>
  subtle.deriveBits(
    { name: 'PBKDF2', salt: rawKey, iterations: 1000, hash: 'SHA-256' },
    pbkdf2Key,
    256));

await assertNoPromiseConstructorAccess('deriveBits PBKDF2 zero-length', () =>
  subtle.deriveBits(
    { name: 'PBKDF2', salt: rawKey, iterations: 1000, hash: 'SHA-256' },
    pbkdf2Key,
    0));

const hkdfKey = await subtle.importKey(
  'raw', rawKey, 'HKDF', false, ['deriveBits']);

await assertNoPromiseConstructorAccess('deriveBits HKDF zero-length', () =>
  subtle.deriveBits(
    { name: 'HKDF', hash: 'SHA-256', salt: rawKey, info: rawKey },
    hkdfKey,
    0));

const ecdhKeyPair = await subtle.generateKey(
  { name: 'ECDH', namedCurve: 'P-256' },
  false,
  ['deriveBits']);

await assertNoPromiseConstructorAccess('deriveBits ECDH', () =>
  subtle.deriveBits(
    { name: 'ECDH', public: ecdhKeyPair.publicKey },
    ecdhKeyPair.privateKey,
    256));

await assertNoPromiseConstructorAccess('deriveKey', () =>
  subtle.deriveKey(
    { name: 'PBKDF2', salt: rawKey, iterations: 1000, hash: 'SHA-256' },
    pbkdf2Key,
    { name: 'AES-CBC', length: 256 },
    true,
    ['encrypt', 'decrypt']));
await assertNoInheritedArrayBufferThenAccess('deriveKey', () =>
  subtle.deriveKey(
    { name: 'PBKDF2', salt: rawKey, iterations: 1000, hash: 'SHA-256' },
    pbkdf2Key,
    { name: 'AES-CBC', length: 256 },
    true,
    ['encrypt', 'decrypt']));
await assertNoInheritedCryptoKeyThenAccess('deriveKey result', () =>
  subtle.deriveKey(
    { name: 'PBKDF2', salt: rawKey, iterations: 1000, hash: 'SHA-256' },
    pbkdf2Key,
    { name: 'AES-CBC', length: 256 },
    true,
    ['encrypt', 'decrypt']));

const wrappingKey = await subtle.generateKey(
  { name: 'AES-KW', length: 256 }, true, ['wrapKey', 'unwrapKey']);

const keyToWrap = await subtle.generateKey(
  { name: 'AES-CBC', length: 256 }, true, ['encrypt', 'decrypt']);

const wrapped = await assertNoPromiseConstructorAccess('wrapKey', () =>
  subtle.wrapKey('raw', keyToWrap, wrappingKey, 'AES-KW'));

const wrappedJwk = await assertNoInheritedJwkPropertyAccess('wrapKey jwk', () =>
  assertNoInheritedToJSONAccess('wrapKey jwk', () =>
    assertNoUserMutableEncodeAccess('wrapKey jwk', () =>
      assertNoPromiseConstructorAccess('wrapKey jwk', () =>
        subtle.wrapKey('jwk', keyToWrap, wrappingKey, 'AES-KW')))));

await assertNoPromiseConstructorAccess('unwrapKey', () =>
  subtle.unwrapKey(
    'raw',
    wrapped,
    wrappingKey,
    'AES-KW',
    { name: 'AES-CBC', length: 256 },
    true,
    ['encrypt', 'decrypt']));
await assertNoInheritedArrayBufferThenAccess('unwrapKey', () =>
  subtle.unwrapKey(
    'raw',
    wrapped,
    wrappingKey,
    'AES-KW',
    { name: 'AES-CBC', length: 256 },
    true,
    ['encrypt', 'decrypt']));
await assertNoInheritedCryptoKeyThenAccess('unwrapKey result', () =>
  subtle.unwrapKey(
    'raw',
    wrapped,
    wrappingKey,
    'AES-KW',
    { name: 'AES-CBC', length: 256 },
    true,
    ['encrypt', 'decrypt']));

await assertNoUserMutableDecodeAccess('unwrapKey jwk', () =>
  assertNoPromiseConstructorAccess('unwrapKey jwk', () =>
    subtle.unwrapKey(
      'jwk',
      wrappedJwk,
      wrappingKey,
      'AES-KW',
      { name: 'AES-CBC', length: 256 },
      true,
      ['encrypt', 'decrypt'])));
await assertNoInheritedArrayBufferThenAccess('unwrapKey jwk', () =>
  subtle.unwrapKey(
    'jwk',
    wrappedJwk,
    wrappingKey,
    'AES-KW',
    { name: 'AES-CBC', length: 256 },
    true,
    ['encrypt', 'decrypt']));
await assertNoInheritedCryptoKeyThenAccess('unwrapKey jwk result', () =>
  subtle.unwrapKey(
    'jwk',
    wrappedJwk,
    wrappingKey,
    'AES-KW',
    { name: 'AES-CBC', length: 256 },
    true,
    ['encrypt', 'decrypt']));

{
  const jwkUnwrappingKey = await subtle.generateKey(
    { name: 'AES-CBC', length: 128 },
    true,
    ['encrypt', 'unwrapKey']);

  {
    const iv = globalThis.crypto.getRandomValues(new Uint8Array(16));
    const validWrappedJwk = await subtle.encrypt(
      { name: 'AES-CBC', iv },
      jwkUnwrappingKey,
      Buffer.from('{"kty":"oct","k":"AAAAAAAAAAAAAAAAAAAAAA"}'));

    await assertNoUserMutableDecodeAccess('unwrapKey jwk AES-CBC', () =>
      assertNoPromiseConstructorAccess('unwrapKey jwk AES-CBC', () =>
        subtle.unwrapKey(
          'jwk',
          validWrappedJwk,
          jwkUnwrappingKey,
          { name: 'AES-CBC', iv },
          { name: 'AES-CBC', length: 128 },
          true,
          ['encrypt'])));
    await assertNoInheritedCryptoKeyThenAccess(
      'unwrapKey jwk AES-CBC result',
      () => subtle.unwrapKey(
        'jwk',
        validWrappedJwk,
        jwkUnwrappingKey,
        { name: 'AES-CBC', iv },
        { name: 'AES-CBC', length: 128 },
        true,
        ['encrypt']));
  }

  {
    const iv = globalThis.crypto.getRandomValues(new Uint8Array(16));
    const wrappedRawKey = await subtle.encrypt(
      { name: 'AES-CBC', iv },
      jwkUnwrappingKey,
      rawKey);

    await assertNoPromiseConstructorAccess('unwrapKey raw AES-CBC', () =>
      subtle.unwrapKey(
        'raw',
        wrappedRawKey,
        jwkUnwrappingKey,
        { name: 'AES-CBC', iv },
        { name: 'AES-CBC', length: 256 },
        true,
        ['encrypt']));
    await assertNoInheritedArrayBufferThenAccess('unwrapKey raw AES-CBC', () =>
      subtle.unwrapKey(
        'raw',
        wrappedRawKey,
        jwkUnwrappingKey,
        { name: 'AES-CBC', iv },
        { name: 'AES-CBC', length: 256 },
        true,
        ['encrypt']));
    await assertNoInheritedCryptoKeyThenAccess(
      'unwrapKey raw AES-CBC result',
      () => subtle.unwrapKey(
        'raw',
        wrappedRawKey,
        jwkUnwrappingKey,
        { name: 'AES-CBC', iv },
        { name: 'AES-CBC', length: 256 },
        true,
        ['encrypt']));
  }

  {
    const iv = globalThis.crypto.getRandomValues(new Uint8Array(16));
    const missingKtyWrappedJwk = await subtle.encrypt(
      { name: 'AES-CBC', iv },
      jwkUnwrappingKey,
      Buffer.from('{"k":"AAAAAAAAAAAAAAAAAAAAAA"}'));

    await assertMissingJwkKtyIgnoresPrototype(() =>
      subtle.unwrapKey(
        'jwk',
        missingKtyWrappedJwk,
        jwkUnwrappingKey,
        { name: 'AES-CBC', iv },
        { name: 'AES-CBC', length: 128 },
        true,
        ['encrypt']));
  }
}

await assertNoPromiseConstructorAccess('getPublicKey', () =>
  subtle.getPublicKey(extractableKeyPair.privateKey, ['verify']));
await assertNoInheritedCryptoKeyThenAccess('getPublicKey', () =>
  subtle.getPublicKey(extractableKeyPair.privateKey, ['verify']));

if (hasOpenSSL(3, 5) || process.features.openssl_is_boringssl) {
  const kemPair = await subtle.generateKey(
    { name: 'ML-KEM-768' }, true,
    ['encapsulateKey', 'encapsulateBits', 'decapsulateKey', 'decapsulateBits']);

  await assertNoInheritedArrayBufferThenAccess('exportKey raw-seed', () =>
    subtle.exportKey('raw-seed', kemPair.privateKey));
  await assertExportKeyNoPromiseConstructorAccess(
    'raw-seed',
    'raw-seed',
    kemPair.privateKey);

  const { ciphertext: ct1 } =
    await assertNoRawSharedKeyObjectThenAccess('encapsulateKey', () =>
      assertNoPromiseConstructorAccess('encapsulateKey', () =>
        subtle.encapsulateKey(
          { name: 'ML-KEM-768' },
          kemPair.publicKey,
          'HKDF',
          false,
          ['deriveBits'])));

  await assertNoPromiseConstructorAccess('decapsulateKey', () =>
    subtle.decapsulateKey(
      { name: 'ML-KEM-768' },
      kemPair.privateKey,
      ct1,
      'HKDF',
      false,
      ['deriveBits']));
  await assertNoInheritedArrayBufferThenAccess('decapsulateKey', () =>
    subtle.decapsulateKey(
      { name: 'ML-KEM-768' },
      kemPair.privateKey,
      ct1,
      'HKDF',
      false,
      ['deriveBits']));
  await assertNoInheritedCryptoKeyThenAccess('decapsulateKey result', () =>
    subtle.decapsulateKey(
      { name: 'ML-KEM-768' },
      kemPair.privateKey,
      ct1,
      'HKDF',
      false,
      ['deriveBits']));

  const { ciphertext: ct2 } =
    await assertNoPromiseConstructorAccess('encapsulateBits', () =>
      subtle.encapsulateBits(
        { name: 'ML-KEM-768' },
        kemPair.publicKey));

  await assertNoPromiseConstructorAccess('decapsulateBits', () =>
    subtle.decapsulateBits(
      { name: 'ML-KEM-768' },
      kemPair.privateKey,
      ct2));
}

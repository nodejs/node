// Flags: --expose-internals

import * as common from '../common/index.mjs';
import assert from 'node:assert';
import { createRequire } from 'node:module';

if (!common.hasCrypto) common.skip('missing crypto');

// Regression tests for prototype pollution reaching WebCrypto input validation
// and normalization, via BufferSource prototype accessors, inherited
// %Object.prototype% keys, or %Array.prototype%[%Symbol.iterator%]. See
// test-webcrypto-promise-prototype-pollution.mjs for the promise side.

const require = createRequire(import.meta.url);
const { kSupportedAlgorithms } = require('internal/crypto/util');
const { getFips } = require('node:crypto');
const { hasOpenSSL } = require('../common/crypto');
const { subtle } = globalThis.crypto;

const TypedArrayPrototype = Object.getPrototypeOf(Uint8Array.prototype);
const data = new TextEncoder().encode('prototype pollution');
const modulusLength = getFips() === 1 ? 2048 : 1024;

// Avoids SubtleCrypto.supports(), which warns and invokes the registry's
// experimental-algorithm getters.
function supports(operation, name) {
  return Object.hasOwn(kSupportedAlgorithms[operation] ?? {}, name);
}

// Each poison is { target, key, ...descriptor }.
async function withPoisoned(poisons, fn) {
  const saved = [];
  for (const { target, key, ...descriptor } of poisons) {
    saved.push([target, key, Object.getOwnPropertyDescriptor(target, key)]);
    Object.defineProperty(target, key, {
      __proto__: null,
      configurable: true,
      ...descriptor,
    });
  }
  try {
    return await fn();
  } finally {
    for (let i = saved.length - 1; i >= 0; i--) {
      const { 0: target, 1: key, 2: descriptor } = saved[i];
      if (descriptor === undefined) {
        delete target[key];
      } else {
        Object.defineProperty(target, key, descriptor);
      }
    }
  }
}

function poisonTypedArrayByteLength(value) {
  return [{ target: TypedArrayPrototype, key: 'byteLength', get: () => value }];
}

function inherited(key, value) {
  return [{ target: Object.prototype, key, value, writable: true }];
}

const poisonArrayIterator = [{
  target: Array.prototype,
  key: Symbol.iterator,
  value: () => ({ next: () => ({ done: true, value: undefined }) }),
  writable: true,
}];

// A poisoned array iterator breaks assert too, so settle under the poison and
// assert once it has been restored.
async function settleUnderPoison(poisons, fn) {
  const outcome = { __proto__: null, value: undefined, error: undefined };
  await withPoisoned(poisons, async () => {
    try {
      outcome.value = await fn();
    } catch (err) {
      outcome.error = err;
    }
  });
  return outcome;
}

// validateByteLength(). Unguarded, the empty iv reaches OpenSSL, which also
// fails with OperationError, hence the message assertion.
{
  const key = await subtle.importKey(
    'raw-secret', new Uint8Array(16), 'AES-CBC', false, ['encrypt']);
  await withPoisoned(poisonTypedArrayByteLength(16), common.mustCall(() =>
    assert.rejects(
      subtle.encrypt({ name: 'AES-CBC', iv: new Uint8Array(0) }, key, data),
      {
        name: 'OperationError',
        message: /algorithm\.iv must contain exactly 16 bytes/,
      })));
}

// validateMaxBufferLength().
{
  const key = await subtle.importKey(
    'raw-secret', new Uint8Array(32), 'HKDF', false, ['deriveBits']);
  await withPoisoned(poisonTypedArrayByteLength(0), common.mustCall(() =>
    assert.rejects(
      subtle.deriveBits({
        name: 'HKDF',
        hash: 'SHA-256',
        salt: new Uint8Array(0),
        info: new Uint8Array(4096),
      }, key, 8),
      {
        name: 'OperationError',
        message: /algorithm\.info must be at most 1024 bytes/,
      })));
}

// aesImportKey().
await withPoisoned(poisonTypedArrayByteLength(16), common.mustCall(async () => {
  const key = await subtle.importKey(
    'raw-secret', new Uint8Array(32), 'AES-GCM', true, ['encrypt']);
  assert.strictEqual(key.algorithm.length, 256);
}));

// validateCShakeFunctionName().
if (supports('digest', 'cSHAKE128')) {
  await withPoisoned(poisonTypedArrayByteLength(0), common.mustCall(() =>
    assert.rejects(
      subtle.digest({
        name: 'cSHAKE128',
        outputLength: 256,
        functionName: new Uint8Array([0x41, 0x42, 0x43, 0x44]),
      }, data),
      {
        name: 'NotSupportedError',
        message: /Unsupported CShakeParams functionName/,
      })));

  // asyncDigest() picks the cSHAKE job over plain SHAKE on a non-empty
  // customization.
  if (hasOpenSSL(3)) {
    const algorithm = {
      name: 'cSHAKE128',
      outputLength: 256,
      customization: new Uint8Array([1, 2, 3]),
    };
    if (getFips() === 1) {
      await withPoisoned(poisonTypedArrayByteLength(0), common.mustCall(() =>
        assert.rejects(subtle.digest(algorithm, data), {
          name: 'NotSupportedError',
          message: 'Unsupported CShakeParams customization',
        })));
    } else {
      const expected = new Uint8Array(await subtle.digest(algorithm, data));
      const plain = new Uint8Array(
        await subtle.digest({ name: 'cSHAKE128', outputLength: 256 }, data));
      assert.notDeepStrictEqual(expected, plain);
      await withPoisoned(poisonTypedArrayByteLength(0),
                         common.mustCall(async () => {
                           assert.deepStrictEqual(
                             new Uint8Array(await subtle.digest(algorithm, data)),
                             expected);
                         }));
    }
  }
}

// AeadParams: AES-OCB caps the iv at 15 bytes.
if (supports('encrypt', 'AES-OCB')) {
  const key = await subtle.importKey(
    'raw-secret', new Uint8Array(16), 'AES-OCB', false, ['encrypt']);
  await withPoisoned(poisonTypedArrayByteLength(12), common.mustCall(() =>
    assert.rejects(
      subtle.encrypt({ name: 'AES-OCB', iv: new Uint8Array(20) }, key, data),
      {
        name: 'OperationError',
        message: /algorithm\.iv must be no more than 15 bytes/,
      })));
}

// Argon2Params: the nonce has an 8 byte minimum.
if (supports('deriveBits', 'Argon2id')) {
  const key = await subtle.importKey(
    'raw-secret', new Uint8Array(32), 'Argon2id', false, ['deriveBits']);
  await withPoisoned(poisonTypedArrayByteLength(16), common.mustCall(() =>
    assert.rejects(
      subtle.deriveBits({
        name: 'Argon2id',
        nonce: new Uint8Array(4),
        memory: 32,
        passes: 1,
        parallelism: 1,
      }, key, 256),
      {
        name: 'OperationError',
        message: /nonce must be at least 8 bytes/,
      })));
}

// bigIntArrayToUnsignedInt(): TypedArray `length` is a prototype accessor.
await withPoisoned(
  [{ target: TypedArrayPrototype, key: 'length', get: () => 0 }],
  common.mustCall(async () => {
    const { publicKey } = await subtle.generateKey({
      name: 'RSA-OAEP',
      modulusLength,
      publicExponent: new Uint8Array([1, 0, 1]),
      hash: 'SHA-256',
    }, true, ['encrypt', 'decrypt']);
    assert.strictEqual(publicKey.algorithm.modulusLength, modulusLength);
    assert.deepStrictEqual(
      publicKey.algorithm.publicExponent, new Uint8Array([1, 0, 1]));
  }));

// ecdhDeriveBits() bounds the request by the native job's ArrayBuffer.
{
  const { privateKey, publicKey } = await subtle.generateKey(
    { name: 'ECDH', namedCurve: 'P-256' }, false, ['deriveBits']);
  await withPoisoned(
    [{ target: ArrayBuffer.prototype, key: 'byteLength', get: () => 1e9 }],
    common.mustCall(() => assert.rejects(
      subtle.deriveBits({ name: 'ECDH', public: publicKey }, privateKey, 8192),
      { name: 'OperationError' })));
}

// simpleAlgorithmDictionaries relies on a miss returning undefined.
{
  const { privateKey, publicKey } = await subtle.generateKey(
    { name: 'ECDH', namedCurve: 'P-256' }, false, ['deriveBits']);
  await withPoisoned(
    inherited('EcdhKeyDeriveParams',
              { keys: ['public'], types: { public: 'BufferSource' } }),
    common.mustCall(async () => {
      const bits = await subtle.deriveBits(
        { name: 'ECDH', public: publicKey }, privateKey, 128);
      assert.strictEqual(bits.byteLength, 16);
    }));

  await withPoisoned(
    inherited('AesKeyGenParams',
              { keys: ['name'], types: { name: 'AlgorithmIdentifier' } }),
    common.mustCall(async () => {
      const key = await subtle.generateKey(
        { name: 'AES-GCM', length: 128 }, false, ['encrypt']);
      assert.strictEqual(key.algorithm.length, 128);
    }));
}

// createDictionaryConverter() reads optional member descriptor keys.
{
  const key = await subtle.generateKey(
    { name: 'AES-GCM', length: 128 }, false, ['encrypt']);
  const encrypt = () => subtle.encrypt(
    { name: 'AES-GCM', iv: new Uint8Array(12) }, key, data);

  for (const poison of [
    inherited('required', true),
    inherited('defaultValue', () => 9999),
    inherited('validator', common.mustNotCall('Object.prototype.validator')),
  ]) {
    await withPoisoned(poison, common.mustCall(async () => {
      assert.strictEqual((await encrypt()).byteLength, data.byteLength + 16);
    }));
  }
}

// Conversion options are read by key by the Web IDL converters.
{
  const key = await subtle.importKey(
    'raw-secret', new Uint8Array(32), 'HKDF', false, ['deriveBits']);
  const hkdf = (length) => subtle.deriveBits({
    name: 'HKDF',
    hash: 'SHA-256',
    salt: new Uint8Array(0),
    info: new Uint8Array(0),
  }, key, length);

  // deriveBits length is [EnforceRange], so 2 ** 32 must throw even when
  // conversion option properties are inherited from Object.prototype.
  for (const attribute of ['enforceRange', 'clamp']) {
    await withPoisoned(
      inherited(attribute, true),
      common.mustCall(() => assert.rejects(hkdf(2 ** 32), {
        code: 'ERR_OUT_OF_RANGE',
        name: 'TypeError',
      })));
  }

  // [AllowResizable] is not set for BufferSource.
  await withPoisoned(inherited('allowResizable', true), common.mustCall(() =>
    subtle.digest('SHA-256', new ArrayBuffer(8, { maxByteLength: 1024 }))
  ));

  // makeException() falls back to ERR_INVALID_ARG_TYPE.
  await withPoisoned(inherited('code', 'ERR_POLLUTED'), common.mustCall(() =>
    assert.rejects(subtle.digest('SHA-256', 'not a BufferSource'),
                   { code: 'ERR_INVALID_ARG_TYPE' })));
}

// enforceRangeOptions(): [EnforceRange] uses IntegerPart, not round-half-even.
{
  const key = await subtle.importKey(
    'raw-secret', new Uint8Array(32), 'PBKDF2', false, ['deriveBits']);
  const pbkdf2 = (iterations) => subtle.deriveBits({
    name: 'PBKDF2',
    hash: 'SHA-256',
    salt: new Uint8Array(16),
    iterations,
  }, key, 112);

  const expected = new Uint8Array(await pbkdf2(1000));
  await withPoisoned(inherited('clamp', true), common.mustCall(async () => {
    assert.deepStrictEqual(new Uint8Array(await pbkdf2(1000.5)), expected);
  }));
}

// keyDetail() is filled in by C++ with an ordinary [[Set]].
{
  const { publicKey } = await subtle.generateKey({
    name: 'RSA-PSS',
    modulusLength,
    publicExponent: new Uint8Array([1, 0, 1]),
    hash: 'SHA-256',
  }, true, ['sign', 'verify']);
  const spki = await subtle.exportKey('spki', publicKey);

  await withPoisoned([
    {
      target: Object.prototype, key: 'modulusLength',
      get: () => 8192, set() {},
    },
    {
      target: Object.prototype, key: 'publicExponent',
      get: () => new Uint8Array([9, 9, 9]), set() {},
    },
  ], common.mustCall(async () => {
    const imported = await subtle.importKey(
      'spki', spki, { name: 'RSA-PSS', hash: 'SHA-256' }, true, ['verify']);
    assert.strictEqual(imported.algorithm.modulusLength, modulusLength);
    assert.deepStrictEqual(
      imported.algorithm.publicExponent, new Uint8Array([1, 0, 1]));
  }));
}

{
  const { publicKey } = await subtle.generateKey(
    { name: 'ECDSA', namedCurve: 'P-384' }, true, ['sign', 'verify']);
  const spki = await subtle.exportKey('spki', publicKey);

  await withPoisoned(
    [{
      target: Object.prototype, key: 'namedCurve',
      get: () => 'prime256v1', set() {},
    }],
    common.mustCall(() => assert.rejects(
      subtle.importKey(
        'spki', spki, { name: 'ECDSA', namedCurve: 'P-256' }, true, ['verify']),
      { name: 'DataError', message: /Named curve mismatch/ })));
}

// Key usages under a poisoned array iterator. Callers pass a Set so the
// spec-mandated sequence conversion still yields the requested usage; only
// WebCrypto's own re-iteration of that array sees the poison.
{
  // Every Set has to be built before the poison is installed, otherwise the
  // Set constructor itself iterates its array argument and comes out empty.
  const signOnly = new Set(['sign']);
  const encryptOnly = new Set(['encrypt']);
  const decryptOnly = new Set(['decrypt']);
  const decapsulateKeyOnly = new Set(['decapsulateKey']);

  // Secret keys reject empty usages anyway, so match the message: the usage
  // has to be rejected as unsupported, not as missing.
  const cases = [
    {
      name: 'AES-GCM',
      message: /Unsupported key usage for AES-GCM key/,
      importKey: () => subtle.importKey(
        'raw-secret', new Uint8Array(32), 'AES-GCM', false, signOnly),
    },
    {
      name: 'HKDF',
      message: /Unsupported key usage for a HKDF key/,
      importKey: () => subtle.importKey(
        'raw-secret', new Uint8Array(32), 'HKDF', false, encryptOnly),
    },
  ];

  const addPublicKeyCase = async (name, algorithm, usages, disallowed) => {
    if (!supports('importKey', name)) return;
    const { publicKey } = await subtle.generateKey(algorithm, true, usages);
    const spki = await subtle.exportKey('spki', publicKey);
    cases.push({
      name,
      importKey: () => subtle.importKey(
        'spki', spki, algorithm, true, disallowed),
    });
  };

  await addPublicKeyCase('ECDSA', { name: 'ECDSA', namedCurve: 'P-256' },
                         ['sign', 'verify'], signOnly);
  await addPublicKeyCase('Ed25519', { name: 'Ed25519' },
                         ['sign', 'verify'], signOnly);
  await addPublicKeyCase('RSA-OAEP', {
    name: 'RSA-OAEP',
    modulusLength,
    publicExponent: new Uint8Array([1, 0, 1]),
    hash: 'SHA-256',
  }, ['encrypt', 'decrypt'], decryptOnly);
  await addPublicKeyCase('ML-DSA-44', { name: 'ML-DSA-44' },
                         ['sign', 'verify'], signOnly);
  await addPublicKeyCase('ML-KEM-512', { name: 'ML-KEM-512' },
                         ['encapsulateKey', 'decapsulateKey'],
                         decapsulateKeyOnly);

  for (const { name, message, importKey } of cases) {
    const outcome = await settleUnderPoison(poisonArrayIterator, importKey);
    assert.strictEqual(outcome.value, undefined, name);
    assert.strictEqual(outcome.error?.name, 'SyntaxError', name);
    if (message !== undefined) assert.match(outcome.error.message, message);
  }
}

// The registry, the Web IDL converters and the hash name aliases are built at
// module load, so poisoning those needs a fresh process. The child bodies are
// written as real functions and stringified into -e so that they stay linted.
async function runInFreshProcess(fn, args, expected) {
  const { code, stdout, stderr } = await common.spawnPromisified(
    process.execPath, ['-e', `(${fn})(${args})`]);
  assert.strictEqual(code, 0, stderr);
  assert.strictEqual(stdout.trim(), expected, stderr);
}

// Only the load happens under the poison: a sequence<KeyUsage> argument would
// legitimately come out empty while the caller's iterator is broken.
async function pollutedArrayIteratorChild(kmac) {
  const real = Array.prototype[Symbol.iterator];
  Array.prototype[Symbol.iterator] = () => ({ next: () => ({ done: true }) });
  const { subtle } = globalThis.crypto;
  const out = [];
  try {
    out.push((await subtle.digest('SHA-256', new Uint8Array(4))).byteLength);
  } finally {
    Array.prototype[Symbol.iterator] = real;
  }
  const hmac = await subtle.generateKey(
    { name: 'HMAC', hash: 'SHA-256' }, false, ['sign']);
  out.push((await subtle.sign('HMAC', hmac, new Uint8Array(4))).byteLength);
  const aes = await subtle.generateKey(
    { name: 'AES-GCM', length: 128 }, false, ['encrypt']);
  out.push((await subtle.encrypt(
    { name: 'AES-GCM', iv: new Uint8Array(12) }, aes, new Uint8Array(4),
  )).byteLength);
  if (kmac) {
    const key = await subtle.generateKey(
      { name: 'KMAC128', length: 128 }, false, ['sign']);
    out.push((await subtle.sign(
      { name: 'KMAC128', outputLength: 256 }, key, new Uint8Array(4),
    )).byteLength);
  }
  console.log(out.join(','));
}

// kHashNames indexes its aliases at load time.
async function pollutedHashNameChild() {
  Object.prototype['SHA-256'] = { 1: 'md5', 2: 'POLLUTED' };
  const { subtle } = globalThis.crypto;
  const key = await subtle.generateKey(
    { name: 'HMAC', hash: 'SHA-256' }, true, ['sign']);
  const signature = await subtle.sign('HMAC', key, new Uint8Array(4));
  const { alg } = await subtle.exportKey('jwk', key);
  console.log(`${signature.byteLength},${alg}`);
}

{
  const kmac = supports('generateKey', 'KMAC128');
  await runInFreshProcess(pollutedArrayIteratorChild, kmac,
                          kmac ? '32,32,20,32' : '32,32,20');
  await runInFreshProcess(pollutedHashNameChild, '', '32,HS256');
}

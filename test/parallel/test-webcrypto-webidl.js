// Flags: --expose-internals
'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');

const webidl = require('internal/crypto/webidl');
const { subtle } = globalThis.crypto;
const { generateKeySync } = require('crypto');

const { converters } = webidl;
const prefix = "Failed to execute 'fn' on 'interface'";
const context = '1st argument';
const opts = { prefix, context };

// Required arguments.length
{
  assert.throws(() => webidl.requiredArguments(0, 3, { prefix }), {
    code: 'ERR_MISSING_ARGS',
    name: 'TypeError',
    message: `${prefix}: 3 arguments required, but only 0 present.`
  });

  assert.throws(() => webidl.requiredArguments(0, 1, { prefix }), {
    code: 'ERR_MISSING_ARGS',
    name: 'TypeError',
    message: `${prefix}: 1 argument required, but only 0 present.`
  });

  // Does not throw when extra are added
  webidl.requiredArguments(4, 3, { prefix });
}

// boolean
{
  assert.strictEqual(converters.boolean(0), false);
  assert.strictEqual(converters.boolean(NaN), false);
  assert.strictEqual(converters.boolean(undefined), false);
  assert.strictEqual(converters.boolean(null), false);
  assert.strictEqual(converters.boolean(false), false);
  assert.strictEqual(converters.boolean(''), false);

  assert.strictEqual(converters.boolean(1), true);
  assert.strictEqual(converters.boolean(Number.POSITIVE_INFINITY), true);
  assert.strictEqual(converters.boolean(Number.NEGATIVE_INFINITY), true);
  assert.strictEqual(converters.boolean('1'), true);
  assert.strictEqual(converters.boolean('0'), true);
  assert.strictEqual(converters.boolean('false'), true);
  assert.strictEqual(converters.boolean(function() {}), true);
  assert.strictEqual(converters.boolean(Symbol()), true);
  assert.strictEqual(converters.boolean([]), true);
  assert.strictEqual(converters.boolean({}), true);
}

// int conversion
// https://webidl.spec.whatwg.org/#abstract-opdef-converttoint
{
  for (const [converter, max] of [
    [converters.octet, Math.pow(2, 8) - 1],
    [converters['unsigned short'], Math.pow(2, 16) - 1],
    [converters['unsigned long'], Math.pow(2, 32) - 1],
  ]) {
    assert.strictEqual(converter(0), 0);
    assert.strictEqual(converter(max), max);
    assert.strictEqual(converter('' + 0), 0);
    assert.strictEqual(converter('' + max), max);
    assert.strictEqual(converter(3), 3);
    assert.strictEqual(converter('' + 3), 3);
    assert.strictEqual(converter(3.1), 3);
    assert.strictEqual(converter(3.7), 3);

    assert.strictEqual(converter(max + 1), 0);
    assert.strictEqual(converter(max + 2), 1);
    assert.throws(() => converter(max + 1, { ...opts, enforceRange: true }), {
      name: 'TypeError',
      code: 'ERR_OUT_OF_RANGE',
      message: `${prefix}: ${context} is outside the expected range of 0 to ${max}.`,
    });

    assert.strictEqual(converter({}), 0);
    assert.strictEqual(converter(NaN), 0);
    assert.strictEqual(converter(false), 0);
    assert.strictEqual(converter(true), 1);
    assert.strictEqual(converter('1'), 1);
    assert.strictEqual(converter('0'), 0);
    assert.strictEqual(converter('{}'), 0);
    assert.strictEqual(converter({}), 0);
    assert.strictEqual(converter([]), 0);
    assert.strictEqual(converter(function() {}), 0);

    assert.throws(() => converter(Symbol(), opts), {
      name: 'TypeError',
      code: 'ERR_INVALID_ARG_TYPE',
      message: `${prefix}: ${context} is a Symbol and cannot be converted to a number.`
    });
    assert.throws(() => converter(0n, opts), {
      name: 'TypeError',
      code: 'ERR_INVALID_ARG_TYPE',
      message: `${prefix}: ${context} is a BigInt and cannot be converted to a number.`
    });
  }
}

// DOMString
{
  assert.strictEqual(converters.DOMString(1), '1');
  assert.strictEqual(converters.DOMString(1n), '1');
  assert.strictEqual(converters.DOMString(false), 'false');
  assert.strictEqual(converters.DOMString(true), 'true');
  assert.strictEqual(converters.DOMString(undefined), 'undefined');
  assert.strictEqual(converters.DOMString(NaN), 'NaN');
  assert.strictEqual(converters.DOMString({}), '[object Object]');
  assert.strictEqual(converters.DOMString({ foo: 'bar' }), '[object Object]');
  assert.strictEqual(converters.DOMString([]), '');
  assert.strictEqual(converters.DOMString([1, 2]), '1,2');

  assert.throws(() => converters.DOMString(Symbol(), opts), {
    name: 'TypeError',
    code: 'ERR_INVALID_ARG_TYPE',
    message: `${prefix}: ${context} is a Symbol and cannot be converted to a string.`
  });
}

// object
{
  for (const good of [{}, [], new Array(), function() {}]) {
    assert.deepStrictEqual(converters.object(good), good);
  }

  for (const bad of [undefined, null, NaN, false, true, 0, 1, '', 'foo', Symbol(), 9n]) {
    assert.throws(() => converters.object(bad, opts), {
      name: 'TypeError',
      code: 'ERR_INVALID_ARG_TYPE',
      message: `${prefix}: ${context} is not an object.`
    });
  }
}

// Uint8Array
{
  for (const good of [Buffer.alloc(0), new Uint8Array()]) {
    assert.deepStrictEqual(converters.Uint8Array(good), good);
  }

  for (const bad of [new ArrayBuffer(), new SharedArrayBuffer(), [], null, 'foo', undefined, true]) {
    assert.throws(() => converters.Uint8Array(bad, opts), {
      name: 'TypeError',
      code: 'ERR_INVALID_ARG_TYPE',
      message: `${prefix}: ${context} is not an Uint8Array object.`
    });
  }

  assert.throws(() => converters.Uint8Array(new Uint8Array(new SharedArrayBuffer()), opts), {
    name: 'TypeError',
    code: 'ERR_INVALID_ARG_TYPE',
    message: `${prefix}: ${context} is a view on a SharedArrayBuffer, which is not allowed.`
  });
}

// BufferSource
{
  for (const good of [
    Buffer.alloc(0),
    new Uint8Array(),
    new ArrayBuffer(),
    new DataView(new ArrayBuffer()),
    new BigInt64Array(),
    new BigUint64Array(),
    new Float32Array(),
    new Float64Array(),
    new Int8Array(),
    new Int16Array(),
    new Int32Array(),
    new Uint8ClampedArray(),
    new Uint16Array(),
    new Uint32Array(),
  ]) {
    assert.deepStrictEqual(converters.BufferSource(good), good);
  }

  for (const bad of [new SharedArrayBuffer(), [], null, 'foo', undefined, true]) {
    assert.throws(() => converters.BufferSource(bad, opts), {
      name: 'TypeError',
      code: 'ERR_INVALID_ARG_TYPE',
      message: `${prefix}: ${context} is not instance of ArrayBuffer, Buffer, TypedArray, or DataView.`
    });
  }

  assert.throws(() => converters.BufferSource(new Uint8Array(new SharedArrayBuffer()), opts), {
    name: 'TypeError',
    code: 'ERR_INVALID_ARG_TYPE',
    message: `${prefix}: ${context} is a view on a SharedArrayBuffer, which is not allowed.`
  });
}

// CryptoKey
{

  subtle.generateKey({ name: 'AES-CBC', length: 128 }, false, ['encrypt']).then((key) => {
    assert.deepStrictEqual(converters.CryptoKey(key), key);
  }).then(common.mustCall());

  for (const bad of [
    generateKeySync('aes', { length: 128 }),
    undefined, null, 1, {}, Symbol(), true, false, [],
  ]) {
    assert.throws(() => converters.CryptoKey(bad, opts), {
      name: 'TypeError',
      code: 'ERR_INVALID_ARG_TYPE',
      message: `${prefix}: ${context} is not of type CryptoKey.`
    });
  }
}

// sequence<DOMString>
{
  // TODO
}

// AlgorithmIdentifier (Union for (object or DOMString))
{
  assert.strictEqual(converters.AlgorithmIdentifier('foo'), 'foo');
  assert.deepStrictEqual(converters.AlgorithmIdentifier({ name: 'foo' }), { name: 'foo' });
}

// JsonWebKey
{
  // TODO
}

// BufferSource or JsonWebKey
{
  // TODO
}

// KeyFormat
{
  for (const good of ['jwk', 'spki', 'pkcs8', 'raw']) {
    assert.strictEqual(converters.KeyFormat(good), good);
  }

  for (const bad of ['foo', 1, false]) {
    assert.throws(() => converters.KeyFormat(bad, opts), {
      name: 'TypeError',
      code: 'ERR_INVALID_ARG_VALUE',
      message: `${prefix}: ${context} value '${bad}' is not a valid enum value of type KeyFormat.`,
    });
  }
}

// KeyUsage
{
  for (const good of [
    'encrypt',
    'decrypt',
    'sign',
    'verify',
    'deriveKey',
    'deriveBits',
    'wrapKey',
    'unwrapKey',
  ]) {
    assert.strictEqual(converters.KeyUsage(good), good);
  }

  for (const bad of ['foo', 1, false]) {
    assert.throws(() => converters.KeyUsage(bad, opts), {
      name: 'TypeError',
      code: 'ERR_INVALID_ARG_VALUE',
      message: `${prefix}: ${context} value '${bad}' is not a valid enum value of type KeyUsage.`,
    });
  }
}

// Algorithm
{
  const good = { name: 'RSA-PSS' };
  assert.deepStrictEqual(converters.Algorithm({ ...good, this: 'will get stripped' }, opts), good);

  assert.throws(() => converters.Algorithm({}, opts), {
    name: 'TypeError',
    code: 'ERR_MISSING_OPTION',
    message: `${prefix}: ${context} can not be converted to 'Algorithm' because 'name' is required in 'Algorithm'.`,
  });
}

// sequence<KeyUsage>
{
  // TODO
}

// RsaHashedKeyGenParams
{
  // TODO
}

// RsaHashedImportParams
{
  // TODO
}

// RsaPssParams
{
  const good = { name: 'RSA-PSS', saltLength: 20 };
  assert.deepStrictEqual(converters.RsaPssParams({ ...good, this: 'will get stripped' }, opts), good);

  assert.throws(() => converters.RsaPssParams({ ...good, saltLength: undefined }, opts), {
    name: 'TypeError',
    code: 'ERR_MISSING_OPTION',
    message: `${prefix}: ${context} can not be converted to 'RsaPssParams' because 'saltLength' is required in 'RsaPssParams'.`,
  });
}

// RsaOaepParams
{
  for (const good of [{ name: 'RSA-OAEP' }, { name: 'RSA-OAEP', label: Buffer.alloc(0) }]) {
    assert.deepStrictEqual(converters.RsaOaepParams({ ...good, this: 'will get stripped' }, opts), good);
  }
}

// EcKeyImportParams, EcKeyGenParams
{
  for (const name of ['EcKeyImportParams', 'EcKeyGenParams']) {
    const { [name]: converter } = converters;

    const good = { name: 'ECDSA', namedCurve: 'P-256' };
    assert.deepStrictEqual(converter({ ...good, this: 'will get stripped' }, opts), good);

    assert.throws(() => converter({ ...good, namedCurve: undefined }, opts), {
      name: 'TypeError',
      code: 'ERR_MISSING_OPTION',
      message: `${prefix}: ${context} can not be converted to '${name}' because 'namedCurve' is required in '${name}'.`,
    });
  }
}

// EcdsaParams
{
  // TODO
}

// HmacKeyGenParams, HmacImportParams
{
  for (const name of ['HmacKeyGenParams', 'HmacImportParams']) {
    const { [name]: converter } = converters;

    for (const good of [
      { name: 'HMAC', hash: { name: 'SHA-256' } },
      { name: 'HMAC', hash: { name: 'SHA-256' }, length: 20 },
      { name: 'HMAC', hash: 'SHA-256' },
      { name: 'HMAC', hash: 'SHA-256', length: 20 },
    ]) {
      assert.deepStrictEqual(converter({ ...good, this: 'will get stripped' }, opts), good);
      assert.throws(() => converter({ ...good, hash: undefined }, opts), {
        name: 'TypeError',
        code: 'ERR_MISSING_OPTION',
        message: `${prefix}: ${context} can not be converted to '${name}' because 'hash' is required in '${name}'.`,
      });
    }
  }
}

// AesKeyGenParams
{
  // TODO
}

// AesDerivedKeyParams
{
  // TODO
}

// HkdfParams
{
  // TODO
}

// Pbkdf2Params
{
  // TODO
}

// AesCbcParams
{
  // TODO
}

// AesGcmParams
{
  // TODO
}

// AesCtrParams
{
  // TODO
}

// EcdhKeyDeriveParams
{
  // TODO
}

// Ed448Params
{
  // TODO
}

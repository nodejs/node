'use strict';

const common = require('../common');

if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const { types: { isCryptoKey } } = require('util');
const {
  webcrypto: { subtle, CryptoKey },
  createSecretKey,
} = require('crypto');

const allUsages = [
  'encrypt',
  'decrypt',
  'sign',
  'verify',
  'deriveBits',
  'deriveKey',
  'wrapKey',
  'unwrapKey',
];
const vectors = {
  'AES-CTR': {
    algorithm: { length: 256 },
    usages: [
      'encrypt',
      'decrypt',
      'wrapKey',
      'unwrapKey',
    ],
    mandatoryUsages: []
  },
  'AES-CBC': {
    algorithm: { length: 256 },
    usages: [
      'encrypt',
      'decrypt',
      'wrapKey',
      'unwrapKey',
    ],
    mandatoryUsages: []
  },
  'AES-GCM': {
    algorithm: { length: 256 },
    usages: [
      'encrypt',
      'decrypt',
      'wrapKey',
      'unwrapKey',
    ],
    mandatoryUsages: []
  },
  'AES-KW': {
    algorithm: { length: 256 },
    usages: [
      'wrapKey',
      'unwrapKey',
    ],
    mandatoryUsages: []
  },
  'HMAC': {
    algorithm: { length: 256, hash: 'SHA-256' },
    usages: [
      'sign',
      'verify',
    ],
    mandatoryUsages: []
  },
  'RSASSA-PKCS1-v1_5': {
    algorithm: {
      modulusLength: 1024,
      publicExponent: new Uint8Array([1, 0, 1]),
      hash: 'SHA-256'
    },
    usages: [
      'sign',
      'verify',
    ],
    mandatoryUsages: ['sign'],
  },
  'RSA-PSS': {
    algorithm: {
      modulusLength: 1024,
      publicExponent: new Uint8Array([1, 0, 1]),
      hash: 'SHA-256'
    },
    usages: [
      'sign',
      'verify',
    ],
    mandatoryUsages: ['sign']
  },
  'RSA-OAEP': {
    algorithm: {
      modulusLength: 1024,
      publicExponent: new Uint8Array([1, 0, 1]),
      hash: 'SHA-256'
    },
    usages: [
      'encrypt',
      'decrypt',
      'wrapKey',
      'unwrapKey',
    ],
    mandatoryUsages: [
      'decrypt',
      'unwrapKey',
    ]
  },
  'ECDSA': {
    algorithm: { namedCurve: 'P-521' },
    usages: [
      'sign',
      'verify',
    ],
    mandatoryUsages: ['sign']
  },
  'ECDH': {
    algorithm: { namedCurve: 'P-521' },
    usages: [
      'deriveKey',
      'deriveBits',
    ],
    mandatoryUsages: [
      'deriveKey',
      'deriveBits',
    ]
  },
  'Ed25519': {
    usages: [
      'sign',
      'verify',
    ],
    mandatoryUsages: ['sign']
  },
  'Ed448': {
    usages: [
      'sign',
      'verify',
    ],
    mandatoryUsages: ['sign']
  },
  'X25519': {
    usages: [
      'deriveKey',
      'deriveBits',
    ],
    mandatoryUsages: [
      'deriveKey',
      'deriveBits',
    ]
  },
  'X448': {
    usages: [
      'deriveKey',
      'deriveBits',
    ],
    mandatoryUsages: [
      'deriveKey',
      'deriveBits',
    ]
  },
};

// Test invalid algorithms
{
  async function test(algorithm) {
    return assert.rejects(
      // The extractable and usages values are invalid here also,
      // but the unrecognized algorithm name should be caught first.
      subtle.generateKey(algorithm, 7, ['zebra']), {
        message: /Unrecognized name/
      });
  }

  const tests = [
    'AES',
    { name: 'AES' },
    { name: 'AES-CMAC' },
    { name: 'AES-CFB' },
    { name: 'HMAC', hash: 'MD5' },
    {
      name: 'RSA',
      hash: 'SHA-256',
      modulusLength: 2048,
      publicExponent: new Uint8Array([1, 0, 1])
    },
    {
      name: 'RSA-PSS',
      hash: 'SHA',
      modulusLength: 2048,
      publicExponent: new Uint8Array([1, 0, 1])
    },
    {
      name: 'EC',
      namedCurve: 'P521'
    },
  ].map(async (algorithm) => test(algorithm));

  Promise.all(tests).then(common.mustCall());
}

// Test bad usages
{
  async function test(name) {
    const invalidUsages = [];
    allUsages.forEach((usage) => {
      if (!vectors[name].usages.includes(usage))
        invalidUsages.push(usage);
    });
    return assert.rejects(
      subtle.generateKey(
        {
          name, ...vectors[name].algorithm
        },
        true,
        invalidUsages),
      { message: /Unsupported key usage/ });
  }

  const tests = Object.keys(vectors).map(test);

  Promise.all(tests).then(common.mustCall());
}

// Test RSA key generation
{
  async function test(
    name,
    modulusLength,
    publicExponent,
    hash,
    privateUsages,
    publicUsages = privateUsages) {
    let usages = privateUsages;
    if (publicUsages !== privateUsages)
      usages = usages.concat(publicUsages);
    const { publicKey, privateKey } = await subtle.generateKey({
      name,
      modulusLength,
      publicExponent,
      hash
    }, true, usages);

    assert(publicKey);
    assert(privateKey);
    assert(isCryptoKey(publicKey));
    assert(isCryptoKey(privateKey));

    assert(publicKey instanceof CryptoKey);
    assert(privateKey instanceof CryptoKey);

    assert.strictEqual(publicKey.type, 'public');
    assert.strictEqual(privateKey.type, 'private');
    assert.strictEqual(publicKey.extractable, true);
    assert.strictEqual(privateKey.extractable, true);
    assert.deepStrictEqual(publicKey.usages, publicUsages);
    assert.deepStrictEqual(privateKey.usages, privateUsages);
    assert.strictEqual(publicKey.algorithm.name, name);
    assert.strictEqual(publicKey.algorithm.modulusLength, modulusLength);
    assert.deepStrictEqual(publicKey.algorithm.publicExponent, publicExponent);
    assert.strictEqual(publicKey.algorithm.hash.name, hash);
    assert.strictEqual(privateKey.algorithm.name, name);
    assert.strictEqual(privateKey.algorithm.modulusLength, modulusLength);
    assert.deepStrictEqual(privateKey.algorithm.publicExponent, publicExponent);
    assert.strictEqual(privateKey.algorithm.hash.name, hash);

    // Missing parameters
    await assert.rejects(
      subtle.generateKey({ name, publicExponent, hash }, true, usages), {
        code: 'ERR_INVALID_ARG_TYPE'
      });

    await assert.rejects(
      subtle.generateKey({ name, modulusLength, hash }, true, usages), {
        code: 'ERR_INVALID_ARG_TYPE'
      });

    await assert.rejects(
      subtle.generateKey({ name, modulusLength }, true, usages), {
        code: 'ERR_MISSING_OPTION'
      });

    await Promise.all(['', true, {}].map((modulusLength) => {
      return assert.rejects(subtle.generateKey({
        name,
        modulusLength,
        publicExponent,
        hash
      }, true, usages), {
        code: 'ERR_INVALID_ARG_TYPE'
      });
    }));

    await Promise.all(
      [
        '',
        true,
        {},
        1,
        [],
        new Uint32Array(2),
      ].map((publicExponent) => {
        return assert.rejects(
          subtle.generateKey(
            { name, modulusLength, publicExponent, hash }, true, usages),
          { code: 'ERR_INVALID_ARG_TYPE' });
      }));

    await Promise.all([true, {}, 1, []].map((hash) => {
      return assert.rejects(subtle.generateKey({
        name,
        modulusLength,
        publicExponent,
        hash
      }, true, usages), {
        message: /Unrecognized name/
      });
    }));

    await Promise.all(['', {}, 1, []].map((extractable) => {
      return assert.rejects(subtle.generateKey({
        name,
        modulusLength,
        publicExponent,
        hash
      }, extractable, usages), {
        code: 'ERR_INVALID_ARG_TYPE'
      });
    }));

    await Promise.all(['', {}, 1, false].map((usages) => {
      return assert.rejects(subtle.generateKey({
        name,
        modulusLength,
        publicExponent,
        hash
      }, true, usages), {
        code: 'ERR_INVALID_ARG_TYPE'
      });
    }));
  }

  const kTests = [
    [
      'RSASSA-PKCS1-v1_5',
      1024,
      Buffer.from([1, 0, 1]),
      'SHA-256',
      ['sign'],
      ['verify'],
    ],
    [
      'RSA-PSS',
      2048,
      Buffer.from([1, 0, 1]),
      'SHA-512',
      ['sign'],
      ['verify'],
    ],
    [
      'RSA-OAEP',
      1024,
      Buffer.from([3]),
      'SHA-384',
      ['decrypt', 'unwrapKey'],
      ['encrypt', 'wrapKey'],
    ],
  ];

  const tests = kTests.map((args) => test(...args));

  Promise.all(tests).then(common.mustCall());
}

// Test EC Key Generation
{
  async function test(
    name,
    namedCurve,
    privateUsages,
    publicUsages = privateUsages) {

    let usages = privateUsages;
    if (publicUsages !== privateUsages)
      usages = usages.concat(publicUsages);

    const { publicKey, privateKey } = await subtle.generateKey({
      name,
      namedCurve
    }, true, usages);

    assert(publicKey);
    assert(privateKey);
    assert(isCryptoKey(publicKey));
    assert(isCryptoKey(privateKey));

    assert.strictEqual(publicKey.type, 'public');
    assert.strictEqual(privateKey.type, 'private');
    assert.strictEqual(publicKey.extractable, true);
    assert.strictEqual(privateKey.extractable, true);
    assert.deepStrictEqual(publicKey.usages, publicUsages);
    assert.deepStrictEqual(privateKey.usages, privateUsages);
    assert.strictEqual(publicKey.algorithm.name, name);
    assert.strictEqual(privateKey.algorithm.name, name);
    assert.strictEqual(publicKey.algorithm.namedCurve, namedCurve);
    assert.strictEqual(privateKey.algorithm.namedCurve, namedCurve);

    // Invalid parameters
    [1, true, {}, [], undefined, null].forEach(async (namedCurve) => {
      await assert.rejects(
        subtle.generateKey({ name, namedCurve }, true, privateUsages), {
          code: 'ERR_INVALID_ARG_TYPE'
        });
    });
  }

  const kTests = [
    [
      'ECDSA',
      'P-384',
      ['sign'],
      ['verify'],
    ],
    [
      'ECDSA',
      'P-521',
      ['sign'],
      ['verify'],
    ],
    [
      'ECDH',
      'P-384',
      ['deriveKey', 'deriveBits'],
      [],
    ],
    [
      'ECDH',
      'P-521',
      ['deriveKey', 'deriveBits'],
      [],
    ],
  ];

  const tests = kTests.map((args) => test(...args));

  // Test bad parameters

  Promise.all(tests).then(common.mustCall());
}

// Test AES Key Generation
{
  async function test(name, length, usages) {
    const key = await subtle.generateKey({
      name,
      length
    }, true, usages);

    assert(key);
    assert(isCryptoKey(key));

    assert.strictEqual(key.type, 'secret');
    assert.strictEqual(key.extractable, true);
    assert.deepStrictEqual(key.usages, usages);
    assert.strictEqual(key.algorithm.name, name);
    assert.strictEqual(key.algorithm.length, length);

    // Invalid parameters
    [1, 100, 257].forEach(async (length) => {
      await assert.rejects(
        subtle.generateKey({ name, length }, true, usages), {
          code: 'ERR_INVALID_ARG_VALUE'
        });
    });

    ['', {}, [], false, null, undefined].forEach(async (length) => {
      await assert.rejects(
        subtle.generateKey({ name, length }, true, usages), {
          code: 'ERR_INVALID_ARG_TYPE'
        });
    });
  }

  const kTests = [
    [ 'AES-CTR', 128, ['encrypt', 'decrypt', 'wrapKey']],
    [ 'AES-CTR', 256, ['encrypt', 'decrypt', 'unwrapKey']],
    [ 'AES-CBC', 128, ['encrypt', 'decrypt']],
    [ 'AES-CBC', 256, ['encrypt', 'decrypt']],
    [ 'AES-GCM', 128, ['encrypt', 'decrypt']],
    [ 'AES-GCM', 256, ['encrypt', 'decrypt']],
    [ 'AES-KW', 128, ['wrapKey', 'unwrapKey']],
    [ 'AES-KW', 256, ['wrapKey', 'unwrapKey']],
  ];

  const tests = Promise.all(kTests.map((args) => test(...args)));

  tests.then(common.mustCall());
}

// Test HMAC Key Generation
{
  async function test(length, hash, usages) {
    const key = await subtle.generateKey({
      name: 'HMAC',
      length,
      hash
    }, true, usages);

    if (length === undefined) {
      switch (hash) {
        case 'SHA-1': length = 160; break;
        case 'SHA-256': length = 256; break;
        case 'SHA-384': length = 384; break;
        case 'SHA-512': length = 512; break;
      }
    }

    assert(key);
    assert(isCryptoKey(key));

    assert.strictEqual(key.type, 'secret');
    assert.strictEqual(key.extractable, true);
    assert.deepStrictEqual(key.usages, usages);
    assert.strictEqual(key.algorithm.name, 'HMAC');
    assert.strictEqual(key.algorithm.length, length);
    assert.strictEqual(key.algorithm.hash.name, hash);

    ['', {}, [], false, null].forEach(async (length) => {
      await assert.rejects(
        subtle.generateKey({ name: 'HMAC', length, hash }, true, usages), {
          code: 'ERR_INVALID_ARG_TYPE'
        });
    });

    [1, {}, [], false, null].forEach(async (hash) => {
      await assert.rejects(
        subtle.generateKey({ name: 'HMAC', length, hash }, true, usages), {
          message: /Unrecognized name/
        });
    });
  }

  const kTests = [
    [ undefined, 'SHA-1', ['sign', 'verify']],
    [ undefined, 'SHA-256', ['sign', 'verify']],
    [ undefined, 'SHA-384', ['sign', 'verify']],
    [ undefined, 'SHA-512', ['sign', 'verify']],
    [ 128, 'SHA-256', ['sign', 'verify']],
    [ 1024, 'SHA-512', ['sign', 'verify']],
  ];

  const tests = Promise.all(kTests.map((args) => test(...args)));

  tests.then(common.mustCall());
}

// End user code cannot create CryptoKey directly
assert.throws(() => new CryptoKey(), { code: 'ERR_ILLEGAL_CONSTRUCTOR' });

{
  const buffer = Buffer.from('Hello World');
  const keyObject = createSecretKey(buffer);
  assert(!isCryptoKey(buffer));
  assert(!isCryptoKey(keyObject));
}

// Test OKP Key Generation
{
  async function test(
    name,
    privateUsages,
    publicUsages = privateUsages) {

    let usages = privateUsages;
    if (publicUsages !== privateUsages)
      usages = usages.concat(publicUsages);

    const { publicKey, privateKey } = await subtle.generateKey({
      name,
    }, true, usages);

    assert(publicKey);
    assert(privateKey);
    assert(isCryptoKey(publicKey));
    assert(isCryptoKey(privateKey));

    assert.strictEqual(publicKey.type, 'public');
    assert.strictEqual(privateKey.type, 'private');
    assert.strictEqual(publicKey.extractable, true);
    assert.strictEqual(privateKey.extractable, true);
    assert.deepStrictEqual(publicKey.usages, publicUsages);
    assert.deepStrictEqual(privateKey.usages, privateUsages);
    assert.strictEqual(publicKey.algorithm.name, name);
    assert.strictEqual(privateKey.algorithm.name, name);
  }

  const kTests = [
    [
      'Ed25519',
      ['sign'],
      ['verify'],
    ],
    [
      'Ed448',
      ['sign'],
      ['verify'],
    ],
    [
      'X25519',
      ['deriveKey', 'deriveBits'],
      [],
    ],
    [
      'X448',
      ['deriveKey', 'deriveBits'],
      [],
    ],
  ];

  const tests = kTests.map((args) => test(...args));

  // Test bad parameters

  Promise.all(tests).then(common.mustCall());
}

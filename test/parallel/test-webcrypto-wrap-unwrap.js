'use strict';

const common = require('../common');

if (!common.hasCrypto)
  common.skip('missing crypto');

const { hasOpenSSL } = require('../common/crypto');

const assert = require('assert');
const { subtle } = globalThis.crypto;

const kWrappingData = {
  'RSA-OAEP': {
    generate: {
      modulusLength: 4096,
      publicExponent: new Uint8Array([1, 0, 1]),
      hash: 'SHA-256',
    },
    wrap: { label: new Uint8Array(8) },
    pair: true
  },
  'AES-CTR': {
    generate: { length: 128 },
    wrap: { counter: new Uint8Array(16), length: 64 },
    pair: false
  },
  'AES-CBC': {
    generate: { length: 128 },
    wrap: { iv: new Uint8Array(16) },
    pair: false
  },
  'AES-GCM': {
    generate: { length: 128 },
    wrap: {
      iv: new Uint8Array(16),
      additionalData: new Uint8Array(16),
      tagLength: 64
    },
    pair: false
  },
  'ChaCha20-Poly1305': {
    wrap: {
      iv: new Uint8Array(12),
      additionalData: new Uint8Array(16),
      tagLength: 128
    },
    pair: false
  },
  'AES-KW': {
    generate: { length: 128 },
    wrap: { },
    pair: false
  }
};

function generateWrappingKeys() {
  return Promise.all(Object.keys(kWrappingData).map(async (name) => {
    const keys = await subtle.generateKey(
      { name, ...kWrappingData[name].generate },
      true,
      ['wrapKey', 'unwrapKey']);
    if (kWrappingData[name].pair) {
      kWrappingData[name].wrappingKey = keys.publicKey;
      kWrappingData[name].unwrappingKey = keys.privateKey;
    } else {
      kWrappingData[name].wrappingKey = keys;
      kWrappingData[name].unwrappingKey = keys;
    }
  }));
}

async function generateKeysToWrap() {
  const parameters = [
    {
      algorithm: {
        name: 'RSASSA-PKCS1-v1_5',
        modulusLength: 1024,
        publicExponent: new Uint8Array([1, 0, 1]),
        hash: 'SHA-256'
      },
      privateUsages: ['sign'],
      publicUsages: ['verify'],
      pair: true,
    },
    {
      algorithm: {
        name: 'RSA-PSS',
        modulusLength: 1024,
        publicExponent: new Uint8Array([1, 0, 1]),
        hash: 'SHA-256'
      },
      privateUsages: ['sign'],
      publicUsages: ['verify'],
      pair: true,
    },
    {
      algorithm: {
        name: 'RSA-OAEP',
        modulusLength: 1024,
        publicExponent: new Uint8Array([1, 0, 1]),
        hash: 'SHA-256'
      },
      privateUsages: ['decrypt'],
      publicUsages: ['encrypt'],
      pair: true,
    },
    {
      algorithm: {
        name: 'ECDSA',
        namedCurve: 'P-384'
      },
      privateUsages: ['sign'],
      publicUsages: ['verify'],
      pair: true,
    },
    {
      algorithm: {
        name: 'ECDH',
        namedCurve: 'P-384'
      },
      privateUsages: ['deriveBits'],
      publicUsages: [],
      pair: true,
    },
    {
      algorithm: {
        name: 'Ed25519',
      },
      privateUsages: ['sign'],
      publicUsages: ['verify'],
      pair: true,
    },
    {
      algorithm: {
        name: 'Ed448',
      },
      privateUsages: ['sign'],
      publicUsages: ['verify'],
      pair: true,
    },
    {
      algorithm: {
        name: 'X25519',
      },
      privateUsages: ['deriveBits'],
      publicUsages: [],
      pair: true,
    },
    {
      algorithm: {
        name: 'X448',
      },
      privateUsages: ['deriveBits'],
      publicUsages: [],
      pair: true,
    },
    {
      algorithm: {
        name: 'AES-CTR',
        length: 128
      },
      usages: ['encrypt', 'decrypt'],
      pair: false,
    },
    {
      algorithm: {
        name: 'AES-CBC',
        length: 128
      },
      usages: ['encrypt', 'decrypt'],
      pair: false,
    },
    {
      algorithm: {
        name: 'AES-GCM', length: 128
      },
      usages: ['encrypt', 'decrypt'],
      pair: false,
    },
    {
      algorithm: {
        name: 'ChaCha20-Poly1305'
      },
      usages: ['encrypt', 'decrypt'],
      pair: false,
    },
    {
      algorithm: {
        name: 'AES-KW',
        length: 128
      },
      usages: ['wrapKey', 'unwrapKey'],
      pair: false,
    },
    {
      algorithm: {
        name: 'HMAC',
        length: 128,
        hash: 'SHA-256'
      },
      usages: ['sign', 'verify'],
      pair: false,
    },
  ];

  if (hasOpenSSL(3, 5)) {
    for (const name of ['ML-DSA-44', 'ML-DSA-65', 'ML-DSA-87']) {
      parameters.push({
        algorithm: { name },
        privateUsages: ['sign'],
        publicUsages: ['verify'],
        pair: true,
      });
    }
  }

  const allkeys = await Promise.all(parameters.map(async (params) => {
    const usages = 'usages' in params ?
      params.usages :
      params.publicUsages.concat(params.privateUsages);

    const keys = await subtle.generateKey(params.algorithm, true, usages);

    if (params.pair) {
      return [
        {
          algorithm: params.algorithm,
          usages: params.publicUsages,
          key: keys.publicKey,
        },
        {
          algorithm: params.algorithm,
          usages: params.privateUsages,
          key: keys.privateKey,
        },
      ];
    }

    return [{
      algorithm: params.algorithm,
      usages: params.usages,
      key: keys,
    }];
  }));

  return allkeys.flat();
}

function getFormats(key) {
  switch (key.type) {
    case 'secret': {
      if (key.algorithm.name === 'ChaCha20-Poly1305') return ['raw-secret', 'jwk'];
      return ['raw-secret', 'raw', 'jwk'];
    };
    case 'public': {
      switch (key.algorithm.name.slice(0, 2)) {
        case 'EC': // ECDSA, ECDH
          return ['spki', 'jwk', 'raw', 'raw-public'];
        case 'ML': // ML-DSA
          return ['jwk', 'raw-public'];
        default:
          return ['spki', 'jwk'];
      }
    }
    case 'private': {
      switch (key.algorithm.name.slice(0, 2)) {
        case 'ML': // ML-DSA
          return ['jwk', 'raw-seed'];
        default:
          return ['pkcs8', 'jwk'];
      }
    }
  }
}

// If the wrapping algorithm is AES-KW, the exported key
// material length must be a multiple of 8.
// If the wrapping algorithm is RSA-OAEP, the exported key
// material maximum length is a factor of the modulusLength
//
// As per the NOTE in step 13 https://w3c.github.io/webcrypto/#SubtleCrypto-method-wrapKey
// we're padding AES-KW wrapped JWK to make sure it is always a multiple of 8 bytes
// in length
async function wrappingIsPossible(name, exported) {
  if ('byteLength' in exported) {
    switch (name) {
      case 'AES-KW':
        return exported.byteLength % 8 === 0;
      case 'RSA-OAEP':
        return exported.byteLength <= 446;
    }
  } else if ('kty' in exported && name === 'RSA-OAEP') {
    return JSON.stringify(exported).length <= 478;
  }
  return true;
}

async function testWrap(wrappingKey, unwrappingKey, key, wrap, format) {
  const exported = await subtle.exportKey(format, key.key);
  if (!(await wrappingIsPossible(wrappingKey.algorithm.name, exported)))
    return;

  const wrapped =
    await subtle.wrapKey(
      format,
      key.key,
      wrappingKey,
      { name: wrappingKey.algorithm.name, ...wrap });
  const unwrapped =
    await subtle.unwrapKey(
      format,
      wrapped,
      unwrappingKey,
      { name: wrappingKey.algorithm.name, ...wrap },
      key.algorithm,
      true,
      key.usages);
  assert(unwrapped.extractable);

  const exportedAgain = await subtle.exportKey(format, unwrapped);
  assert.deepStrictEqual(exported, exportedAgain);
}

function testWrapping(name, keys) {
  const variations = [];

  const {
    wrappingKey,
    unwrappingKey,
    wrap
  } = kWrappingData[name];

  keys.forEach((key) => {
    getFormats(key.key).forEach((format) => {
      variations.push(testWrap(wrappingKey, unwrappingKey, key, wrap, format));
    });
  });

  return variations;
}

(async function() {
  await generateWrappingKeys();
  const keys = await generateKeysToWrap();
  const variations = [];
  Object.keys(kWrappingData).forEach((name) => {
    variations.push(...testWrapping(name, keys));
  });
  await Promise.all(variations);
})().then(common.mustCall());

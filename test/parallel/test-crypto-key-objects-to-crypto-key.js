'use strict';
// Flags: --expose-internals

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const {
  createSecretKey,
  KeyObject,
  randomBytes,
  generateKeyPairSync,
} = require('crypto');
const { kSupportedAlgorithms } = require('internal/crypto/util');

const hashes = Object.keys(kSupportedAlgorithms.digest).filter((name) => {
  return name.startsWith('SHA-') || name.startsWith('SHA3-');
});
const namedCurves = ['P-256', 'P-384', 'P-521'];

const signingUsages = {
  public: ['verify'],
  private: ['sign'],
};
const derivationUsages = {
  public: [],
  private: ['deriveBits'],
};
const rsaOaepUsages = {
  public: ['encrypt', 'wrapKey'],
  private: ['decrypt', 'unwrapKey'],
};
const mlKemUsages = {
  public: ['encapsulateBits'],
  private: ['decapsulateBits'],
};

function assertCryptoKey(cryptoKey, keyObject, algorithm, extractable, usages) {
  assert.strictEqual(cryptoKey instanceof CryptoKey, true);
  assert.strictEqual(cryptoKey.type, keyObject.type);
  assert.strictEqual(cryptoKey.algorithm.name, algorithm);
  assert.strictEqual(cryptoKey.extractable, extractable);
  assert.deepStrictEqual(cryptoKey.usages, usages);
  if (extractable) {
    assert.strictEqual(keyObject.equals(KeyObject.from(cryptoKey)), true);
  } else {
    assert.throws(() => KeyObject.from(cryptoKey), {
      code: 'ERR_INVALID_ARG_VALUE',
      message: /must be an extractable CryptoKey/,
    });
  }
}

function algorithmName(algorithm) {
  return typeof algorithm === 'string' ? algorithm : algorithm.name;
}

function getUsages(keyObject, usages) {
  return keyObject.type === 'public' ? usages.public : usages.private;
}

function secretKey(length) {
  return createSecretKey(randomBytes(length >> 3));
}

function vector(
  keyObject,
  algorithm,
  usages,
  expected,
  extractable,
) {
  return {
    keyObject,
    algorithm,
    usages,
    expected,
    extractable,
  };
}

function assertVector(vector, extractable) {
  const { keyObject, algorithm, usages, expected } = vector;
  const name = algorithmName(algorithm);

  const cryptoKey = keyObject.toCryptoKey(algorithm, extractable, usages);
  assertCryptoKey(cryptoKey, keyObject, name, extractable, usages);
  if (expected !== undefined && 'length' in expected)
    assert.strictEqual(cryptoKey.algorithm.length, expected.length);
  if (expected !== undefined && 'hash' in expected)
    assert.strictEqual(cryptoKey.algorithm.hash.name, expected.hash);
  if (expected !== undefined && 'namedCurve' in expected)
    assert.strictEqual(cryptoKey.algorithm.namedCurve, expected.namedCurve);
}

function runVector(vector) {
  if (vector.extractable !== undefined) {
    assertVector(vector, vector.extractable);
    return;
  }
  assertVector(vector, true);
  assertVector(vector, false);
}

function symmetricVectors(name, usages) {
  const vectors = [];
  for (const length of [128, 192, 256]) {
    vectors.push(vector(
      secretKey(length),
      name,
      usages,
      { length }));
  }
  return vectors;
}

function chacha20Poly1305Vectors() {
  return [
    vector(
      secretKey(256),
      'ChaCha20-Poly1305',
      ['encrypt', 'decrypt'],
      { length: undefined }),
  ];
}

function genericSecretVectors(name) {
  return [
    vector(
      createSecretKey(randomBytes(16)),
      name,
      ['deriveBits'],
      undefined,
      false),
  ];
}

function macInvalid(algorithm, invalidLengthMessage, allowZeroKey = false) {
  const key = createSecretKey(randomBytes(32));
  const usages = ['sign', 'verify'];

  if (allowZeroKey) {
    const zeroKey = createSecretKey(Buffer.alloc(0))
      .toCryptoKey(algorithm, true, usages);
    assert.strictEqual(zeroKey.algorithm.length, 0);

    const explicitZeroKey = createSecretKey(Buffer.alloc(0))
      .toCryptoKey({ ...algorithm, length: 0 }, true, usages);
    assert.strictEqual(explicitZeroKey.algorithm.length, 0);
  } else {
    assert.throws(() => {
      createSecretKey(Buffer.alloc(0)).toCryptoKey(algorithm, true, usages);
    }, {
      name: 'DataError',
      message: 'Zero-length key is not supported',
    });
  }

  assert.throws(() => key.toCryptoKey(algorithm, true, []), {
    name: 'SyntaxError',
    message: 'Usages cannot be empty when importing a secret key.'
  });

  assert.throws(() => {
    key.toCryptoKey({ ...algorithm, length: 0 }, true, usages);
  }, {
    name: 'DataError',
    message: invalidLengthMessage,
  });
}

function hmacVectors() {
  const vectors = [];
  for (const length of [128, 192, 256]) {
    const key = secretKey(length);
    for (const hash of hashes) {
      vectors.push(vector(
        key,
        { name: 'HMAC', hash },
        ['sign', 'verify'],
        { length }));
    }
  }
  return vectors;
}

function kmacVectors(name) {
  return [
    vector(
      secretKey(256),
      { name },
      ['sign', 'verify'],
      { length: 256 }),
  ];
}

function asymmetricVectors(keyPair, algorithm, usagesByType, expected) {
  const { publicKey, privateKey } = keyPair;
  const vectors = [];
  for (const keyObject of [publicKey, privateKey]) {
    vectors.push(vector(
      keyObject,
      algorithm,
      getUsages(keyObject, usagesByType),
      expected));
  }
  return vectors;
}

function rsaVectors(name, usagesByType) {
  const keyPair = generateKeyPairSync('rsa', { modulusLength: 2048 });
  const vectors = [];
  for (const keyObject of [keyPair.publicKey, keyPair.privateKey]) {
    for (const hash of hashes) {
      vectors.push(vector(
        keyObject,
        { name, hash },
        getUsages(keyObject, usagesByType),
        { hash }));
    }
  }
  return vectors;
}

function ecVectors(name, usagesByType) {
  const vectors = [];
  for (const namedCurve of namedCurves) {
    const keyPair = generateKeyPairSync('ec', { namedCurve });
    for (const keyObject of [keyPair.publicKey, keyPair.privateKey]) {
      vectors.push(vector(
        keyObject,
        { name, namedCurve },
        getUsages(keyObject, usagesByType),
        { namedCurve }));
    }
  }
  return vectors;
}

function cfrgVectors(name, usagesByType) {
  const keyPair = generateKeyPairSync(name.toLowerCase());
  return asymmetricVectors(keyPair, name, usagesByType);
}

function simpleAsymmetricVectors(name, usagesByType) {
  const keyPair = generateKeyPairSync(name.toLowerCase());
  return asymmetricVectors(keyPair, { name }, usagesByType);
}

function genericSecretInvalid(name) {
  const key = createSecretKey(randomBytes(16));
  assert.throws(() => key.toCryptoKey(name, true, ['deriveBits']), {
    name: 'SyntaxError',
    message: `${name} keys are not extractable`
  });
  assert.throws(() => key.toCryptoKey(name, false, ['wrapKey']), {
    name: 'SyntaxError',
    message: `Unsupported key usage for a ${name} key`
  });
}

function ecdhInvalid() {
  const { publicKey, privateKey } = generateKeyPairSync('ec', {
    namedCurve: 'P-256',
  });

  assert.throws(() => {
    privateKey.toCryptoKey({
      name: 'ECDH',
      namedCurve: 'P-256',
    }, true, []);
  }, {
    name: 'SyntaxError',
    message: 'Usages cannot be empty when importing a private key.'
  });

  assert.throws(() => {
    publicKey.toCryptoKey({
      name: 'ECDH',
      namedCurve: 'P-384',
    }, true, []);
  }, {
    name: 'DataError',
    message: 'Named curve mismatch'
  });
}

function invalidAsymmetricKeyType(name, invalidAlgorithm) {
  const { publicKey } = generateKeyPairSync(name.toLowerCase());
  assert.throws(() => {
    publicKey.toCryptoKey(invalidAlgorithm, true, []);
  }, {
    name: 'DataError',
    message: 'Invalid key type'
  });
}

function emptyPrivateUsagesInvalid(name) {
  const { privateKey } = generateKeyPairSync(name.toLowerCase());
  assert.throws(() => {
    privateKey.toCryptoKey(name, true, []);
  }, {
    name: 'SyntaxError',
    message: 'Usages cannot be empty when importing a private key.'
  });
}

const tests = {
  'AES-KW': symmetricVectors('AES-KW', ['wrapKey', 'unwrapKey']),
  'ECDH': ecVectors('ECDH', derivationUsages),
  'ECDSA': ecVectors('ECDSA', signingUsages),
  'Ed25519': cfrgVectors('Ed25519', signingUsages),
  'HMAC': hmacVectors(),
  'RSA-OAEP': rsaVectors('RSA-OAEP', rsaOaepUsages),
  'RSA-PSS': rsaVectors('RSA-PSS', signingUsages),
  'RSASSA-PKCS1-v1_5': rsaVectors('RSASSA-PKCS1-v1_5', signingUsages),
  'X25519': cfrgVectors('X25519', derivationUsages),
};

const invalid = {
  'ECDH': ecdhInvalid,
  'Ed25519': () => invalidAsymmetricKeyType('Ed25519', 'X25519'),
  'HMAC': () => macInvalid(
    { name: 'HMAC', hash: 'SHA-256' },
    'HmacImportParams.length cannot be 0'),
  'X25519': () => invalidAsymmetricKeyType('X25519', 'Ed25519'),
};

for (const name of ['AES-CBC', 'AES-CTR', 'AES-GCM', 'AES-OCB']) {
  if (name in kSupportedAlgorithms.importKey)
    tests[name] = symmetricVectors(name, ['encrypt', 'decrypt']);
}

if ('ChaCha20-Poly1305' in kSupportedAlgorithms.importKey)
  tests['ChaCha20-Poly1305'] = chacha20Poly1305Vectors();

for (const name of ['HKDF', 'PBKDF2', 'Argon2d', 'Argon2i', 'Argon2id']) {
  if (name in kSupportedAlgorithms.importKey) {
    tests[name] = genericSecretVectors(name);
    invalid[name] = () => genericSecretInvalid(name);
  }
}

for (const name of ['KMAC128', 'KMAC256']) {
  if (name in kSupportedAlgorithms.importKey) {
    tests[name] = kmacVectors(name);
    invalid[name] = () => {
      macInvalid({ name }, 'Invalid key length', true);
    };
  }
}

for (const [name, usages, invalidAlgorithm] of [
  ['Ed448', signingUsages, 'X25519'],
  ['X448', derivationUsages, 'Ed25519'],
]) {
  if (name in kSupportedAlgorithms.importKey) {
    tests[name] = cfrgVectors(name, usages);
    invalid[name] = () => {
      invalidAsymmetricKeyType(name, invalidAlgorithm);
    };
  }
}

for (const name of ['ML-DSA-44', 'ML-DSA-65', 'ML-DSA-87']) {
  if (name in kSupportedAlgorithms.importKey) {
    tests[name] = simpleAsymmetricVectors(name, signingUsages);
    invalid[name] = () => emptyPrivateUsagesInvalid(name);
  }
}

for (const name of ['ML-KEM-512', 'ML-KEM-768', 'ML-KEM-1024']) {
  if (name in kSupportedAlgorithms.importKey) {
    tests[name] = simpleAsymmetricVectors(name, mlKemUsages);
    invalid[name] = () => emptyPrivateUsagesInvalid(name);
  }
}

const unsupportedToCryptoKeyAlgorithms = new Set();

for (const name of Object.keys(kSupportedAlgorithms.importKey)) {
  const vectors = tests[name];
  if (vectors === undefined) {
    if (!unsupportedToCryptoKeyAlgorithms.has(name)) {
      assert.fail(
        `${name} needs a tests entry or must be listed in ` +
        'unsupportedToCryptoKeyAlgorithms');
    }
    continue;
  }

  for (const vector of vectors)
    runVector(vector);

  invalid[name]?.();
}

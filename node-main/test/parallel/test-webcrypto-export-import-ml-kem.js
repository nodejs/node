'use strict';

const common = require('../common');

if (!common.hasCrypto)
  common.skip('missing crypto');

const { hasOpenSSL } = require('../common/crypto');

if (!hasOpenSSL(3, 5))
  common.skip('requires OpenSSL >= 3.5');

const assert = require('assert');
const { subtle } = globalThis.crypto;

const fixtures = require('../common/fixtures');

function getKeyFileName(type, suffix) {
  return `${type.replaceAll('-', '_')}_${suffix}.pem`;
}

function toDer(pem) {
  const der = pem.replace(/(?:-----(?:BEGIN|END) (?:PRIVATE|PUBLIC) KEY-----|\s)/g, '');
  return Buffer.alloc(Buffer.byteLength(der, 'base64'), der, 'base64');
}

const keyData = {
  'ML-KEM-512': {
    pkcs8_seed_only: toDer(fixtures.readKey(getKeyFileName('ml-kem-512', 'private_seed_only'), 'ascii')),
    pkcs8: toDer(fixtures.readKey(getKeyFileName('ml-kem-512', 'private'), 'ascii')),
    pkcs8_priv_only: toDer(fixtures.readKey(getKeyFileName('ml-kem-512', 'private_priv_only'), 'ascii')),
    spki: toDer(fixtures.readKey(getKeyFileName('ml-kem-512', 'public'), 'ascii')),
    pub_len: 800,
  },
  'ML-KEM-768': {
    pkcs8_seed_only: toDer(fixtures.readKey(getKeyFileName('ml-kem-768', 'private_seed_only'), 'ascii')),
    pkcs8: toDer(fixtures.readKey(getKeyFileName('ml-kem-768', 'private'), 'ascii')),
    pkcs8_priv_only: toDer(fixtures.readKey(getKeyFileName('ml-kem-768', 'private_priv_only'), 'ascii')),
    spki: toDer(fixtures.readKey(getKeyFileName('ml-kem-768', 'public'), 'ascii')),
    pub_len: 1184,
  },
  'ML-KEM-1024': {
    pkcs8_seed_only: toDer(fixtures.readKey(getKeyFileName('ml-kem-1024', 'private_seed_only'), 'ascii')),
    pkcs8: toDer(fixtures.readKey(getKeyFileName('ml-kem-1024', 'private'), 'ascii')),
    pkcs8_priv_only: toDer(fixtures.readKey(getKeyFileName('ml-kem-1024', 'private_priv_only'), 'ascii')),
    spki: toDer(fixtures.readKey(getKeyFileName('ml-kem-1024', 'public'), 'ascii')),
    pub_len: 1568,
  },
};

const testVectors = [
  {
    name: 'ML-KEM-512',
    privateUsages: ['decapsulateKey', 'decapsulateBits'],
    publicUsages: ['encapsulateKey', 'encapsulateBits']
  },
  {
    name: 'ML-KEM-768',
    privateUsages: ['decapsulateKey', 'decapsulateBits'],
    publicUsages: ['encapsulateKey', 'encapsulateBits']
  },
  {
    name: 'ML-KEM-1024',
    privateUsages: ['decapsulateKey', 'decapsulateBits'],
    publicUsages: ['encapsulateKey', 'encapsulateBits']
  },
];

async function testImportSpki({ name, publicUsages }, extractable) {
  const key = await subtle.importKey(
    'spki',
    keyData[name].spki,
    { name },
    extractable,
    publicUsages);
  assert.strictEqual(key.type, 'public');
  assert.strictEqual(key.extractable, extractable);
  assert.deepStrictEqual(key.usages, publicUsages);
  assert.deepStrictEqual(key.algorithm.name, name);
  assert.strictEqual(key.algorithm, key.algorithm);
  assert.strictEqual(key.usages, key.usages);

  if (extractable) {
    // Test the roundtrip
    const spki = await subtle.exportKey('spki', key);
    assert.strictEqual(
      Buffer.from(spki).toString('hex'),
      keyData[name].spki.toString('hex'));
  } else {
    await assert.rejects(
      subtle.exportKey('spki', key), {
        message: /key is not extractable/
      });
  }

  // Bad usage
  await assert.rejects(
    subtle.importKey(
      'spki',
      keyData[name].spki,
      { name },
      extractable,
      ['wrapKey']),
    { message: /Unsupported key usage/ });
}

async function testImportPkcs8({ name, privateUsages }, extractable) {
  const key = await subtle.importKey(
    'pkcs8',
    keyData[name].pkcs8,
    { name },
    extractable,
    privateUsages);
  assert.strictEqual(key.type, 'private');
  assert.strictEqual(key.extractable, extractable);
  assert.deepStrictEqual(key.usages, privateUsages);
  assert.deepStrictEqual(key.algorithm.name, name);
  assert.strictEqual(key.algorithm, key.algorithm);
  assert.strictEqual(key.usages, key.usages);

  if (extractable) {
    // Test the roundtrip
    const pkcs8 = await subtle.exportKey('pkcs8', key);
    assert.strictEqual(
      Buffer.from(pkcs8).toString('hex'),
      keyData[name].pkcs8_seed_only.toString('hex'));
  } else {
    await assert.rejects(
      subtle.exportKey('pkcs8', key), {
        message: /key is not extractable/
      });
  }

  await assert.rejects(
    subtle.importKey(
      'pkcs8',
      keyData[name].pkcs8,
      { name },
      extractable,
      [/* empty usages */]),
    { name: 'SyntaxError', message: 'Usages cannot be empty when importing a private key.' });
}

async function testImportPkcs8SeedOnly({ name, privateUsages }, extractable) {
  const key = await subtle.importKey(
    'pkcs8',
    keyData[name].pkcs8_seed_only,
    { name },
    extractable,
    privateUsages);
  assert.strictEqual(key.type, 'private');
  assert.strictEqual(key.extractable, extractable);
  assert.deepStrictEqual(key.usages, privateUsages);
  assert.deepStrictEqual(key.algorithm.name, name);
  assert.strictEqual(key.algorithm, key.algorithm);
  assert.strictEqual(key.usages, key.usages);

  if (extractable) {
    // Test the roundtrip
    const pkcs8 = await subtle.exportKey('pkcs8', key);
    assert.strictEqual(
      Buffer.from(pkcs8).toString('hex'),
      keyData[name].pkcs8_seed_only.toString('hex'));
  } else {
    await assert.rejects(
      subtle.exportKey('pkcs8', key), {
        message: /key is not extractable/
      });
  }

  await assert.rejects(
    subtle.importKey(
      'pkcs8',
      keyData[name].pkcs8_seed_only,
      { name },
      extractable,
      [/* empty usages */]),
    { name: 'SyntaxError', message: 'Usages cannot be empty when importing a private key.' });
}

async function testImportPkcs8PrivOnly({ name, privateUsages }, extractable) {
  const key = await subtle.importKey(
    'pkcs8',
    keyData[name].pkcs8_priv_only,
    { name },
    extractable,
    privateUsages);
  assert.strictEqual(key.type, 'private');
  assert.strictEqual(key.extractable, extractable);
  assert.deepStrictEqual(key.usages, privateUsages);
  assert.deepStrictEqual(key.algorithm.name, name);
  assert.strictEqual(key.algorithm, key.algorithm);
  assert.strictEqual(key.usages, key.usages);

  if (extractable) {
    await assert.rejects(subtle.exportKey('pkcs8', key), (err) => {
      assert.strictEqual(err.name, 'OperationError');
      assert.strictEqual(err.cause.code, 'ERR_CRYPTO_OPERATION_FAILED');
      assert.strictEqual(err.cause.message, 'Failed to get raw seed');
      return true;
    });
  } else {
    await assert.rejects(
      subtle.exportKey('pkcs8', key), {
        message: /key is not extractable/
      });
  }

  await assert.rejects(
    subtle.importKey(
      'pkcs8',
      keyData[name].pkcs8_seed_only,
      { name },
      extractable,
      [/* empty usages */]),
    { name: 'SyntaxError', message: 'Usages cannot be empty when importing a private key.' });
}

async function testImportRawPublic({ name, publicUsages }, extractable) {
  const pub = keyData[name].spki.subarray(-keyData[name].pub_len);

  const publicKey = await subtle.importKey(
    'raw-public',
    pub,
    { name },
    extractable, publicUsages);

  assert.strictEqual(publicKey.type, 'public');
  assert.deepStrictEqual(publicKey.usages, publicUsages);
  assert.strictEqual(publicKey.algorithm.name, name);
  assert.strictEqual(publicKey.algorithm, publicKey.algorithm);
  assert.strictEqual(publicKey.usages, publicKey.usages);
  assert.strictEqual(publicKey.extractable, extractable);

  if (extractable) {
    const value = await subtle.exportKey('raw-public', publicKey);
    assert.deepStrictEqual(Buffer.from(value), pub);

    await assert.rejects(subtle.exportKey('raw', publicKey), {
      name: 'NotSupportedError',
      message: `Unable to export ${publicKey.algorithm.name} public key using raw format`,
    });
  }

  await assert.rejects(
    subtle.importKey(
      'raw-public',
      pub.subarray(0, pub.byteLength - 1),
      { name },
      extractable, publicUsages),
    { message: 'Invalid keyData' });

  await assert.rejects(
    subtle.importKey(
      'raw-public',
      pub,
      { name: name === 'ML-KEM-512' ? 'ML-KEM-768' : 'ML-KEM-512' },
      extractable, publicUsages),
    { message: 'Invalid keyData' });
}

async function testImportRawSeed({ name, privateUsages }, extractable) {
  const seed = keyData[name].pkcs8_seed_only.subarray(-64);

  const privateKey = await subtle.importKey(
    'raw-seed',
    seed,
    { name },
    extractable, privateUsages);

  assert.strictEqual(privateKey.type, 'private');
  assert.deepStrictEqual(privateKey.usages, privateUsages);
  assert.strictEqual(privateKey.algorithm.name, name);
  assert.strictEqual(privateKey.algorithm, privateKey.algorithm);
  assert.strictEqual(privateKey.usages, privateKey.usages);
  assert.strictEqual(privateKey.extractable, extractable);

  if (extractable) {
    const value = await subtle.exportKey('raw-seed', privateKey);
    assert.deepStrictEqual(Buffer.from(value), seed);
  }

  await assert.rejects(
    subtle.importKey(
      'raw-seed',
      seed.subarray(0, 30),
      { name },
      extractable,
      privateUsages),
    { message: 'Invalid keyData' });
}

(async function() {
  const tests = [];
  for (const vector of testVectors) {
    for (const extractable of [true, false]) {
      tests.push(testImportSpki(vector, extractable));
      tests.push(testImportPkcs8(vector, extractable));
      tests.push(testImportPkcs8SeedOnly(vector, extractable));
      tests.push(testImportPkcs8PrivOnly(vector, extractable));
      tests.push(testImportRawSeed(vector, extractable));
      tests.push(testImportRawPublic(vector, extractable));
    }
  }
  await Promise.all(tests);
})().then(common.mustCall());

(async function() {
  const alg = 'ML-KEM-512';
  const pub = keyData[alg].spki.subarray(-keyData[alg].pub_len);
  await assert.rejects(subtle.importKey('raw', pub, alg, false, []), {
    name: 'NotSupportedError',
    message: 'Unable to import ML-KEM-512 using raw format',
  });
})().then(common.mustCall());

'use strict';

const common = require('../common');

if (!common.hasCrypto)
  common.skip('missing crypto');

const { hasOpenSSL } = require('../common/crypto');

if (!hasOpenSSL(3, 5))
  common.skip('requires OpenSSL >= 3.5');

const assert = require('assert');
const { subtle } = globalThis.crypto;
const { createPrivateKey } = require('crypto');

const fixtures = require('../common/fixtures');

function getKeyFileName(type, suffix) {
  return `${type.replaceAll('-', '_')}_${suffix}.pem`;
}

function toDer(pem) {
  const der = pem.replace(/(?:-----(?:BEGIN|END) (?:PRIVATE|PUBLIC) KEY-----|\s)/g, '');
  return Buffer.alloc(Buffer.byteLength(der, 'base64'), der, 'base64');
}

const keyData = {};

for (const name of ['ML-KEM-512', 'ML-KEM-768', 'ML-KEM-1024']) {
  const lcName = name.toLowerCase();
  keyData[name] = {
    pkcs8_seed_only: toDer(fixtures.readKey(getKeyFileName(lcName, 'private_seed_only'), 'ascii')),
    pkcs8: toDer(fixtures.readKey(getKeyFileName(lcName, 'private'), 'ascii')),
    pkcs8_priv_only: toDer(fixtures.readKey(getKeyFileName(lcName, 'private_priv_only'), 'ascii')),
    spki: toDer(fixtures.readKey(getKeyFileName(lcName, 'public'), 'ascii')),
    jwk: JSON.parse(fixtures.readKey(`${lcName}.json`)),
  };
}

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
        message: /key is not extractable/,
        name: 'InvalidAccessError',
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
        message: /key is not extractable/,
        name: 'InvalidAccessError',
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
        message: /key is not extractable/,
        name: 'InvalidAccessError',
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
  await assert.rejects(
    subtle.importKey(
      'pkcs8',
      keyData[name].pkcs8_priv_only,
      { name },
      extractable,
      privateUsages),
    {
      name: 'NotSupportedError',
      message: 'Importing an ML-KEM PKCS#8 key without a seed is not supported',
    });
}

async function testImportPkcs8MismatchedSeed({ name, privateUsages }, extractable) {
  const modified = Buffer.from(keyData[name].pkcs8);
  modified[30] ^= 0xff;
  await assert.rejects(
    subtle.importKey(
      'pkcs8',
      modified,
      { name },
      extractable,
      privateUsages),
    {
      name: 'DataError',
    });
}

async function testImportRawPublic({ name, publicUsages }, extractable) {
  const jwk = keyData[name].jwk;
  const pub = Buffer.from(jwk.pub, 'base64url');

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
  const jwk = keyData[name].jwk;
  const seed = Buffer.from(jwk.priv, 'base64url');

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

async function testImportJwk({ name, publicUsages, privateUsages }, extractable) {

  const jwk = keyData[name].jwk;

  const tests = [
    subtle.importKey(
      'jwk',
      {
        kty: jwk.kty,
        alg: jwk.alg,
        pub: jwk.pub,
      },
      { name },
      extractable, publicUsages),
    subtle.importKey(
      'jwk',
      jwk,
      { name },
      extractable,
      privateUsages),
  ];

  const [
    publicKey,
    privateKey,
  ] = await Promise.all(tests);

  assert.strictEqual(publicKey.type, 'public');
  assert.strictEqual(privateKey.type, 'private');
  assert.strictEqual(publicKey.extractable, extractable);
  assert.strictEqual(privateKey.extractable, extractable);
  assert.deepStrictEqual(publicKey.usages, publicUsages);
  assert.deepStrictEqual(privateKey.usages, privateUsages);
  assert.strictEqual(publicKey.algorithm.name, name);
  assert.strictEqual(privateKey.algorithm.name, name);
  assert.strictEqual(privateKey.algorithm, privateKey.algorithm);
  assert.strictEqual(privateKey.usages, privateKey.usages);
  assert.strictEqual(publicKey.algorithm, publicKey.algorithm);
  assert.strictEqual(publicKey.usages, publicKey.usages);

  if (extractable) {
    // Test the round trip
    const [
      pubJwk,
      pvtJwk,
    ] = await Promise.all([
      subtle.exportKey('jwk', publicKey),
      subtle.exportKey('jwk', privateKey),
    ]);

    assert.deepStrictEqual(pubJwk.key_ops, publicUsages);
    assert.strictEqual(pubJwk.ext, true);
    assert.strictEqual(pubJwk.kty, 'AKP');
    assert.strictEqual(pubJwk.pub, jwk.pub);

    assert.deepStrictEqual(pvtJwk.key_ops, privateUsages);
    assert.strictEqual(pvtJwk.ext, true);
    assert.strictEqual(pvtJwk.kty, 'AKP');
    assert.strictEqual(pvtJwk.pub, jwk.pub);
    assert.strictEqual(pvtJwk.priv, jwk.priv);

    assert.strictEqual(pubJwk.alg, jwk.alg);
    assert.strictEqual(pvtJwk.alg, jwk.alg);
  } else {
    await assert.rejects(
      subtle.exportKey('jwk', publicKey), {
        message: /key is not extractable/,
        name: 'InvalidAccessError',
      });
    await assert.rejects(
      subtle.exportKey('jwk', privateKey), {
        message: /key is not extractable/,
        name: 'InvalidAccessError',
      });
  }

  await assert.rejects(
    subtle.importKey(
      'jwk',
      { ...jwk, use: 'sig' },
      { name },
      extractable,
      privateUsages),
    { message: 'Invalid JWK "use" Parameter' });

  await assert.rejects(
    subtle.importKey(
      'jwk',
      { ...jwk, pub: undefined },
      { name },
      extractable,
      privateUsages),
    { message: 'Invalid keyData' });

  await assert.rejects(
    subtle.importKey(
      'jwk',
      { ...jwk, priv: 'AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA' }, // Public vs private mismatch
      { name },
      extractable,
      privateUsages),
    { message: 'Invalid keyData' });

  await assert.rejects(
    subtle.importKey(
      'jwk',
      { ...jwk, kty: 'OKP' },
      { name },
      extractable,
      privateUsages),
    { message: 'Invalid JWK "kty" Parameter' });

  await assert.rejects(
    subtle.importKey(
      'jwk',
      { ...jwk },
      { name },
      extractable,
      publicUsages), // Invalid for a private key
    { message: /Unsupported key usage/ });

  await assert.rejects(
    subtle.importKey(
      'jwk',
      { ...jwk, ext: false },
      { name },
      true,
      privateUsages),
    { message: 'JWK "ext" Parameter and extractable mismatch' });

  await assert.rejects(
    subtle.importKey(
      'jwk',
      { ...jwk, priv: undefined },
      { name },
      extractable,
      privateUsages), // Invalid for a public key
    { message: /Unsupported key usage/ });

  for (const alg of [undefined, name === 'ML-KEM-512' ? 'ML-KEM-1024' : 'ML-KEM-512']) {
    await assert.rejects(
      subtle.importKey(
        'jwk',
        { kty: jwk.kty, pub: jwk.pub, alg },
        { name },
        extractable,
        publicUsages),
      { message: 'JWK "alg" Parameter and algorithm name mismatch' });

    await assert.rejects(
      subtle.importKey(
        'jwk',
        { ...jwk, alg },
        { name },
        extractable,
        privateUsages),
      { message: 'JWK "alg" Parameter and algorithm name mismatch' });
  }

  await assert.rejects(
    subtle.importKey(
      'jwk',
      { ...jwk },
      { name },
      extractable,
      [/* empty usages */]),
    { name: 'SyntaxError', message: 'Usages cannot be empty when importing a private key.' });

  await assert.rejects(
    subtle.importKey(
      'jwk',
      { kty: jwk.kty, /* missing pub */ alg: jwk.alg },
      { name },
      extractable,
      publicUsages),
    { name: 'DataError', message: 'Invalid keyData' });
}

(async function() {
  const tests = [];
  for (const vector of testVectors) {
    for (const extractable of [true, false]) {
      tests.push(testImportSpki(vector, extractable));
      tests.push(testImportPkcs8(vector, extractable));
      tests.push(testImportPkcs8SeedOnly(vector, extractable));
      tests.push(testImportPkcs8PrivOnly(vector, extractable));
      tests.push(testImportPkcs8MismatchedSeed(vector, extractable));
      tests.push(testImportJwk(vector, extractable));
      tests.push(testImportRawSeed(vector, extractable));
      tests.push(testImportRawPublic(vector, extractable));
    }
  }
  await Promise.all(tests);
})().then(common.mustCall());

(async function() {
  const alg = 'ML-KEM-512';
  const pub = Buffer.from(keyData[alg].jwk.pub, 'base64url');
  await assert.rejects(subtle.importKey('raw', pub, alg, false, []), {
    name: 'NotSupportedError',
    message: 'Unable to import ML-KEM-512 using raw format',
  });
})().then(common.mustCall());

(async function() {
  for (const { name, privateUsages } of testVectors) {
    const pem = fixtures.readKey(getKeyFileName(name.toLowerCase(), 'private_priv_only'), 'ascii');
    const keyObject = createPrivateKey(pem);
    const key = keyObject.toCryptoKey({ name }, true, privateUsages);
    await assert.rejects(subtle.exportKey('pkcs8', key), (err) => {
      assert.strictEqual(err.name, 'OperationError');
      return true;
    });
  }
})().then(common.mustCall());

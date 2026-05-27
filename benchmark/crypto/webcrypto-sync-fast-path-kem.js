'use strict';

const common = require('../common.js');
const { hasOpenSSL } = require('../../test/common/crypto.js');
const {
  fixtureCryptoKeyPair,
  measureAsync,
} = require('./_webcrypto_sync_fast_path_common.js');

const { subtle } = globalThis.crypto;

const keyFixtures = {};

if (hasOpenSSL(3, 5)) {
  keyFixtures['ml-kem-512'] = {
    publicKeyName: 'ml_kem_512_public',
    privateKeyName: 'ml_kem_512_private',
    algorithm: { name: 'ML-KEM-512' },
  };
  keyFixtures['ml-kem-768'] = {
    publicKeyName: 'ml_kem_768_public',
    privateKeyName: 'ml_kem_768_private',
    algorithm: { name: 'ML-KEM-768' },
  };
  keyFixtures['ml-kem-1024'] = {
    publicKeyName: 'ml_kem_1024_public',
    privateKeyName: 'ml_kem_1024_private',
    algorithm: { name: 'ML-KEM-1024' },
  };
} else if (process.features.openssl_is_boringssl) {
  keyFixtures['ml-kem-768'] = {
    publicKeyName: 'ml_kem_768_public',
    privateKeyName: 'ml_kem_768_private_seed_only',
    algorithm: { name: 'ML-KEM-768' },
  };
  keyFixtures['ml-kem-1024'] = {
    publicKeyName: 'ml_kem_1024_public',
    privateKeyName: 'ml_kem_1024_private_seed_only',
    algorithm: { name: 'ML-KEM-1024' },
  };
}

const algorithms = Object.keys(keyFixtures);

if (algorithms.length === 0) {
  console.log('no supported WebCrypto ML-KEM algorithms available');
  process.exit(0);
}

const bench = common.createBenchmark(main, {
  algorithm: algorithms,
  operation: ['encapsulateBits', 'decapsulateBits'],
  mode: ['serial', 'parallel'],
  n: [1e3],
});

async function setupKemOperation(algorithm, operation) {
  const fixture = keyFixtures[algorithm];
  const pair = fixtureCryptoKeyPair(
    fixture.publicKeyName,
    fixture.privateKeyName,
    fixture.algorithm,
    ['encapsulateBits'],
    ['decapsulateBits']);

  if (operation === 'encapsulateBits') {
    return () => subtle.encapsulateBits(fixture.algorithm, pair.publicKey);
  }

  const { ciphertext } = await subtle.encapsulateBits(fixture.algorithm, pair.publicKey);
  return () => subtle.decapsulateBits(
    fixture.algorithm,
    pair.privateKey,
    ciphertext);
}

async function main({ n, algorithm, operation, mode }) {
  const run = await setupKemOperation(algorithm, operation);
  await measureAsync(bench, n, mode, run);
}

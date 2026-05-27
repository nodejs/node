'use strict';

const common = require('../common.js');
const {
  fixtureCryptoKeyPair,
  measureAsync,
} = require('./_webcrypto_sync_fast_path_common.js');

const { subtle } = globalThis.crypto;

const keyFixtures = {
  'ecdh-p256': {
    publicKeyName: 'ec_p256_public',
    privateKeyName: 'ec_p256_private',
    keyAlgorithm: { name: 'ECDH', namedCurve: 'P-256' },
    deriveAlgorithmName: 'ECDH',
    length: 256,
  },
  'x25519': {
    publicKeyName: 'x25519_public',
    privateKeyName: 'x25519_private',
    keyAlgorithm: { name: 'X25519' },
    deriveAlgorithmName: 'X25519',
    length: 256,
  },
};

if (!process.features.openssl_is_boringssl) {
  keyFixtures.x448 = {
    publicKeyName: 'x448_public',
    privateKeyName: 'x448_private',
    keyAlgorithm: { name: 'X448' },
    deriveAlgorithmName: 'X448',
    length: 448,
  };
}

const algorithms = Object.keys(keyFixtures);

const bench = common.createBenchmark(main, {
  algorithm: algorithms,
  mode: ['serial', 'parallel'],
  n: [1e3],
});

async function setupDeriveOperation(algorithm) {
  const fixture = keyFixtures[algorithm];
  const pair = fixtureCryptoKeyPair(
    fixture.publicKeyName,
    fixture.privateKeyName,
    fixture.keyAlgorithm,
    [],
    ['deriveBits']);
  const params = {
    name: fixture.deriveAlgorithmName,
    public: pair.publicKey,
  };

  return () => subtle.deriveBits(params, pair.privateKey, fixture.length);
}

async function main({ n, algorithm, mode }) {
  const run = await setupDeriveOperation(algorithm);
  await measureAsync(bench, n, mode, run);
}

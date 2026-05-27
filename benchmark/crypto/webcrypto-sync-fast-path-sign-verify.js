'use strict';

const common = require('../common.js');
const {
  fixtureCryptoKeyPair,
  isSupported,
  kThresholdSizeLabels,
  measureAsync,
  ptn,
  thresholdSize,
} = require('./_webcrypto_sync_fast_path_common.js');

const { subtle } = globalThis.crypto;

const keyFixtures = {
  'ecdsa-p256': {
    publicKeyName: 'ec_p256_public',
    privateKeyName: 'ec_p256_private',
    keyAlgorithm: { name: 'ECDSA', namedCurve: 'P-256' },
    signAlgorithm: { name: 'ECDSA', hash: 'SHA-256' },
  },
  'ed25519': {
    publicKeyName: 'ed25519_public',
    privateKeyName: 'ed25519_private',
    keyAlgorithm: { name: 'Ed25519' },
    signAlgorithm: { name: 'Ed25519' },
  },
  'ed448': {
    publicKeyName: 'ed448_public',
    privateKeyName: 'ed448_private',
    keyAlgorithm: { name: 'Ed448' },
    signAlgorithm: { name: 'Ed448' },
  },
};

const algorithms = Object.keys(keyFixtures)
  .filter((name) => isSupported('sign', keyFixtures[name].signAlgorithm));

if (algorithms.length === 0) {
  console.log('no supported WebCrypto sign/verify algorithms available');
  process.exit(0);
}

const bench = common.createBenchmark(main, {
  algorithm: algorithms,
  operation: ['sign', 'verify'],
  size: kThresholdSizeLabels,
  mode: ['serial', 'parallel'],
  n: [1e3],
});

async function setupSignVerifyOperation(algorithm, operation, size) {
  const fixture = keyFixtures[algorithm];
  const pair = fixtureCryptoKeyPair(
    fixture.publicKeyName,
    fixture.privateKeyName,
    fixture.keyAlgorithm,
    ['verify'],
    ['sign']);
  const data = ptn(thresholdSize(size));

  if (operation === 'sign') {
    return () => subtle.sign(fixture.signAlgorithm, pair.privateKey, data);
  }

  const signature = await subtle.sign(fixture.signAlgorithm, pair.privateKey, data);
  return () => subtle.verify(fixture.signAlgorithm, pair.publicKey, signature, data);
}

async function main({ n, algorithm, operation, size, mode }) {
  const run = await setupSignVerifyOperation(algorithm, operation, size);
  await measureAsync(bench, n, mode, run);
}

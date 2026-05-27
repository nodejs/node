'use strict';

const common = require('../common.js');
const {
  importSecretKey,
  isSupported,
  kThresholdSizeLabels,
  kWebCryptoSyncFastPathThreshold,
  measureAsync,
  ptn,
  thresholdSize,
} = require('./_webcrypto_sync_fast_path_common.js');

const { subtle } = globalThis.crypto;

const keyAlgorithms = {
  HMAC: { name: 'HMAC', hash: 'SHA-256' },
  KMAC128: { name: 'KMAC128' },
  KMAC256: { name: 'KMAC256' },
};

const signAlgorithms = {
  HMAC: { name: 'HMAC' },
  KMAC128: { name: 'KMAC128', outputLength: 256 },
  KMAC256: { name: 'KMAC256', outputLength: 256 },
};

const kKmacSyncFastPathThreshold = 16 * 1024;

const algorithms = Object.keys(keyAlgorithms)
  .filter((name) => isSupported('sign', signAlgorithms[name]));

if (algorithms.length === 0) {
  console.log('no supported WebCrypto MAC algorithms available');
  process.exit(0);
}

const bench = common.createBenchmark(main, {
  algorithm: algorithms,
  operation: ['sign', 'verify'],
  size: kThresholdSizeLabels,
  mode: ['serial', 'parallel'],
  n: [1e3],
});

function outputByteLength(algorithm) {
  return algorithm === 'HMAC' ? 32 : signAlgorithms[algorithm].outputLength / 8;
}

function syncFastPathThreshold(algorithm) {
  return algorithm === 'HMAC' ?
    kWebCryptoSyncFastPathThreshold : kKmacSyncFastPathThreshold;
}

function dataSize(algorithm, operation, size) {
  if (size === 'minimal')
    return 1;

  let overhead = 0;
  if (operation === 'verify')
    overhead += outputByteLength(algorithm);
  if (algorithm !== 'HMAC')
    overhead += outputByteLength(algorithm);

  return thresholdSize(size, {
    minimum: overhead + 1,
    threshold: syncFastPathThreshold(algorithm),
  }) - overhead;
}

async function setupMacOperation(algorithm, operation, size) {
  const key = await importSecretKey({
    algorithm: keyAlgorithms[algorithm],
    usages: ['sign', 'verify'],
    length: 32,
  });
  const data = ptn(dataSize(algorithm, operation, size));
  const params = signAlgorithms[algorithm];

  if (operation === 'sign') {
    return () => subtle.sign(params, key, data);
  }

  const signature = await subtle.sign(params, key, data);
  return () => subtle.verify(params, key, signature, data);
}

async function main({ n, algorithm, operation, size, mode }) {
  const run = await setupMacOperation(algorithm, operation, size);
  await measureAsync(bench, n, mode, run);
}

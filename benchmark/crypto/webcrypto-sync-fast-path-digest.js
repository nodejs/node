'use strict';

const common = require('../common.js');
const {
  isSupported,
  kThresholdSizeLabels,
  measureAsync,
  ptn,
  thresholdSize,
} = require('./_webcrypto_sync_fast_path_common.js');

const { subtle } = globalThis.crypto;

function digestAlgorithm(name) {
  switch (name) {
    case 'cSHAKE128':
      return { name, outputLength: 256 };
    case 'cSHAKE256':
      return { name, outputLength: 512 };
    case 'TurboSHAKE128':
      return { name, outputLength: 256 };
    case 'TurboSHAKE256':
      return { name, outputLength: 512 };
    case 'KT128':
      return { name, outputLength: 256 };
    case 'KT256':
      return { name, outputLength: 512 };
    default:
      return name;
  }
}

const algorithms = [
  'SHA-1',
  'SHA-256',
  'SHA-384',
  'SHA-512',
  'SHA3-256',
  'SHA3-384',
  'SHA3-512',
  'cSHAKE128',
  'cSHAKE256',
  'TurboSHAKE128',
  'TurboSHAKE256',
  'KT128',
  'KT256',
].filter((name) => isSupported('digest', digestAlgorithm(name)));

if (algorithms.length === 0) {
  console.log('no supported WebCrypto digest algorithms available');
  process.exit(0);
}

const bench = common.createBenchmark(main, {
  algorithm: algorithms,
  size: kThresholdSizeLabels,
  mode: ['serial', 'parallel'],
  n: [1e3],
});

async function main({ n, algorithm, size, mode }) {
  const data = ptn(thresholdSize(size));
  const params = digestAlgorithm(algorithm);

  await measureAsync(bench, n, mode, () => subtle.digest(params, data));
}

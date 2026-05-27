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
  'AES-CBC': { name: 'AES-CBC', length: 128 },
  'AES-CTR': { name: 'AES-CTR', length: 128 },
  'AES-GCM': { name: 'AES-GCM', length: 128 },
  'AES-OCB': { name: 'AES-OCB', length: 128 },
  'ChaCha20-Poly1305': { name: 'ChaCha20-Poly1305' },
};

const kAesCbcCtrEncryptSyncFastPathThreshold = 32 * 1024;

function supportParams(algorithm) {
  switch (algorithm) {
    case 'AES-CBC':
      return { name: algorithm, iv: new Uint8Array(16) };
    case 'AES-CTR':
      return { name: algorithm, counter: new Uint8Array(16), length: 64 };
    case 'AES-GCM':
      return { name: algorithm, iv: new Uint8Array(12) };
    case 'AES-OCB':
      return { name: algorithm, iv: new Uint8Array(15), tagLength: 128 };
    case 'ChaCha20-Poly1305':
      return { name: algorithm, iv: new Uint8Array(12), tagLength: 128 };
  }
  throw new Error(`Unknown cipher algorithm: ${algorithm}`);
}

const cipherAlgorithms = Object.keys(keyAlgorithms)
  .filter((name) => isSupported('encrypt', supportParams(name)));

if (cipherAlgorithms.length === 0) {
  console.log('no supported WebCrypto cipher algorithms available');
  process.exit(0);
}

const bench = common.createBenchmark(main, {
  algorithm: cipherAlgorithms,
  operation: ['encrypt', 'decrypt'],
  size: kThresholdSizeLabels,
  mode: ['serial', 'parallel'],
  n: [1e3],
});

function cipherParams(algorithm, size) {
  switch (algorithm) {
    case 'AES-CBC':
      return {
        name: algorithm,
        iv: ptn(16),
      };
    case 'AES-CTR':
      return {
        name: algorithm,
        counter: ptn(16),
        length: 64,
      };
    case 'AES-GCM':
      return {
        name: algorithm,
        iv: ptn(12),
        additionalData: ptn(16),
        tagLength: 128,
      };
    case 'AES-OCB':
      return {
        name: algorithm,
        iv: ptn(15),
        additionalData: ptn(16),
        tagLength: 128,
      };
    case 'ChaCha20-Poly1305':
      return {
        name: algorithm,
        iv: ptn(12),
        additionalData: ptn(16),
        tagLength: 128,
      };
  }
  throw new Error(`Unknown cipher algorithm: ${algorithm}`);
}

function syncFastPathThreshold(algorithm, operation) {
  return operation === 'encrypt' &&
    (algorithm === 'AES-CBC' || algorithm === 'AES-CTR') ?
    kAesCbcCtrEncryptSyncFastPathThreshold :
    kWebCryptoSyncFastPathThreshold;
}

function measuredInputOverhead(algorithm, operation) {
  switch (algorithm) {
    case 'AES-GCM':
    case 'AES-OCB':
    case 'ChaCha20-Poly1305':
      return operation === 'decrypt' ? 32 : 16;
    default:
      return 0;
  }
}

function dataSize(algorithm, operation, size) {
  const minimum = algorithm === 'AES-CBC' ? 16 : 1;
  if (size === 'minimal')
    return minimum;

  const overhead = measuredInputOverhead(algorithm, operation);
  const multiple = algorithm === 'AES-CBC' ? 16 : 1;
  return thresholdSize(size, {
    minimum: minimum + overhead,
    multiple,
    threshold: syncFastPathThreshold(algorithm, operation),
  }) - overhead;
}

async function setupCipherOperation(algorithm, operation, size) {
  const key = await importSecretKey({
    algorithm: keyAlgorithms[algorithm],
    usages: ['encrypt', 'decrypt'],
    length: algorithm === 'ChaCha20-Poly1305' ? 32 : 16,
  });
  const data = ptn(dataSize(algorithm, operation, size));

  const params = cipherParams(algorithm, size);
  if (operation === 'encrypt') {
    return () => subtle.encrypt(params, key, data);
  }

  const ciphertext = await subtle.encrypt(params, key, data);
  return () => subtle.decrypt(params, key, ciphertext);
}

async function main({ n, algorithm, operation, size, mode }) {
  const run = await setupCipherOperation(algorithm, operation, size);
  await measureAsync(bench, n, mode, run);
}

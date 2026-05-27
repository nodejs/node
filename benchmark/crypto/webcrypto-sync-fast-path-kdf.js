'use strict';

const common = require('../common.js');
const {
  importSecretKey,
  kThresholdSizeLabels,
  measureAsync,
  ptn,
} = require('./_webcrypto_sync_fast_path_common.js');

const { subtle } = globalThis.crypto;

const kHkdfSyncFastPathThreshold = 16 * 1024;
const kOutputBytes = 32;

const bench = common.createBenchmark(main, {
  algorithm: ['HKDF'],
  size: kThresholdSizeLabels,
  mode: ['serial', 'parallel'],
  n: [1e3],
});

function aggregateSize(label) {
  switch (label) {
    case 'minimal':
      return kOutputBytes;
    case 'middle':
      return kHkdfSyncFastPathThreshold / 2;
    case 'at-threshold':
      return kHkdfSyncFastPathThreshold;
    case 'after-threshold':
      return kHkdfSyncFastPathThreshold + 1;
  }
  throw new Error(`Unknown HKDF size label: ${label}`);
}

function hkdfParams(size) {
  const aggregate = aggregateSize(size);
  const infoLength = aggregate === kOutputBytes ? 0 : 16;
  const saltLength = aggregate - infoLength - kOutputBytes;
  return {
    name: 'HKDF',
    hash: 'SHA-256',
    salt: ptn(saltLength),
    info: ptn(infoLength),
  };
}

async function main({ n, size, mode }) {
  const key = await importSecretKey({
    algorithm: 'HKDF',
    usages: ['deriveBits'],
    length: 32,
  });
  const params = hkdfParams(size);

  await measureAsync(
    bench,
    n,
    mode,
    () => subtle.deriveBits(params, key, kOutputBytes * 8));
}

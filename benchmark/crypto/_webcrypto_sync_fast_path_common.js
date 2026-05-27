'use strict';

const crypto = require('crypto');
const fs = require('fs');
const path = require('path');

const fixturesKeyDir = path.resolve(__dirname, '../../test/fixtures/keys/');

const kWebCryptoSyncFastPathThreshold = 64 * 1024;

const kThresholdSizeLabels = [
  'minimal',
  'middle',
  'at-threshold',
  'after-threshold',
];

const kParallelism = 4;

function ptn(size) {
  const buffer = Buffer.allocUnsafe(size);
  for (let i = 0; i < size; i++) {
    buffer[i] = i % 0xfb;
  }
  return buffer;
}

function thresholdSize(label, {
  minimum = 1,
  multiple = 1,
  threshold = kWebCryptoSyncFastPathThreshold,
} = {}) {
  switch (label) {
    case 'minimal':
      return minimum;
    case 'middle':
      return roundDown(Math.max(minimum, threshold / 2), multiple);
    case 'at-threshold':
      return roundDown(Math.max(minimum, threshold), multiple);
    case 'after-threshold':
      return roundUp(Math.max(minimum, threshold + 1), multiple);
  }
  throw new Error(`Unknown threshold size label: ${label}`);
}

function roundDown(value, multiple) {
  return Math.max(multiple, Math.floor(value / multiple) * multiple);
}

function roundUp(value, multiple) {
  return Math.ceil(value / multiple) * multiple;
}

function readKey(name) {
  return fs.readFileSync(path.resolve(fixturesKeyDir, `${name}.pem`), 'utf8');
}

function fixtureKeyPair(publicKeyName, privateKeyName) {
  return {
    publicKey: readKey(publicKeyName),
    privateKey: readKey(privateKeyName),
  };
}

function fixtureCryptoKeyPair(
  publicKeyName,
  privateKeyName,
  algorithm,
  publicUsages,
  privateUsages,
) {
  return {
    publicKey: crypto.createPublicKey(readKey(publicKeyName))
      .toCryptoKey(algorithm, false, publicUsages),
    privateKey: crypto.createPrivateKey(readKey(privateKeyName))
      .toCryptoKey(algorithm, false, privateUsages),
  };
}

async function importSecretKey({
  algorithm,
  usages,
  length = 32,
  format = 'raw-secret',
  extractable = false,
}) {
  return globalThis.crypto.subtle.importKey(
    format,
    ptn(length),
    algorithm,
    extractable,
    usages);
}

function isSupported(operation, algorithm) {
  if (typeof SubtleCrypto.supports !== 'function')
    return true;
  return SubtleCrypto.supports(operation, algorithm);
}

async function measureAsync(bench, n, mode, fn) {
  if (mode === 'parallel') {
    const parallelism = Math.min(kParallelism, n);
    let started = 0;
    let completed = 0;
    bench.start();
    await new Promise((resolve, reject) => {
      let failed = false;
      function fail(err) {
        failed = true;
        reject(err);
      }

      function launch() {
        if (failed)
          return;

        const i = started++;
        let promise;
        try {
          promise = fn(i);
        } catch (err) {
          fail(err);
          return;
        }

        Promise.resolve(promise).then(() => {
          if (failed)
            return;
          if (++completed === n) {
            resolve();
          } else if (started < n) {
            launch();
          }
        }, fail);
      }

      for (let i = 0; i < parallelism; i++)
        launch();
    });
    bench.end(n);
    return;
  }

  bench.start();
  for (let i = 0; i < n; i++)
    await fn(i);
  bench.end(n);
}

module.exports = {
  fixtureCryptoKeyPair,
  fixtureKeyPair,
  importSecretKey,
  isSupported,
  kThresholdSizeLabels,
  kWebCryptoSyncFastPathThreshold,
  measureAsync,
  ptn,
  readKey,
  thresholdSize,
};

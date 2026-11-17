// Flags: --expose-internals --no-warnings
'use strict';

const common = require('../common');
if (!common.hasCrypto) common.skip('missing crypto');
const { isBoringSSL } = require('../common/crypto');
if (isBoringSSL) common.skip('requires an OpenSSL provider');

const assert = require('node:assert');
const crypto = require('node:crypto');
const { once } = require('node:events');
const { isMainThread, parentPort, Worker, workerData } = require('node:worker_threads');
const { normalizeAlgorithm } = require('internal/crypto/util');
const { subtle } = globalThis.crypto;

if (!isMainThread && !workerData?.supportsFips)
  common.skip('crypto.setFips() is not supported in workers');

async function check() {
  const fips = crypto.getFips() === 1;
  const turbo = { name: 'TurboSHAKE128', outputLength: 128 };
  if (fips) {
    const error = { name: 'NotSupportedError', message: 'Unrecognized algorithm name' };
    assert.throws(() => normalizeAlgorithm(turbo, 'digest'), error);
    await assert.rejects(subtle.digest(turbo, new Uint8Array()), error);
  } else {
    assert.strictEqual(normalizeAlgorithm(turbo, 'digest').name, turbo.name);
    assert.strictEqual((await subtle.digest(turbo, new Uint8Array())).byteLength, 16);
  }
  assert.strictEqual(SubtleCrypto.supports('digest', turbo), !fips);
  for (const [algorithm, usages] of [
    [{ name: 'AES-OCB', length: 128 }, ['encrypt']],
    [{ name: 'X25519' }, ['deriveBits']],
  ]) {
    const supported = SubtleCrypto.supports('generateKey', algorithm);
    const generated = subtle.generateKey(algorithm, true, usages);
    if (supported) {
      await generated;
    } else {
      await assert.rejects(generated, {
        name: 'NotSupportedError', message: 'Unrecognized algorithm name',
      });
    }
  }
  for (const name of ['ECDH', 'ECDSA']) {
    for (const namedCurve of ['P-256', 'P-384', 'P-521']) {
      const algorithm = { name, namedCurve };
      for (const operation of ['generateKey', 'importKey']) {
        assert.strictEqual(SubtleCrypto.supports(operation, algorithm), true);
      }
    }
  }
  const hashes = crypto.getHashes();
  const hashError = { name: 'NotSupportedError', message: 'Unrecognized algorithm name' };
  const rsa = {
    name: 'RSA-PSS', modulusLength: 1024,
    publicExponent: new Uint8Array([1, 0, 1]), hash: 'SHA-256',
  };
  assert.strictEqual(SubtleCrypto.supports('generateKey', rsa), !fips);
  if (fips) {
    // Hash normalization precedes the RSA operation's modulus validation.
    const error = hashes.includes('sha256') ? {
      name: 'OperationError', message: 'algorithm.modulusLength must be at least 2048',
    } : hashError;
    await assert.rejects(subtle.generateKey(rsa, true, ['sign']), error);
  } else {
    assert.strictEqual(normalizeAlgorithm(rsa, 'generateKey').modulusLength, 1024);
  }
  const salt = new Uint8Array(16);
  for (const [name, alias] of [
    ['SHA-1', 'sha1'], ['SHA-256', 'sha256'], ['SHA-384', 'sha384'], ['SHA-512', 'sha512'],
    ['SHA3-256', 'sha3-256'], ['SHA3-384', 'sha3-384'], ['SHA3-512', 'sha3-512'],
  ]) {
    const available = hashes.includes(alias);
    for (const hash of [name, { name }]) {
      const hmac = { name: 'HMAC', hash, length: 256 };
      const hkdf = { name: 'HKDF', hash, salt, info: new Uint8Array() };
      for (const [operations, algorithm] of [
        [['digest'], hash],
        [['generateKey', 'importKey'], { ...rsa, modulusLength: 2048, hash }],
        [['generateKey', 'importKey'], hmac],
        [['sign', 'verify'], { name: 'ECDSA', hash }],
        [['deriveBits'], hkdf],
        [['deriveBits'], { name: 'PBKDF2', hash, salt, iterations: 1 }],
      ]) {
        for (const operation of operations) {
          const length = operation === 'deriveBits' ? 128 : undefined;
          assert.strictEqual(SubtleCrypto.supports(operation, algorithm, length), available);
          if (!available)
            assert.throws(() => normalizeAlgorithm(algorithm, operation), hashError);
        }
      }
      assert.strictEqual(SubtleCrypto.supports('deriveKey', hkdf, hmac), available);
      if (!available)
        assert.throws(() => normalizeAlgorithm(hmac, 'get key length'), hashError);
    }
  }
}

if (!isMainThread) {
  parentPort.on('message', async () => {
    await check();
    parentPort.postMessage(crypto.getFips());
  });
  parentPort.postMessage('ready');
} else {
  (async () => {
    const originalFips = crypto.getFips();
    await check();
    try {
      crypto.setFips(0);
    } catch (err) {
      if (err.code !== 'ERR_CRYPTO_FIPS_FORCED') throw err;
      common.printSkipMessage('FIPS mode cannot be disabled');
      return;
    }
    let worker;
    try {
      // Start the worker before enabling FIPS: isolate initialization can need
      // OpenSSL entropy, which is unavailable without an active FIPS provider.
      worker = new Worker(__filename, { workerData: { supportsFips: true } });
      worker.on('error', common.mustNotCall());
      await once(worker, 'message');
      for (const fips of [0, 1, 0]) {
        crypto.setFips(fips);
        await check();
        const response = once(worker, 'message');
        worker.postMessage('check');
        const [actual] = await response;
        assert.strictEqual(actual, fips);
      }
    } finally {
      if (worker !== undefined) await worker.terminate();
      crypto.setFips(originalFips);
    }
  })().then(common.mustCall());
}

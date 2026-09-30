'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const {
  createPrivateKey,
  createPublicKey,
  encapsulate,
  decapsulate,
} = require('crypto');
const { once } = require('events');
const { promisify } = require('util');
const { Worker, parentPort, workerData } = require('worker_threads');
const { hasOpenSSL, hasFIPS, isBoringSSL } = require('../common/crypto');
const fixtures = require('../common/fixtures');
const { subtle } = globalThis.crypto;

if (!hasOpenSSL(3) && !isBoringSSL)
  common.skip('requires OpenSSL >= 3 or BoringSSL');

const encapsulateAsync = promisify(encapsulate);
const decapsulateAsync = promisify(decapsulate);

async function encapsulateKeys(keys) {
  const operations = Array.from({ length: 4 }, () =>
    encapsulateAsync(keys.publicKey));
  operations.push(encapsulate(keys.publicKey));
  if (keys.publicCryptoKey) {
    operations.push(subtle.encapsulateBits('ML-KEM-768', keys.publicCryptoKey));
  }

  const results = await Promise.all(operations);
  for (const { sharedKey, ciphertext } of results) {
    assert.strictEqual(sharedKey.byteLength, keys.sharedSecretLength);
    assert.strictEqual(ciphertext.byteLength, keys.ciphertextLength);
  }
  return results;
}

async function decapsulateKeys(keys, results) {
  const operations = results.map(common.mustCall(async ({ sharedKey, ciphertext }) => {
    const result = await decapsulateAsync(keys.privateKey, ciphertext);
    assert.deepStrictEqual(result, Buffer.from(sharedKey));
  }, results.length));

  for (const { sharedKey, ciphertext } of results) {
    assert.deepStrictEqual(
      decapsulate(keys.privateKey, ciphertext), Buffer.from(sharedKey));
    if (keys.privateCryptoKey) {
      operations.push(subtle.decapsulateBits(
        'ML-KEM-768', keys.privateCryptoKey, ciphertext,
      ).then((result) => {
        assert.deepStrictEqual(Buffer.from(result), Buffer.from(sharedKey));
      }));
    }
  }

  await Promise.all(operations);
}

function waitForStart(barrier, phase) {
  parentPort.postMessage(phase);
  Atomics.wait(barrier, phase, 0);
}

async function runWorker() {
  const barrier = new Int32Array(workerData.barrier);
  waitForStart(barrier, 0);
  const results = await encapsulateKeys(workerData.keys);
  waitForStart(barrier, 1);
  await decapsulateKeys(workerData.keys, results);
}

async function testKeys(keys) {
  const barrier = new Int32Array(new SharedArrayBuffer(8));
  const workers = Array.from({ length: 3 }, () => new Worker(__filename, {
    workerData: { test: 'kem-shared-key', keys, barrier: barrier.buffer },
  }));
  const exits = workers.map((worker) => once(worker, 'exit'));

  // Synchronize each phase before the backing keys have been used for it.
  const ready = await Promise.all(
    workers.map((worker) => once(worker, 'message')));
  for (const [phase] of ready)
    assert.strictEqual(phase, 0);
  const decapsulateReady = workers.map((worker) => once(worker, 'message'));
  Atomics.store(barrier, 0, 1);
  Atomics.notify(barrier, 0);
  const results = await encapsulateKeys(keys);

  for (const [phase] of await Promise.all(decapsulateReady))
    assert.strictEqual(phase, 1);
  Atomics.store(barrier, 1, 1);
  Atomics.notify(barrier, 1);
  await decapsulateKeys(keys, results);

  for (const [code] of await Promise.all(exits))
    assert.strictEqual(code, 0);
}

async function runMain() {
  const types = [];
  if (hasOpenSSL(3)) {
    types.push(['rsa', 'rsa', 256, 256]);
  }
  if (hasOpenSSL(3, 2) && !hasFIPS(3)) {
    types.push(['ec', 'ec_p256', 32, 65], ['x25519', 'x25519', 32, 32]);
  }
  if (hasOpenSSL(3, 5) || isBoringSSL) {
    types.push(['ml-kem-768', 'ml_kem_768', 32, 1088]);
  }

  for (const [type, prefix, sharedSecretLength, ciphertextLength] of types) {
    const suffix = type === 'rsa' ? '_2048' : '';
    const privateSuffix = type === 'ml-kem-768' ? '_seed_only' : suffix;
    const publicKey = createPublicKey(fixtures.readKey(
      `${prefix}_public${suffix}.pem`));
    const privateKey = createPrivateKey(fixtures.readKey(
      `${prefix}_private${privateSuffix}.pem`));
    const keys = {
      publicKey, privateKey, sharedSecretLength, ciphertextLength,
    };
    if (type === 'ml-kem-768') {
      keys.publicCryptoKey = publicKey.toCryptoKey(
        'ML-KEM-768', false, ['encapsulateBits']);
      keys.privateCryptoKey = privateKey.toCryptoKey(
        'ML-KEM-768', false, ['decapsulateBits']);
    }
    await testKeys(keys);
  }
}

if (workerData?.test === 'kem-shared-key') {
  runWorker().then(common.mustCall());
} else {
  runMain().then(common.mustCall());
}

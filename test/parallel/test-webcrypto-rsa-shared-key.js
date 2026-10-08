'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const {
  createPrivateKey,
  createPublicKey,
  privateDecrypt,
  publicEncrypt,
} = require('crypto');
const { once } = require('events');
const { Worker, parentPort, workerData } = require('worker_threads');
const fixtures = require('../common/fixtures');
const { subtle } = globalThis.crypto;

const algorithm = { name: 'RSA-OAEP', hash: 'SHA-256' };
const label = Buffer.from('shared RSA-OAEP key');
const plaintext = Buffer.from('concurrent key reuse');

async function encrypt({ publicKey, publicCryptoKey }) {
  const operations = Array.from({ length: 4 }, () =>
    subtle.encrypt({ name: 'RSA-OAEP', label }, publicCryptoKey, plaintext));

  // Exercise the synchronous API while the shared CryptoKey jobs are queued.
  operations.push(publicEncrypt({
    key: publicKey,
    oaepHash: 'sha256',
    oaepLabel: label,
  }, plaintext));

  const ciphertexts = await Promise.all(operations);
  for (const ciphertext of ciphertexts)
    assert.strictEqual(ciphertext.byteLength, 256);
  return ciphertexts;
}

async function decrypt({ privateKey, privateCryptoKey }, ciphertexts) {
  const operations = ciphertexts.map((ciphertext) =>
    subtle.decrypt({ name: 'RSA-OAEP', label }, privateCryptoKey, ciphertext));

  for (const ciphertext of ciphertexts) {
    assert.deepStrictEqual(privateDecrypt({
      key: privateKey,
      oaepHash: 'sha256',
      oaepLabel: label,
    }, Buffer.from(ciphertext)), plaintext);
  }

  for (const result of await Promise.all(operations))
    assert.deepStrictEqual(Buffer.from(result), plaintext);
}

function waitForStart(barrier, phase) {
  parentPort.postMessage(phase);
  Atomics.wait(barrier, phase, 0);
}

async function runWorker() {
  const barrier = new Int32Array(workerData.barrier);
  waitForStart(barrier, 0);
  const ciphertexts = await encrypt(workerData.keys);
  waitForStart(barrier, 1);
  await decrypt(workerData.keys, ciphertexts);
}

async function runMain() {
  const publicKey = createPublicKey(fixtures.readKey('rsa_public_2048.pem'));
  const privateKey = createPrivateKey(fixtures.readKey('rsa_private_2048.pem'));
  const keys = {
    publicKey,
    privateKey,
    publicCryptoKey: publicKey.toCryptoKey(algorithm, false, ['encrypt']),
    privateCryptoKey: privateKey.toCryptoKey(algorithm, false, ['decrypt']),
  };
  const barrier = new Int32Array(new SharedArrayBuffer(8));
  const workers = Array.from({ length: 3 }, () => new Worker(__filename, {
    workerData: { test: 'rsa-shared-key', keys, barrier: barrier.buffer },
  }));
  const exits = workers.map((worker) => once(worker, 'exit'));

  // Clones retain the same backing keys. Release every worker together so
  // their first encryption and decryption also exercise cold key state.
  const ready = await Promise.all(
    workers.map((worker) => once(worker, 'message')));
  for (const [phase] of ready)
    assert.strictEqual(phase, 0);
  const decryptReady = workers.map((worker) => once(worker, 'message'));
  Atomics.store(barrier, 0, 1);
  Atomics.notify(barrier, 0);
  const ciphertexts = await encrypt(keys);

  for (const [phase] of await Promise.all(decryptReady))
    assert.strictEqual(phase, 1);
  Atomics.store(barrier, 1, 1);
  Atomics.notify(barrier, 1);
  await decrypt(keys, ciphertexts);

  for (const [code] of await Promise.all(exits))
    assert.strictEqual(code, 0);
}

if (workerData?.test === 'rsa-shared-key') {
  runWorker().then(common.mustCall());
} else {
  runMain().then(common.mustCall());
}

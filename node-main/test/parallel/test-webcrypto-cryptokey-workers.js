'use strict';

// This test ensures that CryptoKey instances can be correctly
// sent to a Worker via postMessage.

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const { subtle } = globalThis.crypto;
const { once } = require('events');

const {
  Worker,
  parentPort,
} = require('worker_threads');

const keyData =
  Buffer.from(
    '000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f', 'hex');

const sig = '13691a79fb55a0417e4d6699a32f91ad29283fa2c1439865cc0632931f4f48dc';

async function doSig(key) {
  const signature = await subtle.sign({
    name: 'HMAC'
  }, key, Buffer.from('some data'));
  assert.strictEqual(Buffer.from(signature).toString('hex'), sig);
}

if (process.env.HAS_STARTED_WORKER) {
  return parentPort.once('message', (key) => {
    assert.strictEqual(key.algorithm.name, 'HMAC');
    doSig(key).then(common.mustCall());
  });
}

// Don't use isMainThread to allow running this test inside a worker.
process.env.HAS_STARTED_WORKER = 1;

(async function() {
  const worker = new Worker(__filename);

  await once(worker, 'online');

  const key = await subtle.importKey(
    'raw',
    keyData,
    { name: 'HMAC', hash: 'SHA-256' },
    true, ['sign', 'verify']);

  worker.postMessage(key);

  await doSig(key);
})().then(common.mustCall());

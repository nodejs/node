'use strict';
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

// Issue https://github.com/nodejs/node/issues/35263
// Description: Test that passing keyobject to worker thread does not crash.
const {
  generateKeySync,
  generateKeyPairSync,
  webcrypto: { subtle },
} = require('crypto');

const { Worker, isMainThread, workerData } = require('worker_threads');

if (isMainThread) {
  (async () => {
    const secretKey = generateKeySync('aes', { length: 128 });
    const { publicKey, privateKey } = generateKeyPairSync('rsa', {
      modulusLength: 1024
    });
    const cryptoKey = await subtle.generateKey(
      { name: 'AES-CBC', length: 128 }, false, ['encrypt']);

    for (const key of [secretKey, publicKey, privateKey, cryptoKey]) {
      new Worker(__filename, { workerData: key });
    }
  })().then(common.mustCall());
} else {
  console.log(workerData);
}

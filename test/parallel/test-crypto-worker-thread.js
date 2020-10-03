'use strict';
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

// Issue https://github.com/nodejs/node/issues/35263
// Description: Test that passing keyobject to worker thread does not crash.
const { createSecretKey } = require('crypto');

const { Worker, isMainThread, workerData } = require('worker_threads');

if (isMainThread) {
  const key = createSecretKey(Buffer.from('hello'));
  new Worker(__filename, { workerData: key });
} else {
  console.log(workerData);
}

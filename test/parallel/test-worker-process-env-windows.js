'use strict';

const common = require('../common');
const { Worker, isMainThread, parentPort } = require('node:worker_threads');
const { strictEqual } = require('node:assert');

// Test for https://github.com/nodejs/node/issues/48955.

if (!common.isWindows) {
  common.skip('this test is Windows-specific.');
}

if (isMainThread) {
  process.env.barkey = 'foovalue';
  strictEqual(process.env.barkey, process.env.BARKEY);

  const worker = new Worker(__filename);
  worker.once('message', (data) => {
    strictEqual(data.barkey, data.BARKEY);
  });
} else {
  parentPort.postMessage({
    barkey: process.env.barkey,
    BARKEY: process.env.BARKEY,
  });
}

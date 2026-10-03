'use strict';

// A file created by mkstemp() must be closed if the worker terminates before
// the result can be delivered.

const common = require('../common');
if (!common.isLinux) common.skip('counts descriptors through /proc/self/fd');

const assert = require('assert');
const fs = require('fs');
const { once } = require('events');
const { Worker } = require('worker_threads');
const tmpdir = require('../common/tmpdir');

tmpdir.refresh();

const countFds = () => fs.readdirSync('/proc/self/fd').length;

(async () => {
  const before = countFds();
  for (const method of ['fs.mkstemp', 'fs.promises.mkstemp']) {
    const worker = new Worker(`
      const fs = require('fs');
      const { parentPort } = require('worker_threads');
      for (let i = 0; i < 64; i++) {
        const result = ${method}(${JSON.stringify(tmpdir.resolve('terminate-'))}, () => {});
        result?.catch(() => {});
      }
      parentPort.postMessage('started');
    `, { eval: true });
    await once(worker, 'message');
    await worker.terminate();
  }
  assert.strictEqual(countFds(), before);
})().then(common.mustCall());

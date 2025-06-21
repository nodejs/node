'use strict';
const common = require('../common');
const assert = require('assert');
const { Worker } = require('worker_threads');
const { once } = require('events');

(async function() {
  const w = new Worker('', { eval: true });

  await once(w, 'exit');
  await assert.rejects(() => w.getHeapSnapshot(), {
    name: 'Error',
    code: 'ERR_WORKER_NOT_RUNNING'
  });
})().then(common.mustCall());

(async function() {
  const worker = new Worker('setInterval(() => {}, 1000);', { eval: true });
  await once(worker, 'online');

  [1, true, [], null, Infinity, NaN].forEach((i) => {
    assert.throws(() => worker.getHeapSnapshot(i), {
      code: 'ERR_INVALID_ARG_TYPE',
      name: 'TypeError',
      message: 'The "options" argument must be of type object.' +
              common.invalidArgTypeHelper(i)
    });
  });
  await worker.terminate();
})().then(common.mustCall());

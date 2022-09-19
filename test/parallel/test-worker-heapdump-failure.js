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

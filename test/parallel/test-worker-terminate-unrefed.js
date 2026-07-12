'use strict';
const common = require('../common');
const { once } = require('events');
const { Worker } = require('worker_threads');

// Test that calling worker.terminate() on an unref()’ed Worker instance
// still resolves the returned Promise.

async function test() {
  const worker = new Worker('setTimeout(() => {}, 1000000);', { eval: true });
  await once(worker, 'online');
  worker.unref();
  await worker.terminate();
}

test().then(common.mustCall());

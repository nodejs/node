'use strict';
const common = require('../common');
const { Worker } = require('worker_threads');

// Check that a Worker that's running another Worker can be terminated.

const worker = new Worker(`
const { Worker, parentPort } = require('worker_threads');
const worker = new Worker('setInterval(() => {}, 10);', { eval: true });
worker.on('online', () => {
  parentPort.postMessage({});
});
`, { eval: true });

worker.on('message', common.mustCall(() => worker.terminate()));

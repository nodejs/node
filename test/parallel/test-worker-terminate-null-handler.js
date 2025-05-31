'use strict';
const common = require('../common');
const assert = require('assert');
const { Worker } = require('worker_threads');

// Test that calling worker.terminate() if kHandler is null should return an
// empty promise that resolves to undefined, even when a callback is passed

const worker = new Worker(`
const { parentPort } = require('worker_threads');
parentPort.postMessage({ hello: 'world' });
`, { eval: true });

process.once('beforeExit', common.mustCall(() => worker.ref()));

worker.on('exit', common.mustCall(() => {
  worker.terminate().then((res) => assert.strictEqual(res, undefined));

}));

worker.unref();

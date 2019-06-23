'use strict';
const common = require('../common');
const assert = require('assert');
const { Worker } = require('worker_threads');

// Test that calling worker.terminate() if kHandler is null should return an
// empty promise that resolves to undefined, even when a callback is passed

const w = new Worker(`
const { parentPort } = require('worker_threads');
parentPort.postMessage({ hello: 'world' });
`, { eval: true });

process.once('beforeExit', common.mustCall(() => {
  console.log('beforeExit');
  w.ref();
}));

w.on('exit', common.mustCall(() => {
  console.log('exit');
  w.terminate().then((returned) => assert.strictEqual(returned, undefined));
  w.terminate(() => null).then(
    (returned) => assert.strictEqual(returned, undefined)
  );
}));

w.unref();

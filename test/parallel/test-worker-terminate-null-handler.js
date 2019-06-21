'use strict';
const assert = require('assert')
const common = require('../common');
const { Worker } = require('worker_threads');

// Test that calling worker.terminate() if kHandler is null should return an
// empty promise that resolves to undefined

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
  assert.strictEqual(w.terminate(() => null), undefined);
  w.terminate().then(returned => assert.strictEqual(returned, undefined));
}));

w.unref();

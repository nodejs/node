'use strict';
const common = require('../common');
const { Worker } = require('worker_threads');

// This test checks that calling unref() on a Worker will unrefs even future
// listeners on the messagePort.
// Refs: https://github.com/nodejs/node/issues/53036
const w = new Worker(`
const { parentPort } = require('worker_threads');
parentPort.on('message', (msg) => {
  parentPort.postMessage(msg);
});
`, { eval: true });

w.on('message', common.mustNotCall());
w.unref();
w.on('message', common.mustNotCall());

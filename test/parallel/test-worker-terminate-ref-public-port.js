'use strict';
const common = require('../common');
const { Worker } = require('worker_threads');

// The actual test here is that the Worker does not keep the main thread
// running after it has been .terminate()’ed.

const w = new Worker(`
const p = require('worker_threads').parentPort;
while(true) p.postMessage({})`, { eval: true });
w.once('message', () => w.terminate());
w.once('exit', common.mustCall());

'use strict';
const common = require('../common');
const { Worker } = require('worker_threads');

// Check that worker.unref() makes the 'exit' event not be emitted, if it is
// the only thing we would otherwise be waiting for.

const w = new Worker('', { eval: true });
w.unref();
w.on('exit', common.mustNotCall());

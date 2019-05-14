'use strict';
const common = require('../common');
const { Worker } = require('worker_threads');

// Check that worker.unref() makes the 'exit' event not be emitted, if it is
// the only thing we would otherwise be waiting for.

// Use `setInterval()` to make sure the worker is alive until the end of the
// event loop turn.
const w = new Worker('setInterval(() => {}, 100);', { eval: true });
w.unref();
w.on('exit', common.mustNotCall());

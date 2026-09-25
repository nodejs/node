// Flags: --abort-on-uncaught-exception
'use strict';
const common = require('../common');
const assert = require('assert');
const { Worker } = require('worker_threads');

// Tests that --abort-on-uncaught-exception does not apply to
// Workers.

const w = new Worker('throw new Error()', { eval: true });
w.on('error', common.mustCall());
w.on('exit', common.mustCall((code) => assert.strictEqual(code, 1)));

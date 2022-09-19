// Flags: --abort-on-uncaught-exception
'use strict';
const common = require('../common');
const assert = require('assert');
const { Worker } = require('worker_threads');

// Tests that --abort-on-uncaught-exception does not apply to
// termination exceptions.

const w = new Worker('while(true);', { eval: true });
w.on('online', common.mustCall(() => w.terminate()));
w.on('exit', common.mustCall((code) => assert.strictEqual(code, 1)));

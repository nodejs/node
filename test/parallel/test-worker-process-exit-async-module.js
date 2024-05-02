'use strict';

const common = require('../common');
const assert = require('assert');
const { Worker } = require('worker_threads');

// Regression for https://github.com/nodejs/node/issues/43182.
const w = new Worker(new URL('data:text/javascript,process.exit(1);await new Promise(()=>{ process.exit(2); })'));
w.on('exit', common.mustCall((code) => {
  assert.strictEqual(code, 1);
}));

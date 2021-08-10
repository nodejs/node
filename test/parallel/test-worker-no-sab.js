// Flags: --no-harmony-sharedarraybuffer

'use strict';

const common = require('../common');
const assert = require('assert');
const { isMainThread, Worker } = require('worker_threads');

// Regression test for https://github.com/nodejs/node/issues/39717.

const w = new Worker(__filename);

w.on('exit', common.mustCall((status) => {
  assert.strictEqual(status, 2);
}));

if (!isMainThread) process.exit(2);

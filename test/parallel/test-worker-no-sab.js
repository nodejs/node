// Flags: --enable-sharedarraybuffer-per-context

'use strict';

const common = require('../common');
const assert = require('assert');
const { Worker } = require('worker_threads');

// Regression test for https://github.com/nodejs/node/issues/39717.

// Do not use isMainThread so that this test itself can be run inside a Worker.
if (!process.env.HAS_STARTED_WORKER) {
  process.env.HAS_STARTED_WORKER = 1;
  const w = new Worker(__filename);

  w.on('exit', common.mustCall((status) => {
    assert.strictEqual(status, 2);
  }));
} else {
  process.exit(2);
}

'use strict';
const common = require('../common');
const assert = require('assert');
const { Worker } = require('worker_threads');

// Do not use isMainThread so that this test itself can be run inside a Worker.
if (!process.env.HAS_STARTED_WORKER) {
  process.env.HAS_STARTED_WORKER = 1;
  const w = new Worker('abc)', { eval: true });
  w.on('message', common.mustNotCall());
  w.on('error', common.mustCall((err) => {
    assert.strictEqual(err.constructor, SyntaxError);
    assert(/SyntaxError/.test(err));
  }));
} else {
  throw new Error('foo');
}

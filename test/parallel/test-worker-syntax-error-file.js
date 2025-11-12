'use strict';
const common = require('../common');
const fixtures = require('../common/fixtures');
const assert = require('assert');
const { Worker } = require('worker_threads');

// Do not use isMainThread so that this test itself can be run inside a Worker.
if (!process.env.HAS_STARTED_WORKER) {
  process.env.HAS_STARTED_WORKER = 1;
  const w = new Worker(fixtures.path('syntax', 'bad_syntax.js'));
  w.on('message', common.mustNotCall());
  w.on('error', common.mustCall((err) => {
    assert.strictEqual(err.constructor, SyntaxError);
    assert.strictEqual(err.name, 'SyntaxError');
  }));
} else {
  throw new Error('foo');
}

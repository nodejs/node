'use strict';
const common = require('../common');
const assert = require('assert');
const { Worker } = require('worker_threads');

// Do not use isMainThread so that this test itself can be run inside a Worker.
if (!process.env.HAS_STARTED_WORKER) {
  process.env.HAS_STARTED_WORKER = 1;
  const w = new Worker(__filename);
  w.on('message', common.mustNotCall());
  w.on('error', common.mustCall((err) => {
    console.log(err.message);
    assert.match(String(err), /^Error: foo$/);
  }));
  w.on('exit', common.mustCall((code) => {
    // uncaughtException is code 1
    assert.strictEqual(code, 1);
  }));
} else {
  // Cannot use common.mustCall as it cannot catch this
  let called = false;
  process.on('exit', (code) => {
    if (!called) {
      called = true;
    } else {
      assert.fail('Exit callback called twice in worker');
    }
  });

  setTimeout(() => assert.fail('Timeout executed after uncaughtException'),
             2000);

  throw new Error('foo');
}

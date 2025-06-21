'use strict';
const common = require('../common');
const fixtures = require('../common/fixtures');
const { Worker } = require('worker_threads');
const assert = require('assert');

// Regression test for https://github.com/nodejs/node/issues/31777:
// stdio operations coming from preload modules should count towards the
// ref count of the internal communication port on the Worker side.

for (let i = 0; i < 10; i++) {
  const w = new Worker('console.log("B");', {
    execArgv: ['--require', fixtures.path('printA.js')],
    eval: true,
    stdout: true
  });
  w.on('exit', common.mustCall(() => {
    assert.strictEqual(w.stdout.read().toString(), 'A\nB\n');
  }));
}

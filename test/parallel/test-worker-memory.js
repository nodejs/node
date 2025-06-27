'use strict';
const common = require('../common');
if (common.isIBMi)
  common.skip('On IBMi, the rss memory always returns zero');

const assert = require('assert');
const util = require('util');
const { Worker } = require('worker_threads');

let numWorkers = +process.env.JOBS || require('os').availableParallelism();
if (numWorkers > 20) {
  // Cap the number of workers at 20 (as an even divisor of 60 used as
  // the total number of workers started) otherwise the test fails on
  // machines with high core counts.
  numWorkers = 20;
}

// Verify that a Worker's memory isn't kept in memory after the thread finishes.

function run(n, done) {
  console.log(`run() called with n=${n} (numWorkers=${numWorkers})`);
  if (n <= 0)
    return done();
  const worker = new Worker(
    'require(\'worker_threads\').parentPort.postMessage(2 + 2)',
    { eval: true });
  worker.on('message', common.mustCall((value) => {
    assert.strictEqual(value, 4);
  }));
  worker.on('exit', common.mustCall(() => {
    run(n - 1, done);
  }));
}

const startStats = process.memoryUsage();
let finished = 0;
for (let i = 0; i < numWorkers; ++i) {
  run(60 / numWorkers, () => {
    console.log(`done() called (finished=${finished})`);
    if (++finished === numWorkers) {
      const finishStats = process.memoryUsage();
      // A typical value for this ratio would be ~1.15.
      // 5 as a upper limit is generous, but the main point is that we
      // don't have the memory of 50 Isolates/Node.js environments just lying
      // around somewhere.
      assert.ok(finishStats.rss / startStats.rss < 5,
                'Unexpected memory overhead: ' +
                util.inspect([startStats, finishStats]));
    }
  });
}

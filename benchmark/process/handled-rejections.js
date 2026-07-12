'use strict';

const common = require('../common.js');

// Benchmarks the throughput of processing many promise rejections that are
// initially unhandled, get warned, and then handled asynchronously, exercising
// asyncHandledRejections + processPromiseRejections.
//
// Note: This benchmark uses --unhandled-rejections=warn to avoid crashing
// when promises are temporarily unhandled.

const bench = common.createBenchmark(main, {
  n: [1e4, 5e4, 1e5],
}, {
  flags: ['--unhandled-rejections=warn'],
});

function main({ n }) {
  const rejections = [];

  // Suppress warning output during the benchmark
  process.removeAllListeners('warning');

  for (let i = 0; i < n; i++) {
    rejections.push(Promise.reject(i));
  }

  // Wait for them to be processed as unhandled and warned.
  setImmediate(() => {
    setImmediate(() => {
      bench.start();

      for (let i = 0; i < n; i++) {
        rejections[i].catch(() => {});
      }

      // Let processPromiseRejections drain asyncHandledRejections.
      setImmediate(() => {
        bench.end(n);
      });
    });
  });
}

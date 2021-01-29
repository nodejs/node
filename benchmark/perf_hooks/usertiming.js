'use strict';

const common = require('../common.js');

const {
  PerformanceObserver,
  performance,
} = require('perf_hooks');

const bench = common.createBenchmark(main, {
  n: [1e5]
});

function test() {
  performance.mark('a');
  setImmediate(() => {
    performance.mark('b');
    performance.measure('a to b', 'a', 'b');
  });
}

function main({ n }) {
  const obs = new PerformanceObserver(() => {
    bench.end(n);
  });
  obs.observe({ entryTypes: ['measure'], buffered: true });

  bench.start();
  for (let i = 0; i < n; i++)
    test();
}

'use strict';

const common = require('../common');
const {
  PerformanceObserver,
  performance
} = require('perf_hooks');

const bench = common.createBenchmark(main, {
  n: [1e6],
  buffered: ['true', 'false'],
  mark: ['true', 'false']
});

function marked(n, buffered) {
  bench.start();
  const obs = new PerformanceObserver(() => {});
  obs.observe({ entryTypes: ['mark', 'measure'], buffered });
  performance.mark('A');
  for (let i = 0; i < n; i++) {
    performance.mark('B');
    performance.measure('A to B', 'A', 'B');
  }
  bench.end(n);
}

function unmarked(n) {
  bench.start();
  for (let i = 0; i < n; i++) {
    // Do nothing.
  }
  bench.end(n);
}

function main({ n, buffered, mark }) {
  if (mark === 'true') marked(n, buffered === 'true');
  else unmarked(n);
}

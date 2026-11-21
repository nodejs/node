'use strict';
const common = require('../common.js');

const bench = common.createBenchmark(main, {
  n: [1e6],
  listeners: [0, 1],
});

function main({ n, listeners }) {
  const reason = new Error('reason');
  const noop = () => {};

  bench.start();
  for (let i = 0; i < n; i++) {
    const ac = new AbortController();
    if (listeners > 0)
      ac.signal.addEventListener('abort', noop);
    ac.abort(reason);
  }
  bench.end(n);
}

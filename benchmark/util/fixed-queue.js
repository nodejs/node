'use strict';

const common = require('../common');

const bench = common.createBenchmark(main, {
  n: [1e5],
}, { flags: ['--expose-internals'] });

function main({ n }) {
  const FixedQueue = require('internal/fixed_queue');
  const queue = new FixedQueue();
  bench.start();
  for (let i = 0; i < n; i++)
    queue.push(i);
  for (let i = 0; i < n; i++)
    queue.shift();
  bench.end(n);
}

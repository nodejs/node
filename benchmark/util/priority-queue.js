'use strict';

const common = require('../common');

const bench = common.createBenchmark(main, {
  n: [1e5]
}, { flags: ['--expose-internals'] });

function main({ n, type }) {
  const PriorityQueue = require('internal/priority_queue');
  const queue = new PriorityQueue();
  bench.start();
  for (var i = 0; i < n; i++)
    queue.insert(Math.random() * 1e7 | 0);
  for (i = 0; i < n; i++)
    queue.shift();
  bench.end(n);
}

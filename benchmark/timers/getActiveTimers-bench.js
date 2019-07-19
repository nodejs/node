'use strict';
const common = require('../common.js');
const { getActiveTimers } = require('timers');

const bench = common.createBenchmark(main, {
  n: [1e5],
  default_value: [1, 0],
});

function main({ n, default_value }) {
  for (let i = 1; i <= n; i++) {
    setTimeout(() => {}, (default_value ? i : 10));
  }

  bench.start();
  const timers = getActiveTimers();
  bench.end(n);

  for (var j = 0; j < n; j++) {
    clearTimeout(timers[j]);
  }
}

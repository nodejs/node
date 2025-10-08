'use strict';

const { createBenchmark } = require('../common.js');
const bench = createBenchmark(main, {
  n: 1_000_000,
});

function main({ n }) {
  bench.start();
  let sum = 0;
  for (let i = 0; i < n; i++) {
    const result = Math.sin(Math.PI * i) * Math.cos(Math.PI * i);
    sum += result;  // eslint-disable-line no-unused-vars
  }
  bench.end(n);
}

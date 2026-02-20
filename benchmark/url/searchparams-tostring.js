'use strict';

const common = require('../common.js');

const bench = common.createBenchmark(main, {
  length: [5, 500],
  encode: ['true', 'false'],
  count: [1, 1000],
  n: [1e5],
});

function main({ n, length, encode, count }) {
  const char = encode === 'true' ? '\u00A5' : 'a';
  const key = char.repeat(length);
  const value = char.repeat(length);
  const params = [];
  for (let i = 0; i < count; i++) {
    params.push([key, value]);
  }
  const searchParams = new URLSearchParams(params);

  // Warm up
  searchParams.toString();

  bench.start();
  for (let i = 0; i < n; i++) {
    searchParams.toString();
  }
  bench.end(n);
}

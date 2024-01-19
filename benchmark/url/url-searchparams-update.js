'use strict';
const common = require('../common.js');

const bench = common.createBenchmark(main, {
  searchParams: ['true', 'false'],
  property: ['pathname', 'search'],
  n: [1e6],
});

function main({ searchParams, property, n }) {
  const url = new URL('https://nodejs.org');
  if (searchParams === 'true') url.searchParams; // eslint-disable-line no-unused-expressions

  const method = property === 'pathname' ?
    (x) => url.pathname = `/${x}` :
    (x) => url.search = `?${x}`;

  bench.start();
  for (let i = 0; i < n; i++) {
    method(i);
  }
  bench.end(n);
}

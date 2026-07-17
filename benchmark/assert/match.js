'use strict';

const common = require('../common.js');
const assert = require('assert');

const bench = common.createBenchmark(main, {
  n: [2e7],
  method: ['match', 'doesNotMatch'],
}, {
  combinationFilter(p) {
    // These benchmarks purposefully do not run by default. They do not provide
    // might insight, due to only being a small wrapper around a native regexp
    // call.
    return p.n === 1;
  },
});

function main({ n, method }) {
  const fn = assert[method];
  const actual = 'Example of string that will match';
  const expected = method === 'match' ? /will match/ : /will not match/;

  bench.start();
  for (let i = 0; i < n; ++i) {
    fn(actual, expected);
  }
  bench.end(n);
}

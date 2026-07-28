'use strict';

const common = require('../common.js');
const assert = require('assert');

const bench = common.createBenchmark(main, {
  n: [2e5],
  method: ['throws', 'doesNotThrow'],
}, {
  combinationFilter(p) {
    // These benchmarks purposefully do not run by default. They do not provide
    // much insight, due to only being a small wrapper around a try / catch.
    return p.n === 1;
  },
});

function main({ n, method }) {
  const fn = assert[method];
  const shouldThrow = method === 'throws';

  bench.start();
  for (let i = 0; i < n; ++i) {
    fn(() => {
      const err = new Error(`assert.${method}`);
      if (shouldThrow) {
        throw err;
      } else {
        return err;
      }
    });
  }
  bench.end(n);
}

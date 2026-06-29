'use strict';

const common = require('../common.js');
const assert = require('assert');

const bench = common.createBenchmark(main, {
  n: [2e5],
  method: ['rejects', 'doesNotReject'],
}, {
  combinationFilter(p) {
    // These benchmarks purposefully do not run by default. They do not provide
    // much insight, due to only being a small wrapper around a native promise
    // with a few extra checks.
    return p.n === 1;
  },
});

async function main({ n, method }) {
  const fn = assert[method];
  const shouldReject = method === 'rejects';

  bench.start();
  for (let i = 0; i < n; ++i) {
    await fn(async () => {
      const err = new Error(`assert.${method}`);
      if (shouldReject) {
        throw err;
      } else {
        return err;
      }
    });
  }
  bench.end(n);
}

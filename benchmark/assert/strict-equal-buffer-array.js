'use strict';
const common = require('../common.js');
const assert = require('assert');

const bench = common.createBenchmark(main, {
  n: [100],
  size: [1000, 8000],
});

function main({ n, size }) {
  const buffer = Buffer.alloc(size);

  bench.start();
  for (let i = 0; i < n; ++i) {
    new assert.AssertionError({
      actual: buffer,
      expected: [buffer],
      operator: 'strictEqual',
      stackStartFunction: () => {},
    });
  }
  bench.end(n);
}

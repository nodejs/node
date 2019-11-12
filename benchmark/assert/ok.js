'use strict';

const common = require('../common.js');
const assert = require('assert');

const bench = common.createBenchmark(main, { n: [1e5] });

function main({ n }) {
  bench.start();
  for (let i = 0; i < n; ++i) {
    if (i % 2 === 0)
      assert(true);
    else
      assert(true, 'foo bar baz');
  }
  bench.end(n);
}

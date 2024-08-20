'use strict';

const common = require('../common');
const util = require('node:util');
const assert = require('node:assert');

const bench = common.createBenchmark(main, {
  n: [1e6],
});

function main({ n }) {
  bench.start();
  let lastStack = {};
  for (let i = 0; i < n; i++) {
    const stack = util.getCallSite();
    lastStack = stack;
  }
  bench.end(n);
  // Attempt to avoid dead-code elimination
  assert.ok(lastStack);
}

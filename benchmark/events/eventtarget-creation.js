'use strict';

const common = require('../common.js');
const assert = require('node:assert');

const bench = common.createBenchmark(main, {
  n: [1e6],
}, { flags: ['--expose-internals'] });

function main({ n }) {
  const { EventTarget } = require('internal/event_target');

  let target;

  bench.start();
  for (let i = 0; i < n; i++) {
    target = new EventTarget();
  }
  bench.end(n);

  // Avoid V8 deadcode (elimination)
  assert.ok(target);
}

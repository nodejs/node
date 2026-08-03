'use strict';

const common = require('../common.js');
const assert = require('assert').ok;
const { performance } = require('perf_hooks');
const { nodeTiming, eventLoopUtilization } = performance;

const bench = common.createBenchmark(main, {
  n: [1e6],
  method: [
    'idleTime',
    'ELU_simple',
    'ELU_passed',
  ],
});

function main({ method, n }) {
  switch (method) {
    case 'idleTime':
      benchIdleTime(n);
      break;
    case 'ELU_simple':
      benchELUSimple(n);
      break;
    case 'ELU_passed':
      benchELUPassed(n);
      break;
    default:
      throw new Error(`Unsupported method ${method}`);
  }
}

function benchIdleTime(n) {
  bench.start();
  for (let i = 0; i < n; i++)
    nodeTiming.idleTime; // eslint-disable-line no-unused-expressions
  bench.end(n);
}

function benchELUSimple(n) {
  // Need to put this in setImmediate or will always return 0.
  setImmediate(() => {
    const elu = eventLoopUtilization();
    assert(elu.active + elu.idle > 0);

    bench.start();
    for (let i = 0; i < n; i++)
      eventLoopUtilization();
    bench.end(n);
  });
}

function benchELUPassed(n) {
  // Need to put this in setImmediate or will always return 0.
  setImmediate(() => {
    let elu = eventLoopUtilization();
    assert(elu.active + elu.idle > 0);

    bench.start();
    for (let i = 0; i < n; i++)
      elu = eventLoopUtilization(elu);
    bench.end(n);
  });
}

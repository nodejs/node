'use strict';
const common = require('../common.js');
const assert = require('node:assert');

const bench = common.createBenchmark(main, {
  n: [1000],
  fanout: [0, 1, 16, 64],
});

function main({ n, fanout }) {
  const reason = new Error('reason');
  const controllers = [];
  const signals = [];

  for (let i = 0; i < n; i++) {
    const controller = new AbortController();
    controllers[i] = controller;
    for (let j = 0; j < fanout; j++) {
      signals[signals.length] = AbortSignal.any([controller.signal]);
    }
  }

  bench.start();
  for (const controller of controllers) {
    controller.abort(reason);
  }
  bench.end(n);

  assert.strictEqual(signals.length, n * fanout);
  assert(signals.every((signal) => signal.aborted));
}

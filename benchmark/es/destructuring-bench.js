'use strict';

const common = require('../common.js');
const assert = require('assert');

const bench = common.createBenchmark(main, {
  method: ['swap', 'destructure'],
  n: [1e8]
});

function runSwapManual(n) {
  let x, y, r;
  bench.start();
  for (let i = 0; i < n; i++) {
    x = 1, y = 2;
    r = x;
    x = y;
    y = r;
    assert.strictEqual(x, 2);
    assert.strictEqual(y, 1);
  }
  bench.end(n);
}

function runSwapDestructured(n) {
  let x, y;
  bench.start();
  for (let i = 0; i < n; i++) {
    x = 1, y = 2;
    [x, y] = [y, x];
    assert.strictEqual(x, 2);
    assert.strictEqual(y, 1);
  }
  bench.end(n);
}

function main({ n, method }) {
  switch (method) {
    case 'swap':
      runSwapManual(n);
      break;
    case 'destructure':
      runSwapDestructured(n);
      break;
    default:
      throw new Error(`Unexpected method "${method}"`);
  }
}

'use strict';

const common = require('../common.js');
const assert = require('assert');

const bench = common.createBenchmark(main, {
  method: ['swap', 'destructure'],
  millions: [100]
});

function runSwapManual(n) {
  var x, y, r;
  bench.start();
  for (var i = 0; i < n; i++) {
    x = 1, y = 2;
    r = x;
    x = y;
    y = r;
    assert.strictEqual(x, 2);
    assert.strictEqual(y, 1);
  }
  bench.end(n / 1e6);
}

function runSwapDestructured(n) {
  var x, y;
  bench.start();
  for (var i = 0; i < n; i++) {
    x = 1, y = 2;
    [x, y] = [y, x];
    assert.strictEqual(x, 2);
    assert.strictEqual(y, 1);
  }
  bench.end(n / 1e6);
}

function main({ millions, method }) {
  const n = millions * 1e6;

  switch (method) {
    case '':
      // Empty string falls through to next line as default, mostly for tests.
    case 'swap':
      runSwapManual(n);
      break;
    case 'destructure':
      runSwapDestructured(n);
      break;
    default:
      throw new Error('Unexpected method');
  }
}

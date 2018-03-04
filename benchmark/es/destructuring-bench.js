'use strict';

const common = require('../common.js');
const assert = require('assert');

const bench = common.createBenchmark(main, {
  method: ['swap', 'destructure'],
  millions: [100],
});

function runSwapManual(millions) {
  var x, y, r;
  bench.start();
  for (var i = 0; i < millions * 1e6; i++) {
    x = 1, y = 2;
    r = x;
    x = y;
    y = r;
    assert.strictEqual(x, 2);
    assert.strictEqual(y, 1);
  }
  bench.end(millions);
}

function runSwapDestructured(millions) {
  var x, y;
  bench.start();
  for (var i = 0; i < millions * 1e6; i++) {
    x = 1, y = 2;
    [x, y] = [y, x];
    assert.strictEqual(x, 2);
    assert.strictEqual(y, 1);
  }
  bench.end(millions);
}

function main({ millions, method }) {
  switch (method) {
    case '':
      // Empty string falls through to next line as default, mostly for tests.
    case 'swap':
      runSwapManual(millions);
      break;
    case 'destructure':
      runSwapDestructured(millions);
      break;
    default:
      throw new Error(`Unexpected method "${method}"`);
  }
}

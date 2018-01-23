'use strict';

const common = require('../common.js');
const assert = require('assert');

const bench = common.createBenchmark(main, {
  method: ['withoutdefaults', 'withdefaults'],
  millions: [100]
});

function oldStyleDefaults(x, y) {
  x = x || 1;
  y = y || 2;
  assert.strictEqual(x, 1);
  assert.strictEqual(y, 2);
}

function defaultParams(x = 1, y = 2) {
  assert.strictEqual(x, 1);
  assert.strictEqual(y, 2);
}

function runOldStyleDefaults(millions) {
  bench.start();
  for (var i = 0; i < millions * 1e6; i++)
    oldStyleDefaults();
  bench.end(millions);
}

function runDefaultParams(millions) {
  bench.start();
  for (var i = 0; i < millions * 1e6; i++)
    defaultParams();
  bench.end(millions);
}

function main({ millions, method }) {
  switch (method) {
    case '':
      // Empty string falls through to next line as default, mostly for tests.
    case 'withoutdefaults':
      runOldStyleDefaults(millions);
      break;
    case 'withdefaults':
      runDefaultParams(millions);
      break;
    default:
      throw new Error(`Unexpected method "${method}"`);
  }
}

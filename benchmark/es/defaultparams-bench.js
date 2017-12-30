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

function runOldStyleDefaults(n) {

  var i = 0;
  bench.start();
  for (; i < n; i++)
    oldStyleDefaults();
  bench.end(n / 1e6);
}

function runDefaultParams(n) {

  var i = 0;
  bench.start();
  for (; i < n; i++)
    defaultParams();
  bench.end(n / 1e6);
}

function main({ millions, method }) {
  const n = millions * 1e6;

  switch (method) {
    case '':
      // Empty string falls through to next line as default, mostly for tests.
    case 'withoutdefaults':
      runOldStyleDefaults(n);
      break;
    case 'withdefaults':
      runDefaultParams(n);
      break;
    default:
      throw new Error('Unexpected method');
  }
}

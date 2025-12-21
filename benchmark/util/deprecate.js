'use strict';

const common = require('../common');
const assert = require('assert');

const bench = common.createBenchmark(main, {
  n: [1e5],
  modifyPrototype: [1, 0],
  emitWarningSync: [1, 0],
}, {
  flags: ['--expose-internals'],
});

function simpleFunction(x) {
  return x * 2 + (new Array(1000)).fill(0).map((_, i) => i).reduce((a, b) => a + b, 0);
}

function main({ n, modifyPrototype, emitWarningSync }) {
  const { deprecate } = require('internal/util');

  const fn = deprecate(
    simpleFunction,
    'This function is deprecated',
    'DEP0000',
    emitWarningSync,
    !!modifyPrototype,
  );

  let sum = 0;
  bench.start();
  for (let i = 0; i < n; ++i) {
    sum += fn(i);
  }
  bench.end(n);
  assert.ok(sum);
}

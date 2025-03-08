'use strict';
const common = require('../common.js');
const assert = require('assert');

const bench = common.createBenchmark(main, {
  n: [10, 50, 200, 500],
  size: [10, 100],
  datasetName: ['objects'],
});

const baseObject = {
  a: 1,
  b: {
    c: 2,
    d: [3, 4, 5],
    e: 'fghi',
    j: {
      k: 6,
    },
  },
};

function createObjects(size) {
  return Array.from({ length: size }, () => baseObject);
}

function main({ n, size }) {
  bench.start();
  for (let i = 0; i < n; ++i) {
    new assert.AssertionError({
      actual: {},
      expected: createObjects(size),
      operator: 'partialDeepStrictEqual',
      stackStartFunction: () => {},
    });
  }
  bench.end(n);
}

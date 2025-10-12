'use strict';

const common = require('../common.js');
const assert = require('assert');

const bench = common.createBenchmark(main, {
  n: [2e5],
  type: ['string', 'object', 'number'],
  method: ['strictEqual', 'notStrictEqual'],
}, {
  combinationFilter(p) {
    // These benchmarks purposefully do not run by default. They do not provide
    // much insight, due to only being a small wrapper around `Object.is()`.
    return p.n === 1;
  },
});

function main({ type, n, method }) {
  const fn = assert[method];
  let actual, expected;
  switch (type) {
    case 'string':
      actual = expected = 'Hello World';
      if (method === 'notStrictEqual') {
        expected += 'bar';
      }
      break;
    case 'object':
      actual = expected = { a: 'Hello', b: 'World' };
      if (method === 'notStrictEqual') {
        expected = { a: 'Hello', b: 'World' };
      }
      break;
    case 'number':
      actual = expected = 1e9;
      if (method === 'notStrictEqual') {
        expected += 1;
      }
      break;
    default:
      throw new Error('Unexpected type');
  }

  bench.start();
  for (let i = 0; i < n; ++i) {
    fn(actual, expected);
  }
  bench.end(n);
}

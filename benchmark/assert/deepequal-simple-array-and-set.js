'use strict';

const common = require('../common.js');
const { deepEqual, deepStrictEqual, notDeepEqual, notDeepStrictEqual } =
  require('assert');

const bench = common.createBenchmark(main, {
  n: [5e2],
  len: [1e4],
  strict: [1],
  method: [
    'deepEqual_Array',
    'notDeepEqual_Array',
    'deepEqual_Set',
    'notDeepEqual_Set',
  ],
});

function run(fn, n, actual, expected) {
  bench.start();
  for (let i = 0; i < n; ++i) {
    fn(actual, expected);
  }
  bench.end(n);
}

function main({ n, len, method, strict }) {
  const actual = [];
  const expected = [];

  for (let i = 0; i < len; i++) {
    actual.push(i);
    expected.push(i);
  }
  if (method.includes('not')) {
    expected[len - 1] += 1;
  }

  switch (method) {
    case 'deepEqual_Array':
      run(strict ? deepStrictEqual : deepEqual, n, actual, expected);
      break;
    case 'notDeepEqual_Array':
      run(strict ? notDeepStrictEqual : notDeepEqual, n, actual, expected);
      break;
    case 'deepEqual_Set':
      run(strict ? deepStrictEqual : deepEqual,
          n, new Set(actual), new Set(expected));
      break;
    case 'notDeepEqual_Set':
      run(strict ? notDeepStrictEqual : notDeepEqual,
          n, new Set(actual), new Set(expected));
      break;
    default:
      throw new Error(`Unsupported method "${method}"`);
  }
}

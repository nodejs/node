'use strict';

const common = require('../common.js');
const { deepEqual, deepStrictEqual, notDeepEqual, notDeepStrictEqual } =
  require('assert');

const bench = common.createBenchmark(main, {
  n: [1e3],
  len: [1e4],
  strict: [1],
  method: [
    'deepEqual_Array',
    'notDeepEqual_Array',
    'deepEqual_sparseArray',
    'notDeepEqual_sparseArray',
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
  let actual = Array.from({ length: len }, (_, i) => i);
  // Contain one undefined value to trigger a specific code path
  actual[0] = undefined;
  let expected = actual.slice(0);

  if (method.includes('not')) {
    expected[len - 1] += 1;
  }

  switch (method) {
    case 'deepEqual_sparseArray':
    case 'notDeepEqual_sparseArray':
      actual = new Array(len);
      for (let i = 0; i < len; i += 2) {
        actual[i] = i;
      }
      expected = actual.slice(0);
      if (method.includes('not')) {
        expected[len - 2] += 1;
        run(strict ? notDeepStrictEqual : notDeepEqual, n, actual, expected);
      } else {
        run(strict ? deepStrictEqual : deepEqual, n, actual, expected);
      }
      break;
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

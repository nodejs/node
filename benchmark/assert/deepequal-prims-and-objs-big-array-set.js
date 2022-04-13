'use strict';

const common = require('../common.js');
const { deepEqual, deepStrictEqual, notDeepEqual, notDeepStrictEqual } =
  require('assert');

const primValues = {
  'string': 'a',
  'number': 1,
  'object': { 0: 'a' },
  'array': [1, 2, 3],
};

const bench = common.createBenchmark(main, {
  primitive: Object.keys(primValues),
  n: [25],
  len: [2e4],
  strict: [0, 1],
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

function main({ n, len, primitive, method, strict }) {
  const prim = primValues[primitive];
  const actual = [];
  const expected = [];
  const expectedWrong = [];

  for (let x = 0; x < len; x++) {
    actual.push(prim);
    expected.push(prim);
    expectedWrong.push(prim);
  }
  expectedWrong.pop();
  expectedWrong.push('b');

  // Note: primitives are only added once to a set
  const actualSet = new Set(actual);
  const expectedSet = new Set(expected);
  const expectedWrongSet = new Set(expectedWrong);

  switch (method) {
    case 'deepEqual_Array':
      run(strict ? deepStrictEqual : deepEqual, n, actual, expected);
      break;
    case 'notDeepEqual_Array':
      run(strict ? notDeepStrictEqual : notDeepEqual, n, actual, expectedWrong);
      break;
    case 'deepEqual_Set':
      run(strict ? deepStrictEqual : deepEqual, n, actualSet, expectedSet);
      break;
    case 'notDeepEqual_Set':
      run(strict ? notDeepStrictEqual : notDeepEqual,
          n, actualSet, expectedWrongSet);
      break;
    default:
      throw new Error(`Unsupported method "${method}"`);
  }
}

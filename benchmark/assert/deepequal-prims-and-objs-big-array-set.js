'use strict';

const common = require('../common.js');
const { deepEqual, deepStrictEqual, notDeepEqual, notDeepStrictEqual } =
  require('assert');

const primValues = {
  'null': null,
  'undefined': undefined,
  'string': 'a',
  'number': 1,
  'boolean': true,
  'object': { 0: 'a' },
  'array': [1, 2, 3],
  'new-array': new Array([1, 2, 3])
};

const bench = common.createBenchmark(main, {
  primitive: Object.keys(primValues),
  n: [25],
  len: [1e5],
  method: [
    'deepEqual_Array',
    'deepStrictEqual_Array',
    'notDeepEqual_Array',
    'notDeepStrictEqual_Array',
    'deepEqual_Set',
    'deepStrictEqual_Set',
    'notDeepEqual_Set',
    'notDeepStrictEqual_Set'
  ]
});

function run(fn, n, actual, expected) {
  bench.start();
  for (var i = 0; i < n; ++i) {
    fn(actual, expected);
  }
  bench.end(n);
}

function main({ n, len, primitive, method }) {
  const prim = primValues[primitive];
  const actual = [];
  const expected = [];
  const expectedWrong = [];

  for (var x = 0; x < len; x++) {
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
    // Empty string falls through to next line as default, mostly for tests.
    case '':
    case 'deepEqual_Array':
      run(deepEqual, n, actual, expected);
      break;
    case 'deepStrictEqual_Array':
      run(deepStrictEqual, n, actual, expected);
      break;
    case 'notDeepEqual_Array':
      run(notDeepEqual, n, actual, expectedWrong);
      break;
    case 'notDeepStrictEqual_Array':
      run(notDeepStrictEqual, n, actual, expectedWrong);
      break;
    case 'deepEqual_Set':
      run(deepEqual, n, actualSet, expectedSet);
      break;
    case 'deepStrictEqual_Set':
      run(deepStrictEqual, n, actualSet, expectedSet);
      break;
    case 'notDeepEqual_Set':
      run(notDeepEqual, n, actualSet, expectedWrongSet);
      break;
    case 'notDeepStrictEqual_Set':
      run(notDeepStrictEqual, n, actualSet, expectedWrongSet);
      break;
    default:
      throw new Error(`Unsupported method "${method}"`);
  }
}

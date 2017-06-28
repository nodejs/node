'use strict';

const common = require('../common.js');
const assert = require('assert');

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
  prim: Object.keys(primValues),
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

function main(conf) {
  const prim = primValues[conf.prim];
  const n = +conf.n;
  const len = +conf.len;
  const actual = [];
  const expected = [];
  const expectedWrong = [];
  var i;

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

  switch (conf.method) {
    case 'deepEqual_Array':
      bench.start();
      for (i = 0; i < n; ++i) {
        // eslint-disable-next-line no-restricted-properties
        assert.deepEqual(actual, expected);
      }
      bench.end(n);
      break;
    case 'deepStrictEqual_Array':
      bench.start();
      for (i = 0; i < n; ++i) {
        assert.deepStrictEqual(actual, expected);
      }
      bench.end(n);
      break;
    case 'notDeepEqual_Array':
      bench.start();
      for (i = 0; i < n; ++i) {
        // eslint-disable-next-line no-restricted-properties
        assert.notDeepEqual(actual, expectedWrong);
      }
      bench.end(n);
      break;
    case 'notDeepStrictEqual_Array':
      bench.start();
      for (i = 0; i < n; ++i) {
        assert.notDeepStrictEqual(actual, expectedWrong);
      }
      bench.end(n);
      break;
    case 'deepEqual_Set':
      bench.start();
      for (i = 0; i < n; ++i) {
        // eslint-disable-next-line no-restricted-properties
        assert.deepEqual(actualSet, expectedSet);
      }
      bench.end(n);
      break;
    case 'deepStrictEqual_Set':
      bench.start();
      for (i = 0; i < n; ++i) {
        assert.deepStrictEqual(actualSet, expectedSet);
      }
      bench.end(n);
      break;
    case 'notDeepEqual_Set':
      bench.start();
      for (i = 0; i < n; ++i) {
        // eslint-disable-next-line no-restricted-properties
        assert.notDeepEqual(actualSet, expectedWrongSet);
      }
      bench.end(n);
      break;
    case 'notDeepStrictEqual_Set':
      bench.start();
      for (i = 0; i < n; ++i) {
        assert.notDeepStrictEqual(actualSet, expectedWrongSet);
      }
      bench.end(n);
      break;
    default:
      throw new Error('Unsupported method');
  }
}

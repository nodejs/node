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

function main({ n, len, primitive, method }) {
  const prim = primValues[primitive];
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

  switch (method) {
    case '':
      // Empty string falls through to next line as default, mostly for tests.
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

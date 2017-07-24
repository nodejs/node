'use strict';

/* eslint-disable no-restricted-properties */

const common = require('../common.js');
const assert = require('assert');

const bench = common.createBenchmark(main, {
  n: [5e2],
  len: [5e2],
  method: [
    'deepEqual_primitiveOnly',
    'deepStrictEqual_primitiveOnly',
    'deepEqual_objectOnly',
    'deepStrictEqual_objectOnly',
    'deepEqual_mixed',
    'deepStrictEqual_mixed',
    'deepEqual_looseMatches',
    'notDeepEqual_primitiveOnly',
    'notDeepStrictEqual_primitiveOnly',
    'notDeepEqual_objectOnly',
    'notDeepStrictEqual_objectOnly',
    'notDeepEqual_mixed',
    'notDeepStrictEqual_mixed',
    'notDeepEqual_looseMatches',
  ]
});

function benchmark(method, n, values, values2) {
  const actual = new Set(values);
  // Prevent reference equal elements
  const deepCopy = JSON.parse(JSON.stringify(values2 ? values2 : values));
  const expected = new Set(deepCopy);
  bench.start();
  for (var i = 0; i < n; ++i) {
    method(actual, expected);
  }
  bench.end(n);
}

function main(conf) {
  const n = +conf.n;
  const len = +conf.len;

  const array = Array(len).fill(1);

  var values, values2;

  switch (conf.method) {
    case 'deepEqual_primitiveOnly':
      values = array.map((_, i) => `str_${i}`);
      benchmark(assert.deepEqual, n, values);
      break;
    case 'deepStrictEqual_primitiveOnly':
      values = array.map((_, i) => `str_${i}`);
      benchmark(assert.deepStrictEqual, n, values);
      break;
    case 'deepEqual_objectOnly':
      values = array.map((_, i) => [`str_${i}`, null]);
      benchmark(assert.deepEqual, n, values);
      break;
    case 'deepStrictEqual_objectOnly':
      values = array.map((_, i) => [`str_${i}`, null]);
      benchmark(assert.deepStrictEqual, n, values);
      break;
    case 'deepEqual_mixed':
      values = array.map((_, i) => {
        return i % 2 ? [`str_${i}`, null] : `str_${i}`;
      });
      benchmark(assert.deepEqual, n, values);
      break;
    case 'deepStrictEqual_mixed':
      values = array.map((_, i) => {
        return i % 2 ? [`str_${i}`, null] : `str_${i}`;
      });
      benchmark(assert.deepStrictEqual, n, values);
      break;
    case 'deepEqual_looseMatches':
      values = array.map((_, i) => i);
      values2 = values.slice().map((v) => String(v));
      benchmark(assert.deepEqual, n, values, values2);
      break;
    case 'notDeepEqual_primitiveOnly':
      values = array.map((_, i) => `str_${i}`);
      values2 = values.slice(0);
      values2[Math.floor(len / 2)] = 'w00t';
      benchmark(assert.notDeepEqual, n, values, values2);
      break;
    case 'notDeepStrictEqual_primitiveOnly':
      values = array.map((_, i) => `str_${i}`);
      values2 = values.slice(0);
      values2[Math.floor(len / 2)] = 'w00t';
      benchmark(assert.notDeepStrictEqual, n, values, values2);
      break;
    case 'notDeepEqual_objectOnly':
      values = array.map((_, i) => [`str_${i}`, null]);
      values2 = values.slice(0);
      values2[Math.floor(len / 2)] = ['w00t'];
      benchmark(assert.notDeepEqual, n, values, values2);
      break;
    case 'notDeepStrictEqual_objectOnly':
      values = array.map((_, i) => [`str_${i}`, null]);
      values2 = values.slice(0);
      values2[Math.floor(len / 2)] = ['w00t'];
      benchmark(assert.notDeepStrictEqual, n, values, values2);
      break;
    case 'notDeepEqual_mixed':
      values = array.map((_, i) => {
        return i % 2 ? [`str_${i}`, null] : `str_${i}`;
      });
      values2 = values.slice();
      values2[0] = 'w00t';
      benchmark(assert.notDeepEqual, n, values, values2);
      break;
    case 'notDeepStrictEqual_mixed':
      values = array.map((_, i) => {
        return i % 2 ? [`str_${i}`, null] : `str_${i}`;
      });
      values2 = values.slice();
      values2[0] = 'w00t';
      benchmark(assert.notDeepStrictEqual, n, values, values2);
      break;
    case 'notDeepEqual_looseMatches':
      values = array.map((_, i) => i);
      values2 = values.slice().map((v) => String(v));
      values2[len - 1] = String(len + 1);
      benchmark(assert.notDeepEqual, n, values, values2);
      break;
    default:
      throw new Error('Unsupported method');
  }
}

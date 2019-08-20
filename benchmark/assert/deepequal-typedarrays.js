'use strict';
const common = require('../common.js');
const assert = require('assert');

const bench = common.createBenchmark(main, {
  type: [
    'Int8Array',
    'Uint8Array',
    'Float32Array',
    'Float64Array',
    'Uint8ClampedArray',
  ],
  n: [5e2],
  strict: [0, 1],
  method: [
    'deepEqual',
    'notDeepEqual',
  ],
  len: [1e2, 5e3],
});

function main({ type, n, len, method, strict }) {
  if (!method)
    method = 'deepEqual';
  const clazz = global[type];
  const actual = new clazz(len);
  const expected = new clazz(len);
  const expectedWrong = new clazz(len);
  const wrongIndex = Math.floor(len / 2);
  expectedWrong[wrongIndex] = 123;

  if (strict) {
    method = method.replace('eep', 'eepStrict');
  }
  const fn = assert[method];
  const value2 = method.includes('not') ? expectedWrong : expected;

  bench.start();
  for (var i = 0; i < n; ++i) {
    actual[0] = i;
    value2[0] = i;
    fn(actual, value2);
  }
  bench.end(n);
}

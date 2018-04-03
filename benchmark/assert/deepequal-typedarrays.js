'use strict';
const common = require('../common.js');
const assert = require('assert');

const bench = common.createBenchmark(main, {
  type: [
    'Int8Array',
    'Uint8Array',
    'Int16Array',
    'Uint16Array',
    'Int32Array',
    'Uint32Array',
    'Float32Array',
    'Float64Array',
    'Uint8ClampedArray',
  ],
  n: [1],
  method: [
    'deepEqual',
    'deepStrictEqual',
    'notDeepEqual',
    'notDeepStrictEqual'
  ],
  len: [1e6]
});

function main({ type, n, len, method }) {
  const clazz = global[type];
  const actual = new clazz(len);
  const expected = new clazz(len);
  const expectedWrong = Buffer.alloc(len);
  const wrongIndex = Math.floor(len / 2);
  expectedWrong[wrongIndex] = 123;

  // eslint-disable-next-line no-restricted-properties
  const fn = method !== '' ? assert[method] : assert.deepEqual;
  const value2 = method.includes('not') ? expectedWrong : expected;

  bench.start();
  for (var i = 0; i < n; ++i) {
    fn(actual, value2);
  }
  bench.end(n);
}

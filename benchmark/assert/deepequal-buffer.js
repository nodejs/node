'use strict';
const common = require('../common.js');
const assert = require('assert');

const bench = common.createBenchmark(main, {
  n: [1e5],
  len: [1e2, 1e4],
  method: [
    'deepEqual',
    'deepStrictEqual',
    'notDeepEqual',
    'notDeepStrictEqual',
  ],
});

function main({ len, n, method }) {
  const data = Buffer.allocUnsafe(len + 1);
  const actual = Buffer.alloc(len);
  const expected = Buffer.alloc(len);
  const expectedWrong = Buffer.alloc(len + 1);
  data.copy(actual);
  data.copy(expected);
  data.copy(expectedWrong);

  // eslint-disable-next-line no-restricted-properties
  const fn = method !== '' ? assert[method] : assert.deepEqual;
  const value2 = method.includes('not') ? expectedWrong : expected;

  bench.start();
  for (var i = 0; i < n; ++i) {
    fn(actual, value2);
  }
  bench.end(n);
}

'use strict';
const common = require('../common.js');
const assert = require('assert');

const bench = common.createBenchmark(main, {
  n: [2e4],
  len: [1e2, 1e3],
  strict: [0, 1],
  method: [ 'deepEqual', 'notDeepEqual' ],
});

function main({ len, n, method, strict }) {
  if (!method)
    method = 'deepEqual';
  const data = Buffer.allocUnsafe(len + 1);
  const actual = Buffer.alloc(len);
  const expected = Buffer.alloc(len);
  const expectedWrong = Buffer.alloc(len + 1);
  data.copy(actual);
  data.copy(expected);
  data.copy(expectedWrong);

  if (strict) {
    method = method.replace('eep', 'eepStrict');
  }
  const fn = assert[method];
  const value2 = method.includes('not') ? expectedWrong : expected;

  bench.start();
  for (var i = 0; i < n; ++i) {
    fn(actual, value2);
  }
  bench.end(n);
}

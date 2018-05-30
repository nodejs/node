'use strict';

const common = require('../common.js');
const assert = require('assert');

const bench = common.createBenchmark(main, {
  n: [1e6],
  size: [1e2, 1e3, 1e4],
  method: [
    'deepEqual',
    'deepStrictEqual',
    'notDeepEqual',
    'notDeepStrictEqual'
  ]
});

function createObj(source, add = '') {
  return source.map((n) => ({
    foo: 'yarp',
    nope: {
      bar: `123${add}`,
      a: [1, 2, 3],
      baz: n
    }
  }));
}

function main({ size, n, method }) {
  // TODO: Fix this "hack". `n` should not be manipulated.
  n = n / size;

  if (!method)
    method = 'deepEqual';

  const source = Array.apply(null, Array(size));
  const actual = createObj(source);
  const expected = createObj(source);
  const expectedWrong = createObj(source, '4');

  const fn = assert[method];
  const value2 = method.includes('not') ? expectedWrong : expected;

  bench.start();
  for (var i = 0; i < n; ++i) {
    fn(actual, value2);
  }
  bench.end(n);
}

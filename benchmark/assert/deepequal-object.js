'use strict';

const common = require('../common.js');
const assert = require('assert');

const bench = common.createBenchmark(main, {
  n: [5e3],
  size: [1e2, 1e3, 5e4],
  strict: [0, 1],
  method: ['deepEqual', 'notDeepEqual'],
});

function createObj(source, add = '') {
  return source.map((n) => ({
    foo: 'yarp',
    nope: {
      bar: `123${add}`,
      a: [1, 2, 3],
      baz: n,
      c: {},
      b: [],
    },
  }));
}

function main({ size, n, method, strict }) {
  // TODO: Fix this "hack". `n` should not be manipulated.
  n = Math.min(Math.ceil(n / size), 20);

  const source = Array.apply(null, Array(size));
  const actual = createObj(source);
  const expected = createObj(source);
  const expectedWrong = createObj(source, '4');

  if (strict) {
    method = method.replace('eep', 'eepStrict');
  }
  const fn = assert[method];
  const value2 = method.includes('not') ? expectedWrong : expected;

  bench.start();
  for (let i = 0; i < n; ++i) {
    fn(actual, value2);
  }
  bench.end(n);
}

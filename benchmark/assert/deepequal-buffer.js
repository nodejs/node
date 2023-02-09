'use strict';
const common = require('../common.js');
const assert = require('assert');

const bench = common.createBenchmark(main, {
  n: [2e4],
  len: [1e2, 1e3],
  strict: [0, 1],
  arrayBuffer: [0, 1],
  method: ['deepEqual', 'notDeepEqual', 'unequal_length'],
}, {
  combinationFilter: (p) => {
    return p.strict === 1 || p.method === 'deepEqual';
  },
});

function main({ len, n, method, strict, arrayBuffer }) {
  let actual = Buffer.alloc(len);
  let expected = Buffer.alloc(len + Number(method === 'unequal_length'));


  if (method === 'unequal_length') {
    method = 'notDeepEqual';
  }

  for (let i = 0; i < len; i++) {
    actual.writeInt8(i % 128, i);
    expected.writeInt8(i % 128, i);
  }

  if (method.includes('not')) {
    const position = Math.floor(len / 2);
    expected[position] = expected[position] + 1;
  }

  if (strict) {
    method = method.replace('eep', 'eepStrict');
  }

  const fn = assert[method];

  if (arrayBuffer) {
    actual = actual.buffer;
    expected = expected.buffer;
  }

  bench.start();
  for (let i = 0; i < n; ++i) {
    fn(actual, expected);
  }
  bench.end(n);
}

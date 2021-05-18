'use strict';

const common = require('../common.js');
const assert = require('assert');
const bench = common.createBenchmark(main, {
  type: [
    'fast-alloc',
    'fast-alloc-fill',
    'fast-allocUnsafe',
    'slow-allocUnsafe',
  ],
  len: [10, 1024, 4096, 8192],
  n: [6e5],
});

function main({ len, n, type }) {
  let fn, i;
  switch (type) {
    case 'fast-alloc':
      fn = Buffer.alloc;
      break;
    case 'fast-alloc-fill':
      bench.start();
      for (i = 0; i < n; i++) {
        Buffer.alloc(len, 0);
      }
      bench.end(n);
      return;
    case 'fast-allocUnsafe':
      fn = Buffer.allocUnsafe;
      break;
    case 'slow-allocUnsafe':
      fn = Buffer.allocUnsafeSlow;
      break;
    default:
      assert.fail('Should not get here');
  }

  bench.start();
  for (i = 0; i < n; i++) {
    fn(len);
  }
  bench.end(n);
}

'use strict';
const SlowBuffer = require('buffer').SlowBuffer;

const common = require('../common.js');
const assert = require('assert');
const bench = common.createBenchmark(main, {
  type: [
    'fast-alloc',
    'fast-alloc-fill',
    'fast-allocUnsafe',
    'slow-allocUnsafe',
    'slow',
    'buffer()'],
  len: [10, 1024, 2048, 4096, 8192],
  n: [1024]
});

function main({ len, n, type }) {
  let fn, i;
  switch (type) {
    case '':
    case 'fast-alloc':
      fn = Buffer.alloc;
      break;
    case 'fast-alloc-fill':
      bench.start();
      for (i = 0; i < n * 1024; i++) {
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
    case 'slow':
      fn = SlowBuffer;
      break;
    case 'buffer()':
      fn = Buffer;
      break;
    default:
      assert.fail('Should not get here');
  }

  bench.start();
  for (i = 0; i < n * 1024; i++) {
    fn(len);
  }
  bench.end(n);
}

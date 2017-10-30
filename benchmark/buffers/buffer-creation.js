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

function main(conf) {
  const len = +conf.len;
  const n = +conf.n;
  switch (conf.type) {
    case '':
    case 'fast-alloc':
      bench.start();
      for (let i = 0; i < n * 1024; i++) {
        Buffer.alloc(len);
      }
      bench.end(n);
      break;
    case 'fast-alloc-fill':
      bench.start();
      for (let i = 0; i < n * 1024; i++) {
        Buffer.alloc(len, 0);
      }
      bench.end(n);
      break;
    case 'fast-allocUnsafe':
      bench.start();
      for (let i = 0; i < n * 1024; i++) {
        Buffer.allocUnsafe(len);
      }
      bench.end(n);
      break;
    case 'slow-allocUnsafe':
      bench.start();
      for (let i = 0; i < n * 1024; i++) {
        Buffer.allocUnsafeSlow(len);
      }
      bench.end(n);
      break;
    case 'slow':
      bench.start();
      for (let i = 0; i < n * 1024; i++) {
        SlowBuffer(len);
      }
      bench.end(n);
      break;
    case 'buffer()':
      bench.start();
      for (let i = 0; i < n * 1024; i++) {
        Buffer(len);
      }
      bench.end(n);
      break;
    default:
      assert.fail(null, null, 'Should not get here');
  }
}

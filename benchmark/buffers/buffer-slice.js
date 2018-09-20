'use strict';
const common = require('../common.js');
const SlowBuffer = require('buffer').SlowBuffer;

const bench = common.createBenchmark(main, {
  type: ['fast', 'slow'],
  n: [1024]
});

const buf = Buffer.allocUnsafe(1024);
const slowBuf = new SlowBuffer(1024);

function main({ n, type }) {
  const b = type === 'fast' ? buf : slowBuf;
  bench.start();
  for (var i = 0; i < n * 1024; i++) {
    b.slice(10, 256);
  }
  bench.end(n);
}

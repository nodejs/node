'use strict';
const common = require('../common.js');

const bench = common.createBenchmark(main, {
  method: ['offset', 'slice'],
  size: [16, 512, 4096, 16386],
  n: [1e6]
});

function compareUsingSlice(b0, b1, len, iter) {
  for (let i = 0; i < iter; i++)
    Buffer.compare(b0.slice(1, len), b1.slice(1, len));
}

function compareUsingOffset(b0, b1, len, iter) {
  for (let i = 0; i < iter; i++)
    b0.compare(b1, 1, len, 1, len);
}

function main({ n, size, method }) {
  const fn = method === 'slice' ? compareUsingSlice : compareUsingOffset;
  bench.start();
  fn(Buffer.alloc(size, 'a'),
     Buffer.alloc(size, 'b'),
     size >> 1,
     n);
  bench.end(n);
}

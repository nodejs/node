'use strict';
const common = require('../common.js');

const bench = common.createBenchmark(main, {
  method: ['offset', 'slice'],
  size: [16, 512, 1024, 4096, 16386],
  millions: [1]
});

function compareUsingSlice(b0, b1, len, iter) {
  var i;
  bench.start();
  for (i = 0; i < iter; i++)
    Buffer.compare(b0.slice(1, len), b1.slice(1, len));
  bench.end(iter / 1e6);
}

function compareUsingOffset(b0, b1, len, iter) {
  var i;
  bench.start();
  for (i = 0; i < iter; i++)
    b0.compare(b1, 1, len, 1, len);
  bench.end(iter / 1e6);
}

function main({ millions, size, method }) {
  const iter = millions * 1e6;
  const fn = method === 'slice' ? compareUsingSlice : compareUsingOffset;
  fn(Buffer.alloc(size, 'a'),
     Buffer.alloc(size, 'b'),
     size >> 1,
     iter);
}

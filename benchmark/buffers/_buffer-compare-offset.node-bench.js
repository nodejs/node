'use strict';

const { bench } = require('node:bench');
const path = require('node:path');

const methods = ['offset', 'slice'];
const sizes = [16, 512, 4096, 16386];
const n = 1e6;
const name = path.join('buffers', 'buffer-compare-offset.js');

function compareUsingSlice(b0, b1, len, iterations) {
  for (let i = 0; i < iterations; i++)
    Buffer.compare(b0.slice(1, len), b1.slice(1, len));
}

function compareUsingOffset(b0, b1, len, iterations) {
  for (let i = 0; i < iterations; i++)
    b0.compare(b1, 1, len, 1, len);
}

for (const method of methods) {
  for (const size of sizes) {
    const compare = method === 'slice' ?
      compareUsingSlice : compareUsingOffset;

    bench(name, {
      params: { method, n, size },
    }, (b) => {
      b.start();
      compare(Buffer.alloc(size, 'a'),
              Buffer.alloc(size, 'b'),
              size >> 1,
              n);
      b.end(n);
    });
  }
}

'use strict';

// flags: --expose-internals

const common = require('../common');
const { FastBuffer } = require('internal/buffer');

const bench = common.createBenchmark(main, {
  n: [1e6],
  operation: ['fastbuffer', 'bufferalloc'],
});


function main({ n, operation }) {
  switch (operation) {
    case 'fastbuffer':
      bench.start();
      for (let i = 0; i < n; i++) {
        new FastBuffer();
      }
      bench.end(n);
      break;
    case 'bufferalloc':
      bench.start();
      for (let i = 0; i < n; i++) {
        Buffer.alloc(0);
      }
      bench.end(n);
      break;
  }
}

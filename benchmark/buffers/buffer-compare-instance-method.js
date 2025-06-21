'use strict';
const common = require('../common.js');

const bench = common.createBenchmark(main, {
  size: [16, 512, 4096, 16386],
  args: [1, 2, 5],
  n: [1e6],
});

function main({ n, size, args }) {
  const b0 = Buffer.alloc(size, 'a');
  const b1 = Buffer.alloc(size, 'a');
  const b0Len = b0.length;
  const b1Len = b1.length;

  b1[size - 1] = 'b'.charCodeAt(0);

  switch (args) {
    case 2:
      b0.compare(b1, 0);
      bench.start();
      for (let i = 0; i < n; i++) {
        b0.compare(b1, 0);
      }
      bench.end(n);
      break;
    case 3:
      b0.compare(b1, 0, b1Len);
      bench.start();
      for (let i = 0; i < n; i++) {
        b0.compare(b1, 0, b1Len);
      }
      bench.end(n);
      break;
    case 4:
      b0.compare(b1, 0, b1Len, 0);
      bench.start();
      for (let i = 0; i < n; i++) {
        b0.compare(b1, 0, b1Len, 0);
      }
      bench.end(n);
      break;
    case 5:
      b0.compare(b1, 0, b1Len, 0, b0Len);
      bench.start();
      for (let i = 0; i < n; i++) {
        b0.compare(b1, 0, b1Len, 0, b0Len);
      }
      bench.end(n);
      break;
    default:
      b0.compare(b1);
      bench.start();
      for (let i = 0; i < n; i++) {
        b0.compare(b1);
      }
      bench.end(n);
  }
}

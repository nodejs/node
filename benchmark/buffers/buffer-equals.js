'use strict';
const common = require('../common.js');

const bench = common.createBenchmark(main, {
  size: [0, 512, 16386],
  difflen: ['true', 'false'],
  n: [1e6]
});

function main({ n, size, difflen }) {
  const b0 = Buffer.alloc(size, 'a');
  const b1 = Buffer.alloc(size + (difflen === 'true' ? 1 : 0), 'a');

  if (b1.length > 0)
    b1[b1.length - 1] = 'b'.charCodeAt(0);

  bench.start();
  for (let i = 0; i < n; i++) {
    b0.equals(b1);
  }
  bench.end(n);
}

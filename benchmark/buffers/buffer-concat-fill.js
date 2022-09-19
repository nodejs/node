'use strict';
const common = require('../common.js');

const bench = common.createBenchmark(main, {
  extraSize: [1, 256, 4 * 256],
  n: [8e5]
});

function main({ n, extraSize }) {
  const pieces = 4;
  const pieceSize = 256;

  const list = Array.from({ length: pieces })
    .fill(Buffer.allocUnsafe(pieceSize));

  const totalLength = (pieces * pieceSize) + extraSize;

  bench.start();
  for (let i = 0; i < n; i++) {
    Buffer.concat(list, totalLength);
  }
  bench.end(n);
}

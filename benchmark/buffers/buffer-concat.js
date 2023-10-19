'use strict';
const common = require('../common.js');

const bench = common.createBenchmark(main, {
  pieces: [4, 16],
  pieceSize: [1, 16, 256],
  withTotalLength: [0, 1],
  n: [8e5],
});

function main({ n, pieces, pieceSize, withTotalLength }) {
  const list = Array.from({ length: pieces })
    .fill(Buffer.allocUnsafe(pieceSize));

  const totalLength = withTotalLength ? pieces * pieceSize : undefined;

  bench.start();
  for (let i = 0; i < n; i++) {
    Buffer.concat(list, totalLength);
  }
  bench.end(n);
}

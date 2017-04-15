'use strict';
const common = require('../common.js');

const bench = common.createBenchmark(main, {
  pieces: [1, 4, 16],
  pieceSize: [1, 16, 256],
  withTotalLength: [0, 1],
  n: [1024]
});

function main(conf) {
  const n = +conf.n;
  const size = +conf.pieceSize;
  const pieces = +conf.pieces;

  const list = new Array(pieces);
  list.fill(Buffer.allocUnsafe(size));

  const totalLength = conf.withTotalLength ? pieces * size : undefined;

  bench.start();
  for (var i = 0; i < n * 1024; i++) {
    Buffer.concat(list, totalLength);
  }
  bench.end(n);
}

'use strict';
const common = require('../common.js');
const fs = require('fs');
const zlib = require('zlib');

const bench = common.createBenchmark(main, {
  method: ['zstdCompress', 'zstdCompressSync'],
  level: [3, 12],
  inputLen: [16 * 1024, 256 * 1024],
  n: [100],
});

function main({ n, method, level, inputLen }) {
  const input = Buffer.alloc(inputLen, fs.readFileSync(__filename));
  const opts = { params: { [zlib.constants.ZSTD_c_compressionLevel]: level } };

  switch (method) {
    // Performs `n` single zstdCompress operations
    case 'zstdCompress': {
      let i = 0;
      bench.start();
      (function next(err) {
        if (err) throw err;
        if (i++ === n)
          return bench.end(n);
        zlib.zstdCompress(input, opts, next);
      })();
      break;
    }
    // Performs `n` single zstdCompressSync operations
    case 'zstdCompressSync': {
      bench.start();
      for (let i = 0; i < n; ++i)
        zlib.zstdCompressSync(input, opts);
      bench.end(n);
      break;
    }
    default:
      throw new Error('Unsupported zstd method');
  }
}

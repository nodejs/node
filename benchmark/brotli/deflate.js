// Flags: --expose-brotli
'use strict';
const common = require('../common.js');
const brotli = require('brotli');

const bench = common.createBenchmark(main, {
  method: ['Compress', 'compress', 'compressSync'],
  quality: [4, 11],
  inputLen: [1024],
  n: [4e5]
});

function main({ n, method, inputLen, quality }) {
  const chunk = Buffer.alloc(inputLen, 'a');

  var i = 0;
  switch (method) {
    // Performs `n` writes for a single deflate stream
    case 'Compress':
      const compress = new brotli.Compress({ quality });
      compress.resume();
      compress.on('finish', () => {
        bench.end(n);
      });

      bench.start();
      (function next() {
        if (i++ === n)
          return compress.end();
        compress.write(chunk, next);
      })();
      break;
    // Performs `n` single deflate operations
    case 'compress':
      bench.start();
      (function next(err, result) {
        if (i++ === n)
          return bench.end(n);
        brotli.compress(chunk, { quality }, next);
      })();
      break;
    // Performs `n` single deflateSync operations
    case 'compressSync':
      bench.start();
      for (; i < n; ++i)
        brotli.compressSync(chunk, { quality });
      bench.end(n);
      break;
    default:
      throw new Error('Unsupported method');
  }
}

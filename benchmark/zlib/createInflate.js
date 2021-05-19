'use strict';
const common = require('../common.js');
const zlib = require('zlib');

const bench = common.createBenchmark(main, {
  inputLen: [16 * 1024 * 1024],
  chunkLen: [1024],
  n: [1e2],
});

function main({ n, inputLen, chunkLen }) {
  const input = zlib.deflateSync(Buffer.alloc(inputLen, 'a'));

  let i = 0;
  bench.start();
  (function next() {
    let p = 0;
    const inflater = zlib.createInflate();
    inflater.resume();
    inflater.on('finish', () => {
      if (i++ === n)
        return bench.end(n);
      next();
    });

    (function nextChunk() {
      if (p >= input.length)
        return inflater.end();
      inflater.write(input.slice(p, p += chunkLen), nextChunk);
    })();
  })();
}

'use strict';

const common = require('../common.js');
const { Writable } = require('stream');

// Benchmarks StreamBase::Writev with string chunks, exercising the chunk
// cache that avoids redundant V8 array accesses, ToString, and ParseEncoding
// calls between the sizing pass and the write pass.
const bench = common.createBenchmark(main, {
  n: [1e4],
  chunks: [4, 16, 64],
  encoding: ['utf8', 'latin1'],
  type: ['string', 'buffer', 'mixed'],
});

function main({ n, chunks, encoding, type }) {
  const str = 'Hello, benchmark! '.repeat(4);
  const buf = Buffer.from(str, encoding);

  const wr = new Writable({
    writev(chunks, cb) { cb(); },
    write(chunk, enc, cb) { cb(); },
  });

  bench.start();
  for (let i = 0; i < n; i++) {
    wr.cork();
    for (let j = 0; j < chunks; j++) {
      if (type === 'buffer') {
        wr.write(buf);
      } else if (type === 'string') {
        wr.write(str, encoding);
      } else if (j % 2 === 0) {
        // Alternate buffer and string to hit the mixed (non-all_buffers) path.
        wr.write(buf);
      } else {
        wr.write(str, encoding);
      }
    }
    wr.uncork();
  }
  bench.end(n);
}

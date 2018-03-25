'use strict';

const common = require('../common');
const { Readable } = require('stream');

const bench = common.createBenchmark(main, {
  skipChunkCheck: [1, 0],
  n: [1e6]
});

function main({ n, skipChunkCheck }) {
  const buf = Buffer.alloc(64);
  const readable = new Readable({
    skipChunkCheck,
    read() {}
  });

  readable.on('end', function() {
    bench.end(n);
  });
  readable.on('resume', function() {
    bench.start();
    for (var i = 0; i < n; i++)
      this.push(buf);
    this.push(null);
  });
  readable.resume();
}

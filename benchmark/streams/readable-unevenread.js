'use strict';

const common = require('../common');
const Readable = require('stream').Readable;

const bench = common.createBenchmark(main, {
  n: [1e3]
});

function main({ n }) {
  const b = Buffer.alloc(32);
  const s = new Readable();
  function noop() {}
  s._read = noop;

  bench.start();
  for (var k = 0; k < n; ++k) {
    for (var i = 0; i < 1e4; ++i)
      s.push(b);
    while (s.read(12));
  }
  bench.end(n);
}

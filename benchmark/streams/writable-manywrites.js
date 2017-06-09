'use strict';

const common = require('../common');
const Writable = require('stream').Writable;

const bench = common.createBenchmark(main, {
  n: [2e6]
});

function main(conf) {
  const n = +conf.n;
  const b = Buffer.allocUnsafe(1024);
  const s = new Writable();
  s._write = function(chunk, encoding, cb) {
    cb();
  };

  bench.start();
  for (var k = 0; k < n; ++k) {
    s.write(b);
  }
  bench.end(n);
}

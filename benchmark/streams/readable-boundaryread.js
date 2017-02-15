'use strict';

const common = require('../common');
const v8 = require('v8');
const Readable = require('stream').Readable;

const bench = common.createBenchmark(main, {
  n: [200e1]
});

function main(conf) {
  const n = +conf.n;
  const b = new Buffer(32);
  const s = new Readable();
  function noop() {}
  s._read = noop;

  // Force optimization before starting the benchmark
  s.push(b);
  v8.setFlagsFromString('--allow_natives_syntax');
  eval('%OptimizeFunctionOnNextCall(s.push)');
  eval('%OptimizeFunctionOnNextCall(s.read)');
  s.push(b);
  while (s.read(32));

  bench.start();
  for (var k = 0; k < n; ++k) {
    for (var i = 0; i < 1e4; ++i)
      s.push(b);
    while (s.read(32));
  }
  bench.end(n);
}

'use strict';
var common = require('../common.js');
var SlowBuffer = require('buffer').SlowBuffer;

var bench = common.createBenchmark(main, {
  type: ['fast', 'slow'],
  n: [1024]
});

var buf = new Buffer(1024);
var slowBuf = new SlowBuffer(1024);

function main(conf) {
  var n = +conf.n;
  var b = conf.type === 'fast' ? buf : slowBuf;
  bench.start();
  for (var i = 0; i < n * 1024; i++) {
    b.slice(10, 256);
  }
  bench.end(n);
}

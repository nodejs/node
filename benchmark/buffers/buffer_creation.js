SlowBuffer = require('buffer').SlowBuffer;

var common = require('../common.js');
var bench = common.createBenchmark(main, {
  len: [10, 1024],
  n: [1024]
});

function main(conf) {
  var len = +conf.len;
  var n = +conf.n;
  bench.start();
  for (var i = 0; i < n * 1024; i++) {
    b = new SlowBuffer(len);
  }
  bench.end(n);
}

SlowBuffer = require('buffer').SlowBuffer;

var common = require('../common.js');
var bench = common.createBenchmark(main, {
  type: ['fast', 'slow'],
  len: [10, 1024],
  n: [1024]
});

function main(conf) {
  var len = +conf.len;
  var n = +conf.n;
  var clazz = conf.type === 'fast' ? Buffer : SlowBuffer;
  bench.start();
  for (var i = 0; i < n * 1024; i++) {
    b = new clazz(len);
  }
  bench.end(n);
}

var common = require('../common.js');
var bench = common.createBenchmark(main, {
  type: 'Array Buffer Int8Array Uint8Array Int16Array Uint16Array Int32Array Uint32Array Float32Array Float64Array'.split(' '),
  n: [25]
});

function main(conf) {
  var type = conf.type;
  var clazz = global[type];
  var n = +conf.n;

  bench.start();
  var arr = new clazz(n * 1e6);
  for (var i = 0; i < 10; ++i) {
    for (var j = 0, k = arr.length; j < k; ++j) {
      arr[j] = 0;
    }
  }
  bench.end(n);
}

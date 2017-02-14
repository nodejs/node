'use strict';
var common = require('../common.js');

var types = [
  'Array',
  'Buffer',
  'Int8Array',
  'Uint8Array',
  'Int16Array',
  'Uint16Array',
  'Int32Array',
  'Uint32Array',
  'Float32Array',
  'Float64Array'
];

var bench = common.createBenchmark(main, {
  type: types,
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
      arr[j] = (j ^ k) & 127;
    }
  }
  bench.end(n);
}

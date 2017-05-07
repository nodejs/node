'use strict';
const common = require('../common.js');

const bench = common.createBenchmark(main, {
  type: [
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
  ],
  n: [25]
});

function main(conf) {
  const type = conf.type;
  const clazz = global[type];
  const n = +conf.n;

  bench.start();
  var arr = new clazz(n * 1e6);
  for (var i = 0; i < 10; ++i) {
    run();
  }
  bench.end(n);

  function run() {
    for (var j = 0, k = arr.length; j < k; ++j) {
      arr[j] = (j ^ k) & 127;
    }
  }
}

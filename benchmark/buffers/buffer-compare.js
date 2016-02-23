'use strict';
var common = require('../common.js');

var bench = common.createBenchmark(main, {
  size: [16, 512, 1024, 4096, 16386],
  millions: [1]
});

function main(conf) {
  var iter = (conf.millions >>> 0) * 1e6;
  var size = (conf.size >>> 0);
  var b0 = new Buffer(size).fill('a');
  var b1 = new Buffer(size).fill('a');

  b1[size - 1] = 'b'.charCodeAt(0);

  bench.start();
  for (var i = 0; i < iter; i++) {
    Buffer.compare(b0, b1);
  }
  bench.end(iter / 1e6);
}

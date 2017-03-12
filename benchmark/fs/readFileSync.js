'use strict';

var common = require('../common.js');
var fs = require('fs');

var bench = common.createBenchmark(main, {
  n: [60e4]
});

function main(conf) {
  var n = +conf.n;

  bench.start();
  for (var i = 0; i < n; ++i)
    fs.readFileSync(__filename);
  bench.end(n);
}

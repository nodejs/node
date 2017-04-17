'use strict';
var common = require('../common.js');
var path = require('path');

var bench = common.createBenchmark(main, {
  path: [
    'foo\\bar',
    'C:\\foo',
    '\\\\foo\\bar',
    '\\\\?\\foo'
  ],
  n: [1e6]
});

function main(conf) {
  var n = +conf.n;
  var p = path.win32;
  var input = String(conf.path);

  bench.start();
  for (var i = 0; i < n; i++) {
    p._makeLong(input);
  }
  bench.end(n);
}

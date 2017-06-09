'use strict';
var common = require('../common.js');
var path = require('path');

var bench = common.createBenchmark(main, {
  paths: [
    ['/foo', 'bar', '', 'baz/asdf', 'quux', '..'].join('|')
  ],
  n: [1e6]
});

function main(conf) {
  var n = +conf.n;
  var p = path.posix;
  var args = String(conf.paths).split('|');

  bench.start();
  for (var i = 0; i < n; i++) {
    p.join.apply(null, args);
  }
  bench.end(n);
}

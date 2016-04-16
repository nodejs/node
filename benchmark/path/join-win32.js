'use strict';
var common = require('../common.js');
var path = require('path');
var v8 = require('v8');

var bench = common.createBenchmark(main, {
  paths: [
    ['C:\\foo', 'bar', '', 'baz\\asdf', 'quux', '..'].join('|')
  ],
  n: [1e6]
});

function main(conf) {
  var n = +conf.n;
  var p = path.win32;
  var args = ('' + conf.paths).split('|');

  // Force optimization before starting the benchmark
  p.join.apply(null, args);
  v8.setFlagsFromString('--allow_natives_syntax');
  eval('%OptimizeFunctionOnNextCall(p.join)');
  p.join.apply(null, args);

  bench.start();
  for (var i = 0; i < n; i++) {
    p.join.apply(null, args);
  }
  bench.end(n);
}

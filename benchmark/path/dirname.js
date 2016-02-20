'use strict';
var common = require('../common.js');
var path = require('path');
var v8 = require('v8');

var bench = common.createBenchmark(main, {
  type: ['win32', 'posix'],
  n: [1e6],
});

function main(conf) {
  var n = +conf.n;
  var p = path[conf.type];

  // Force optimization before starting the benchmark
  p.dirname('/foo/bar/baz/asdf/quux');
  v8.setFlagsFromString('--allow_natives_syntax');
  eval('%OptimizeFunctionOnNextCall(p.dirname)');
  p.dirname('/foo/bar/baz/asdf/quux');

  bench.start();
  for (var i = 0; i < n; i++) {
    p.dirname('/foo/bar/baz/asdf/quux');
  }
  bench.end(n);
}

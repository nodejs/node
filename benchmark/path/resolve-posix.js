'use strict';
var common = require('../common.js');
var path = require('path');
var v8 = require('v8');

var bench = common.createBenchmark(main, {
  paths: [
    '',
    ['', ''].join('|'),
    ['foo/bar', '/tmp/file/', '..', 'a/../subfile'].join('|'),
    ['a/b/c/', '../../..'].join('|')
  ],
  n: [1e6]
});

function main(conf) {
  var n = +conf.n;
  var p = path.posix;
  var args = ('' + conf.paths).split('|');

  // Force optimization before starting the benchmark
  p.resolve.apply(null, args);
  v8.setFlagsFromString('--allow_natives_syntax');
  eval('%OptimizeFunctionOnNextCall(p.resolve)');
  p.resolve.apply(null, args);

  bench.start();
  for (var i = 0; i < n; i++) {
    p.resolve.apply(null, args);
  }
  bench.end(n);
}

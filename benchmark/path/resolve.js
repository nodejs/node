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
  p.resolve('foo/bar', '/tmp/file/', '..', 'a/../subfile');
  v8.setFlagsFromString('--allow_natives_syntax');
  eval('%OptimizeFunctionOnNextCall(p.resolve)');
  p.resolve('foo/bar', '/tmp/file/', '..', 'a/../subfile');

  bench.start();
  for (var i = 0; i < n; i++) {
    p.resolve('foo/bar', '/tmp/file/', '..', 'a/../subfile');
  }
  bench.end(n);
}

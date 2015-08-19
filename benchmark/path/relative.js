var common = require('../common.js');
var path = require('path');
var v8 = require('v8');

var bench = common.createBenchmark(main, {
  type: ['win32', 'posix'],
  n: [1e5],
});

function main(conf) {
  var n = +conf.n;
  var runTest = conf.type === 'win32' ? runWin32Test : runPosixTest;

  // Force optimization before starting the benchmark
  runTest();
  v8.setFlagsFromString('--allow_natives_syntax');
  eval('%OptimizeFunctionOnNextCall(path[conf.type].relative)');
  runTest();

  bench.start();
  for (var i = 0; i < n; i++) {
    runTest();
  }
  bench.end(n);
}

function runWin32Test() {
  path.win32.relative('C:\\orandea\\test\\aaa', 'C:\\orandea\\impl\\bbb');
}

function runPosixTest() {
  path.posix.relative('/data/orandea/test/aaa', '/data/orandea/impl/bbb');
}

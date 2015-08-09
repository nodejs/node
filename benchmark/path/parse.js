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
  var test = conf.type === 'win32'
    ? 'C:\\path\\dir\\index.html'
    : '/home/user/dir/index.html';

  // Force optimization before starting the benchmark
  p.parse(test);
  v8.setFlagsFromString('--allow_natives_syntax');
  eval('%OptimizeFunctionOnNextCall(p.parse)');
  p.parse(test);

  bench.start();
  for (var i = 0; i < n; i++) {
    p.parse(test);
  }
  bench.end(n);
}

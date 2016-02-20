'use strict';
var common = require('../common.js');
var path = require('path');
var v8 = require('v8');

var bench = common.createBenchmark(main, {
  type: ['win32', 'posix'],
  n: [1e7],
});

function main(conf) {
  var n = +conf.n;
  var p = path[conf.type];
  var test = conf.type === 'win32' ? {
    root: 'C:\\',
    dir: 'C:\\path\\dir',
    base: 'index.html',
    ext: '.html',
    name: 'index'
  } : {
    root : '/',
    dir : '/home/user/dir',
    base : 'index.html',
    ext : '.html',
    name : 'index'
  };

  // Force optimization before starting the benchmark
  p.format(test);
  v8.setFlagsFromString('--allow_natives_syntax');
  eval('%OptimizeFunctionOnNextCall(p.format)');
  p.format(test);

  bench.start();
  for (var i = 0; i < n; i++) {
    p.format(test);
  }
  bench.end(n);
}

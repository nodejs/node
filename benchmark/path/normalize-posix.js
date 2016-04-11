'use strict';
var common = require('../common.js');
var path = require('path');
var v8 = require('v8');

var bench = common.createBenchmark(main, {
  path: [
    '',
    '.',
    '/../',
    '/foo',
    '/foo/bar',
    '/foo/bar//baz/asdf/quux/..'
  ],
  n: [1e6]
});

function main(conf) {
  var n = +conf.n;
  var p = path.posix;
  var input = '' + conf.path;

  // Force optimization before starting the benchmark
  p.normalize(input);
  v8.setFlagsFromString('--allow_natives_syntax');
  eval('%OptimizeFunctionOnNextCall(p.normalize)');
  p.normalize(input);

  bench.start();
  for (var i = 0; i < n; i++) {
    p.normalize(input);
  }
  bench.end(n);
}

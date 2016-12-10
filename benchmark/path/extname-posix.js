'use strict';
var common = require('../common.js');
var path = require('path');
var v8 = require('v8');

var bench = common.createBenchmark(main, {
  path: [
    '',
    '/',
    '/foo',
    'foo/.bar.baz',
    'index.html',
    'index',
    'foo/bar/..baz.quux',
    'foo/bar/...baz.quux',
    '/foo/bar/baz/asdf/quux',
    '/foo/bar/baz/asdf/quux.foobarbazasdfquux'
  ],
  n: [1e6]
});

function main(conf) {
  var n = +conf.n;
  var p = path.posix;
  var input = '' + conf.path;

  // Force optimization before starting the benchmark
  p.extname(input);
  v8.setFlagsFromString('--allow_natives_syntax');
  eval('%OptimizeFunctionOnNextCall(p.extname)');
  p.extname(input);

  bench.start();
  for (var i = 0; i < n; i++) {
    p.extname(input);
  }
  bench.end(n);
}

'use strict';
var common = require('../common.js');
var path = require('path');

var bench = common.createBenchmark(main, {
  path: [
    '',
    '/',
    '/foo',
    '/foo/bar.baz',
    'foo/.bar.baz',
    'foo/bar',
    '/foo/bar/baz/asdf/.quux'
  ],
  n: [1e6]
});

function main(conf) {
  var n = +conf.n;
  var p = path.posix;
  var input = '' + conf.path;

  for (var i = 0; i < n; i++) {
    p.parse(input);
  }
  bench.start();
  for (i = 0; i < n; i++) {
    p.parse(input);
  }
  bench.end(n);
}

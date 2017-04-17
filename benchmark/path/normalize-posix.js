'use strict';
var common = require('../common.js');
var path = require('path');

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
  var input = String(conf.path);

  bench.start();
  for (var i = 0; i < n; i++) {
    p.normalize(input);
  }
  bench.end(n);
}

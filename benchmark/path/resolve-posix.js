'use strict';
var common = require('../common.js');
var path = require('path');

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

  bench.start();
  for (var i = 0; i < n; i++) {
    p.resolve.apply(null, args);
  }
  bench.end(n);
}

'use strict';
var common = require('../common.js');
var path = require('path');

var bench = common.createBenchmark(main, {
  paths: [
    '',
    ['', ''].join('|'),
    ['c:/ignore', 'd:\\a/b\\c/d', '\\e.exe'].join('|'),
    ['c:/blah\\blah', 'd:/games', 'c:../a'].join('|')
  ],
  n: [1e6]
});

function main(conf) {
  var n = +conf.n;
  var p = path.win32;
  var args = ('' + conf.paths).split('|');

  bench.start();
  for (var i = 0; i < n; i++) {
    p.resolve.apply(null, args);
  }
  bench.end(n);
}

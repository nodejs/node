'use strict';
var common = require('../common.js');
var path = require('path');

var bench = common.createBenchmark(main, {
  path: [
    '',
    '.',
    '//server',
    'C:\\baz\\..',
    'C:baz\\..',
    'bar\\baz'
  ],
  n: [1e6]
});

function main(conf) {
  var n = +conf.n;
  var p = path.win32;
  var input = '' + conf.path;

  bench.start();
  for (var i = 0; i < n; i++) {
    p.isAbsolute(input);
  }
  bench.end(n);
}

'use strict';
var common = require('../common.js');
var path = require('path');

var bench = common.createBenchmark(main, {
  path: [
    '',
    '\\',
    '\\foo',
    'C:\\foo\\bar',
    'foo',
    'foo\\bar',
    'D:\\foo\\bar\\baz\\asdf\\quux'
  ],
  n: [1e6]
});

function main(conf) {
  var n = +conf.n;
  var p = path.win32;
  var input = '' + conf.path;

  bench.start();
  for (var i = 0; i < n; i++) {
    p.dirname(input);
  }
  bench.end(n);
}

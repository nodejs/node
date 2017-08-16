'use strict';
var common = require('../common.js');
var path = require('path');

var bench = common.createBenchmark(main, {
  path: [
    '',
    '\\',
    'C:\\foo',
    'foo\\.bar.baz',
    'index.html',
    'index',
    'foo\\bar\\..baz.quux',
    'foo\\bar\\...baz.quux',
    'D:\\foo\\bar\\baz\\asdf\\quux',
    '\\foo\\bar\\baz\\asdf\\quux.foobarbazasdfquux'
  ],
  n: [1e6]
});

function main(conf) {
  var n = +conf.n;
  var p = path.win32;
  var input = String(conf.path);

  bench.start();
  for (var i = 0; i < n; i++) {
    p.extname(input);
  }
  bench.end(n);
}

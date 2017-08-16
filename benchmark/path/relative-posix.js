'use strict';
var common = require('../common.js');
var path = require('path');

var bench = common.createBenchmark(main, {
  paths: [
    ['/data/orandea/test/aaa', '/data/orandea/impl/bbb'].join('|'),
    ['/', '/var'].join('|'),
    ['/', '/'].join('|'),
    ['/var', '/bin'].join('|'),
    ['/foo/bar/baz/quux', '/'].join('|'),
    ['/foo/bar/baz/quux', '/foo/bar/baz/quux'].join('|'),
    ['/foo/bar/baz/quux', '/var/log'].join('|')
  ],
  n: [1e6]
});

function main(conf) {
  var n = +conf.n;
  var p = path.posix;
  var from = String(conf.paths);
  var to = '';
  var delimIdx = from.indexOf('|');
  if (delimIdx > -1) {
    to = from.slice(delimIdx + 1);
    from = from.slice(0, delimIdx);
  }
  for (var i = 0; i < n; i++) {
    p.relative(from, to);
  }

  bench.start();
  for (i = 0; i < n; i++) {
    p.relative(from, to);
  }
  bench.end(n);
}

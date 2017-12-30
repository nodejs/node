'use strict';
const common = require('../common.js');
const { posix } = require('path');

const bench = common.createBenchmark(main, {
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

function main({ n, paths }) {
  var to = '';
  const delimIdx = paths.indexOf('|');
  if (delimIdx > -1) {
    to = paths.slice(delimIdx + 1);
    paths = paths.slice(0, delimIdx);
  }
  for (var i = 0; i < n; i++) {
    posix.relative(paths, to);
  }

  bench.start();
  for (i = 0; i < n; i++) {
    posix.relative(paths, to);
  }
  bench.end(n);
}

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
    ['/foo/bar/baz/quux', '/var/log'].join('|'),
  ],
  n: [1e5]
});

function main({ n, paths }) {
  let to = '';
  const delimIdx = paths.indexOf('|');
  if (delimIdx > -1) {
    to = paths.slice(delimIdx + 1);
    paths = paths.slice(0, delimIdx);
  }

  bench.start();
  for (let i = 0; i < n; i++) {
    if (i % 3 === 0)
      posix.relative(`${paths}${i}`, `${to}${i}`);
    else
      posix.relative(paths, to);
  }
  bench.end(n);
}

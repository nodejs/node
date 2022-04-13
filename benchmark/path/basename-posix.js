'use strict';
const common = require('../common.js');
const { posix } = require('path');

const bench = common.createBenchmark(main, {
  pathext: [
    '',
    '/',
    '/foo',
    '/foo/.bar.baz',
    ['/foo/.bar.baz', '.baz'].join('|'),
    'foo',
    'foo/bar.',
    ['foo/bar.', '.'].join('|'),
    '/foo/bar/baz/asdf/quux.html',
    ['/foo/bar/baz/asdf/quux.html', '.html'].join('|'),
  ],
  n: [1e5]
});

function main({ n, pathext }) {
  let ext;
  const extIdx = pathext.indexOf('|');
  if (extIdx !== -1) {
    ext = pathext.slice(extIdx + 1);
    pathext = pathext.slice(0, extIdx);
  }

  bench.start();
  for (let i = 0; i < n; i++) {
    posix.basename(i % 3 === 0 ? `${pathext}${i}` : pathext, ext);
  }
  bench.end(n);
}

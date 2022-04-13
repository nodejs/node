'use strict';
const common = require('../common.js');
const { win32 } = require('path');

const bench = common.createBenchmark(main, {
  pathext: [
    '',
    'C:\\',
    'C:\\foo',
    'D:\\foo\\.bar.baz',
    ['E:\\foo\\.bar.baz', '.baz'].join('|'),
    'foo',
    'foo\\bar.',
    ['foo\\bar.', '.'].join('|'),
    '\\foo\\bar\\baz\\asdf\\quux.html',
    ['\\foo\\bar\\baz\\asdf\\quux.html', '.html'].join('|'),
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
    win32.basename(i % 3 === 0 ? `${pathext}${i}` : pathext, ext);
  }
  bench.end(n);
}

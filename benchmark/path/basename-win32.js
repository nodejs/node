'use strict';
const common = require('../common.js');
const path = require('path');

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
    ['\\foo\\bar\\baz\\asdf\\quux.html', '.html'].join('|')
  ],
  n: [1e6]
});

function main(conf) {
  const n = +conf.n;
  const p = path.win32;
  var input = String(conf.pathext);
  var ext;
  const extIdx = input.indexOf('|');
  if (extIdx !== -1) {
    ext = input.slice(extIdx + 1);
    input = input.slice(0, extIdx);
  }

  bench.start();
  for (var i = 0; i < n; i++) {
    p.basename(input, ext);
  }
  bench.end(n);
}

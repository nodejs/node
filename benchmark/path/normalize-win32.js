'use strict';
const common = require('../common.js');
const path = require('path');

const bench = common.createBenchmark(main, {
  path: [
    '',
    '.',
    'C:\\..\\',
    'C:\\foo',
    'C:\\foo\\bar',
    'C:\\foo\\bar\\\\baz\\asdf\\quux\\..'
  ],
  n: [1e6]
});

function main(conf) {
  const n = +conf.n;
  const p = path.win32;
  const input = String(conf.path);

  bench.start();
  for (var i = 0; i < n; i++) {
    p.normalize(input);
  }
  bench.end(n);
}

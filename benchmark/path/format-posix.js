'use strict';
const common = require('../common.js');
const path = require('path');

const bench = common.createBenchmark(main, {
  props: [
    ['/', '/home/user/dir', 'index.html', '.html', 'index'].join('|')
  ],
  n: [1e7]
});

function main(conf) {
  const n = +conf.n;
  const p = path.posix;
  const props = String(conf.props).split('|');
  const obj = {
    root: props[0] || '',
    dir: props[1] || '',
    base: props[2] || '',
    ext: props[3] || '',
    name: props[4] || '',
  };

  bench.start();
  for (var i = 0; i < n; i++) {
    p.format(obj);
  }
  bench.end(n);
}

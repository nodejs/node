'use strict';
const common = require('../common.js');
const { posix } = require('path');

const bench = common.createBenchmark(main, {
  props: [
    ['/', '/home/user/dir', 'index.html', '.html', 'index'].join('|'),
  ],
  n: [1e6],
});

function main({ n, props }) {
  props = props.split('|');
  const obj = {
    root: props[0] || '',
    dir: props[1] || '',
    base: '',
    ext: props[3] || '',
    name: '',
  };

  bench.start();
  for (let i = 0; i < n; i++) {
    obj.base = `a${i}${props[2] || ''}`;
    obj.name = `a${i}${props[4] || ''}`;
    posix.format(obj);
  }
  bench.end(n);
}

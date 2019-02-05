'use strict';
const common = require('../common.js');
const { win32 } = require('path');

const bench = common.createBenchmark(main, {
  props: [
    ['C:\\', 'C:\\path\\dir', 'index.html', '.html', 'index'].join('|'),
  ],
  n: [1e7]
});

function main({ n, props }) {
  props = props.split('|');
  const obj = {
    root: props[0] || '',
    dir: props[1] || '',
    base: props[2] || '',
    ext: props[3] || '',
    name: props[4] || '',
  };

  bench.start();
  for (var i = 0; i < n; i++) {
    win32.format(obj);
  }
  bench.end(n);
}

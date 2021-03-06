'use strict';
const common = require('../common.js');
const { win32 } = require('path');

const bench = common.createBenchmark(main, {
  props: [
    ['C:\\', 'C:\\path\\dir', 'index.html', '.html', 'index'].join('|'),
  ],
  n: [1e6]
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
    win32.format(obj);
  }
  bench.end(n);
}

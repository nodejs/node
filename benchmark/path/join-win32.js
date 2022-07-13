'use strict';
const common = require('../common.js');
const { win32 } = require('path');

const bench = common.createBenchmark(main, {
  paths: [
    ['C:\\foo', 'bar', '', 'baz\\asdf', 'quux', '..'].join('|'),
  ],
  n: [1e5]
});

function main({ n, paths }) {
  const args = paths.split('|');
  const copy = [...args];
  const orig = copy[1];

  bench.start();
  for (let i = 0; i < n; i++) {
    if (i % 3 === 0) {
      copy[1] = `${orig}${i}`;
      win32.join(...copy);
    } else {
      win32.join(...args);
    }
  }
  bench.end(n);
}

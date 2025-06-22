'use strict';
const common = require('../common.js');
const { posix } = require('path');

const bench = common.createBenchmark(main, {
  paths: [
    ['/foo', 'bar', '', 'baz/asdf', 'quux', '..'].join('|'),
  ],
  n: [1e5],
});

function main({ n, paths }) {
  const args = paths.split('|');
  const copy = [...args];
  const orig = copy[1];

  bench.start();
  for (let i = 0; i < n; i++) {
    if (i % 5 === 0) {
      copy[1] = `${orig}/${i}`;
      posix.join(...copy);
    } else {
      posix.join(...args);
    }
  }
  bench.end(n);
}

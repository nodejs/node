'use strict';

const common = require('../common.js');

const bench = common.createBenchmark(main, {
  n: [1e7],
  input: [
    '',
    'a',
    'a,b',
    'a,b,c',
    'a,b,c,d',
  ],
  type: [
    'undefined',
    'and',
    'or',
  ],
}, {
  flags: ['--expose-internals'],
});

function main({ n, input, type }) {
  const {
    formatList,
  } = require('internal/errors');

  const list = input.split(',');

  if (type === 'undefined') {
    bench.start();
    for (let i = 0; i < n; ++i) {
      formatList(list);
    }
    bench.end(n);
    return;
  }

  bench.start();
  for (let i = 0; i < n; ++i) {
    formatList(list, type);
  }
  bench.end(n);
}

'use strict';

const common = require('../common.js');

const bench = common.createBenchmark(main, {
  n: [1e5],
}, {
  flags: ['--expose-internals'],
});

const instances = Array.from({ length: 1000 }).map(() => 'Uint8Array');

function main({ n }) {
  const {
    codes: {
      ERR_INVALID_ARG_TYPE,
    },
  } = require('internal/errors');
  bench.start();
  for (let i = 0; i < n; ++i)
    new ERR_INVALID_ARG_TYPE('target', instances, 'test');
  bench.end(n);
}

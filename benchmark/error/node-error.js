'use strict';

const common = require('../common');

const bench = common.createBenchmark(main, {
  n: [1e7],
}, {
  flags: ['--expose-internals']
});

function main({ n }) {
  const {
    codes: {
      ERR_INVALID_STATE,
    }
  } = require('internal/errors');
  bench.start();
  for (let i = 0; i < n; ++i)
    new ERR_INVALID_STATE.TypeError('test');
  bench.end(n);
}

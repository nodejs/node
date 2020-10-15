'use strict';

const common = require('../common.js');

const bench = common.createBenchmark(main, {
  type: ['hide-stackframes-throw', 'direct-call-throw',
         'hide-stackframes-noerr', 'direct-call-noerr'],
  n: [10e4]
}, {
  flags: ['--expose-internals']
});

function main({ n, type }) {
  const {
    hideStackFrames,
    codes: {
      ERR_INVALID_ARG_TYPE,
    },
  } = require('internal/errors');

  const testfn = (value) => {
    if (typeof value !== 'number') {
      throw new ERR_INVALID_ARG_TYPE('Benchmark', 'number', value);
    }
  };

  let fn = testfn;
  if (type.startsWith('hide-stackframe'))
    fn = hideStackFrames(testfn);
  let value = 42;
  if (type.endsWith('-throw'))
    value = 'err';

  bench.start();

  for (let i = 0; i < n; i++) {
    try {
      fn(value);
    // eslint-disable-next-line no-unused-vars
    } catch (e) {
      // No-op
    }
  }

  bench.end(n);
}

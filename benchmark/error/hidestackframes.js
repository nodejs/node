'use strict';

const common = require('../common.js');

const bench = common.createBenchmark(main, {
  type: ['hide-stackframes-throw', 'direct-call-throw',
         'hide-stackframes-noerr', 'direct-call-noerr'],
  n: [1e4],
  nested: [1, 0],
}, {
  flags: ['--expose-internals'],
});

function nestIt(fn, i = 25) {
  const nested = (...args) => {
    return fn(...args);
  };
  if (i === 0) {
    return nested;
  }
  return nestIt(nested, i - 1);
}

function main({ n, type, nested }) {
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

  let fn = type.startsWith('hide-stackframe') ?
    hideStackFrames(testfn) :
    testfn;

  if (nested) {
    fn = nestIt(fn);
  }

  let value = 42;
  if (type.endsWith('-throw'))
    value = 'err';

  bench.start();

  for (let i = 0; i < n; i++) {
    try {
      fn(value);
    } catch {
      // No-op
    }
  }

  bench.end(n);
}

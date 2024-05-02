'use strict';

const common = require('../common');
const assert = require('assert');

const bench = common.createBenchmark(main, {
  n: [1e6],
  code: [
    'built-in',
    'ERR_HTTP2_STREAM_SELF_DEPENDENCY',
    'ERR_INVALID_STATE',
    'ERR_INVALID_URL',
  ],
  stackTraceLimit: [0, 10],
}, {
  flags: ['--expose-internals'],
});

function getErrorFactory(code) {
  const {
    ERR_HTTP2_STREAM_SELF_DEPENDENCY,
    ERR_INVALID_STATE,
    ERR_INVALID_URL,
  } = require('internal/errors').codes;

  switch (code) {
    case 'built-in':
      return (n) => new Error();
    case 'ERR_HTTP2_STREAM_SELF_DEPENDENCY':
      return (n) => new ERR_HTTP2_STREAM_SELF_DEPENDENCY();
    case 'ERR_INVALID_STATE':
      return (n) => new ERR_INVALID_STATE(n + '');
    case 'ERR_INVALID_URL':
      return (n) => new ERR_INVALID_URL({ input: n + '' });
    default:
      throw new Error(`${code} not supported`);
  }
}

function main({ n, code, stackTraceLimit }) {
  const getError = getErrorFactory(code);

  Error.stackTraceLimit = stackTraceLimit;

  // Warm up.
  const length = 1024;
  const array = [];
  for (let i = 0; i < length; ++i) {
    array.push(getError(i));
  }

  bench.start();

  for (let i = 0; i < n; ++i) {
    const index = i % length;
    array[index] = getError(index);
  }

  bench.end(n);

  // Verify the entries to prevent dead code elimination from making
  // the benchmark invalid.
  for (let i = 0; i < length; ++i) {
    assert.strictEqual(typeof array[i], 'object');
  }
}

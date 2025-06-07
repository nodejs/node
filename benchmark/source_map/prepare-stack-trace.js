'use strict';

const common = require('../common.js');
const assert = require('assert');

const options = {
  flags: ['--expose-internals'],
};

const bench = common.createBenchmark(
  main,
  {
    operation: ['non-node-error'],
    n: [1e5],
  },
  options,
);

function ErrorGetCallSites() {
  const originalStackFormatter = Error.prepareStackTrace;
  Error.prepareStackTrace = (_err, stack) => {
    if (stack && stack.length > 1) {
      // Remove node:util
      return stack.slice(1);
    }
    return stack;
  };
  const err = new Error();
  // With the V8 Error API, the stack is not formatted until it is accessed
  err.stack; // eslint-disable-line no-unused-expressions
  Error.prepareStackTrace = originalStackFormatter;
  return err.stack;
}

function main({ operation, n }) {
  const { prepareStackTraceWithSourceMaps } = require('internal/source_map/prepare_stack_trace');
  const {
    ERR_ASSERTION,
  } = require('internal/errors').codes;

  const nonNodeError = new ERR_ASSERTION('non Node error');
  const nonNodeErrorStackTrace = ErrorGetCallSites();

  let preparedStackTrace;
  switch (operation) {
    case 'non-node-error':
      bench.start();
      for (let i = 0; i < n; i++) {
        preparedStackTrace = prepareStackTraceWithSourceMaps(nonNodeError, nonNodeErrorStackTrace);
      }
      bench.end(n);
      break;

    default:
      throw new Error(`Unknown operation: ${operation}`);
  }
  assert.ok(preparedStackTrace);
}

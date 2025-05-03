'use strict';

const common = require('../common.js');
const assert = require('assert');
const kIsNodeError = Symbol('kIsNodeError');

const options = {
  flags: ['--expose-internals'],
};

const bench = common.createBenchmark(
  main,
  {
    operation: ['node-error', 'non-node-error'],
    n: [1e5],
  },
  options,
);

function main({ operation, n }) {
  const { prepareStackTraceWithSourceMaps } = require('internal/source_map/prepare_stack_trace');
  const {
    ERR_ASSERTION,
  } = require('internal/errors').codes;

  const nodeError = new ERR_ASSERTION('Node error');
  nodeError[kIsNodeError] = true;
  // Stacktrace is not formatted until it is accessed
  const nodeErrorStackTrace = nodeError.stack.split('\n').slice(1);

  const nonNodeError = new ERR_ASSERTION('non Node error');
  const nonNodeErrorStackTrace = nonNodeError.stack.split('\n').slice(1);

  let preparedStackTrace;
  switch (operation) {
    case 'node-error':
      bench.start();
      for (let i = 0; i < n; i++) {
        preparedStackTrace = prepareStackTraceWithSourceMaps(nodeError, nodeErrorStackTrace);
      }
      bench.end(n);
      break;

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

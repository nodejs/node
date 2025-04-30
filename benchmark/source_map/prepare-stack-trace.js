'use strict';

const common = require('../common.js');
const kIsNodeError = Symbol('kIsNodeError');

const options = {
  flags: ['--expose-internals'],
};

const bench = common.createBenchmark(
  main,
  {
    operation: ['node-error-callsite', 'non-node-error-callsite'],
    n: [1e5],
  },
  options,
);

function main({ operation, n }) {
  const { prepareStackTraceWithSourceMaps } = require('internal/source_map/prepare_stack_trace');

  const nodeError = new Error('Simulated Node.js error');
  nodeError.name = 'NodeError';
  nodeError.code = 'ERR_SIMULATED';
  nodeError[kIsNodeError] = true;

  const nodeStackTrace = Array.from({ length: 10 }, (_, i) => ({
    getFileName: () => `file${i}.js`,
    getEvalOrigin: () => `eval at <anonymous> (eval${i}.js:1:1)`,
    getLineNumber: () => i + 1,
    getColumnNumber: () => 1,
    getFunctionName: () => `func${i}`,
    isAsync: () => false,
    isConstructor: () => false,
    getTypeName: () => null,
  }));

  const nonNodeError = new Error('Simulated non-Node.js error');
  nonNodeError.name = 'NonNodeError';

  switch (operation) {
    case 'node-error-callsite':
      bench.start();
      for (let i = 0; i < n; i++) {
        prepareStackTraceWithSourceMaps(nodeError, nodeStackTrace);
      }
      bench.end(n);
      break;

    case 'non-node-error-callsite':
      bench.start();
      for (let i = 0; i < n; i++) {
        prepareStackTraceWithSourceMaps(nonNodeError, nodeStackTrace);
      }
      bench.end(n);
      break;

    default:
      throw new Error(`Unknown operation: ${operation}`);
  }
}

'use strict';
const common = require('../common.js');
const assert = require('assert');

const bench = common.createBenchmark(main, {
  n: [1e6],
  stackCount: [99, 101],
  method: ['isInsideNodeModules', 'noop'],
}, {
  flags: ['--expose-internals', '--disable-warning=internal/test/binding'],
});

function main({ n, stackCount, method }) {
  const { internalBinding } = require('internal/test/binding');
  const { isInsideNodeModules } = internalBinding('util');

  const testFunction = method === 'noop' ?
    () => {
      bench.start();
      for (let i = 0; i < n; i++) {
        noop();
      }
      bench.end(n);
    } :
    () => {
      Error.stackTraceLimit = Infinity;
      const existingStackFrameCount = new Error().stack.split('\n').length - 1;
      assert.strictEqual(existingStackFrameCount, stackCount);

      bench.start();
      for (let i = 0; i < n; i++) {
        isInsideNodeModules();
      }
      bench.end(n);
    };

  // Excluding the message line.
  const existingStackFrameCount = new Error().stack.split('\n').length - 1;
  // Excluding the test function itself.
  nestCallStack(stackCount - existingStackFrameCount - 1, testFunction);
}

function nestCallStack(depth, callback) {
  // nestCallStack(1) already adds a stack frame, so we stop at 1.
  if (depth === 1) {
    return callback();
  }
  return nestCallStack(depth - 1, callback);
}

function noop() {}

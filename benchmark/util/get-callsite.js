'use strict';

const common = require('../common');
const { getCallSite } = require('node:util');
const assert = require('node:assert');

const bench = common.createBenchmark(main, {
  n: [1e6],
  method: ['ErrorCallSite', 'ErrorCallSiteSerialized', 'CPP'],
});

function ErrorGetCallSite() {
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

function ErrorCallSiteSerialized() {
  const callsite = ErrorGetCallSite();
  const serialized = [];
  for (let i = 0; i < callsite.length; ++i) {
    serialized.push({
      functionName: callsite[i].getFunctionName(),
      scriptName: callsite[i].getFileName(),
      lineNumber: callsite[i].getLineNumber(),
      column: callsite[i].getColumnNumber(),
    });
  }
  return serialized;
}

function main({ n, method }) {
  let fn;
  switch (method) {
    case 'ErrorCallSite':
      fn = ErrorGetCallSite;
      break;
    case 'ErrorCallSiteSerialized':
      fn = ErrorCallSiteSerialized;
      break;
    case 'CPP':
      fn = getCallSite;
      break;
  }
  let lastStack = {};

  bench.start();
  for (let i = 0; i < n; i++) {
    const stack = fn();
    lastStack = stack;
  }
  bench.end(n);
  // Attempt to avoid dead-code elimination
  assert.ok(lastStack);
}

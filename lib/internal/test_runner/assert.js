'use strict';
const {
  ArrayPrototypeMap,
  ArrayPrototypeSome,
  SafeMap,
} = primordials;
const {
  codes: {
    ERR_INVALID_ARG_TYPE,
  },
} = require('internal/errors');
const {
  validateFunction,
  validateInteger,
  validateString,
} = require('internal/validators');
const { isDeepStrictEqual } = require('internal/util/comparisons');
const { inspect } = require('internal/util/inspect');
const assert = require('assert');
const { AssertionError } = assert;
const methodsToCopy = [
  'deepEqual',
  'deepStrictEqual',
  'doesNotMatch',
  'doesNotReject',
  'doesNotThrow',
  'equal',
  'fail',
  'ifError',
  'match',
  'notDeepEqual',
  'notDeepStrictEqual',
  'notEqual',
  'notStrictEqual',
  'partialDeepStrictEqual',
  'rejects',
  'strictEqual',
  'throws',
];
let assertMap;

function getMockContext(fn) {
  const { MockFunctionContext } = require('internal/test_runner/mock/mock');
  const ctx = fn?.mock;

  if (!(ctx instanceof MockFunctionContext)) {
    throw new ERR_INVALID_ARG_TYPE('fn', 'mock function', fn);
  }

  return ctx;
}

function called(fn, message) {
  const count = getMockContext(fn).callCount();

  if (count === 0) {
    throw new AssertionError({
      __proto__: null,
      message: message ?? 'Expected mock function to have been called',
      actual: count,
      operator: 'called',
      stackStartFn: called,
    });
  }
}

function callCount(fn, times, message) {
  validateInteger(times, 'times', 0);
  const count = getMockContext(fn).callCount();

  if (count !== times) {
    throw new AssertionError({
      __proto__: null,
      message: message ??
        `Expected mock function to have been called ${times} times, but it was called ${count} times`,
      actual: count,
      expected: times,
      operator: 'callCount',
      stackStartFn: callCount,
    });
  }
}

function calledWith(fn, ...args) {
  const calls = ArrayPrototypeMap(getMockContext(fn).calls, (call) => call.arguments);

  if (!ArrayPrototypeSome(calls, (callArgs) => isDeepStrictEqual(callArgs, args))) {
    throw new AssertionError({
      __proto__: null,
      message: `Expected mock function to have been called with ${inspect(args)}\n` +
        `Received calls: ${inspect(calls)}`,
      actual: calls,
      expected: args,
      operator: 'calledWith',
      stackStartFn: calledWith,
    });
  }
}

function getAssertionMap() {
  if (assertMap === undefined) {
    assertMap = new SafeMap();

    for (let i = 0; i < methodsToCopy.length; i++) {
      assertMap.set(methodsToCopy[i], assert[methodsToCopy[i]]);
    }

    assertMap.set('called', called);
    assertMap.set('callCount', callCount);
    assertMap.set('calledWith', calledWith);
  }

  return assertMap;
}

function register(name, fn) {
  validateString(name, 'name');
  validateFunction(fn, 'fn');
  const map = getAssertionMap();
  map.set(name, fn);
}

module.exports = { getAssertionMap, register };

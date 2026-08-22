'use strict';

const {
  FunctionPrototypeBind,
  MathCeil,
  ObjectDefineProperties,
  ReflectApply,
  ReflectConstruct,
} = primordials;

const { createPerformanceNodeEntry } = require('internal/perf/performance_entry');
const { now } = require('internal/perf/utils');

const {
  validateFunction,
  validateObject,
} = require('internal/validators');

const {
  isHistogram,
} = require('internal/histogram');

const {
  codes: {
    ERR_INVALID_ARG_TYPE,
  },
} = require('internal/errors');

const {
  enqueue,
} = require('internal/perf/observe');

const {
  kEmptyObject,
} = require('internal/util');

function processComplete(name, start, args, histogram) {
  const duration = now() - start;
  if (histogram !== undefined)
    histogram.record(MathCeil(duration * 1e6));
  const entry =
    createPerformanceNodeEntry(
      name,
      'function',
      start,
      duration,
      args);

  for (let n = 0; n < args.length; n++)
    entry[n] = args[n];

  enqueue(entry);
}

function onFulfilledTimerified(name, start, args, histogram, value) {
  processComplete(name, start, args, histogram);
  return value;
}

function timerify(fn, options = kEmptyObject) {
  validateFunction(fn, 'fn');

  validateObject(options, 'options');
  const {
    histogram,
  } = options;

  if (histogram !== undefined &&
      (!isHistogram(histogram) || typeof histogram.record !== 'function')) {
    throw new ERR_INVALID_ARG_TYPE(
      'options.histogram',
      'RecordableHistogram',
      histogram);
  }

  function timerified(...args) {
    const isConstructorCall = new.target !== undefined;
    const start = now();
    const result = isConstructorCall ?
      ReflectConstruct(fn, args, fn) :
      ReflectApply(fn, this, args);
    if (!isConstructorCall && typeof result?.then === 'function') {
      // Use `then` (not `finally`) so that a rejected thenable does NOT
      // produce a performance entry or histogram record. This mirrors the
      // behavior of synchronously-thrown errors, which never reach
      // `processComplete`. Refs: https://github.com/nodejs/node/issues/42743
      // Also, thenables are only required to implement `then`; relying on
      // `finally` would break for minimal thenable implementations.
      // Pass only an `onFulfilled` handler so any rejection propagates
      // unchanged through the returned promise.
      return result.then(
        FunctionPrototypeBind(
          onFulfilledTimerified,
          result,
          fn.name,
          start,
          args,
          histogram));
    }
    processComplete(fn.name, start, args, histogram);
    return result;
  }

  ObjectDefineProperties(timerified, {
    length: {
      __proto__: null,
      configurable: false,
      enumerable: true,
      value: fn.length,
    },
    name: {
      __proto__: null,
      configurable: false,
      enumerable: true,
      value: `timerified ${fn.name}`,
    },
  });

  return timerified;
}

module.exports = timerify;

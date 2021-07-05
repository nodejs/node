'use strict';

const {
  FunctionPrototypeBind,
  ObjectDefineProperties,
  MathCeil,
  ReflectApply,
  ReflectConstruct,
  Symbol,
} = primordials;

const { InternalPerformanceEntry } = require('internal/perf/performance_entry');
const { now } = require('internal/perf/utils');

const {
  validateFunction,
  validateObject,
} = require('internal/validators');

const {
  isHistogram
} = require('internal/histogram');

const {
  isConstructor,
} = internalBinding('util');

const {
  codes: {
    ERR_INVALID_ARG_TYPE,
  },
} = require('internal/errors');

const {
  enqueue,
} = require('internal/perf/observe');

const kTimerified = Symbol('kTimerified');

function processComplete(name, start, args, histogram) {
  const duration = now() - start;
  if (histogram !== undefined)
    histogram.record(MathCeil(duration * 1e6));
  const entry =
    new InternalPerformanceEntry(
      name,
      'function',
      start,
      duration,
      args);

  for (let n = 0; n < args.length; n++)
    entry[n] = args[n];

  enqueue(entry);
}

function timerify(fn, options = {}) {
  validateFunction(fn, 'fn');

  validateObject(options, 'options');
  const {
    histogram
  } = options;

  if (histogram !== undefined &&
      (!isHistogram(histogram) || typeof histogram.record !== 'function')) {
    throw new ERR_INVALID_ARG_TYPE(
      'options.histogram',
      'RecordableHistogram',
      histogram);
  }

  if (fn[kTimerified]) return fn[kTimerified];

  const constructor = isConstructor(fn);

  function timerified(...args) {
    const start = now();
    const result = constructor ?
      ReflectConstruct(fn, args, fn) :
      ReflectApply(fn, this, args);
    if (!constructor && typeof result?.finally === 'function') {
      return result.finally(
        FunctionPrototypeBind(
          processComplete,
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
    [kTimerified]: {
      configurable: false,
      enumerable: false,
      value: timerified,
    },
    length: {
      configurable: false,
      enumerable: true,
      value: fn.length,
    },
    name: {
      configurable: false,
      enumerable: true,
      value: `timerified ${fn.name}`
    }
  });

  ObjectDefineProperties(fn, {
    [kTimerified]: {
      configurable: false,
      enumerable: false,
      value: timerified,
    }
  });

  return timerified;
}

module.exports = timerify;

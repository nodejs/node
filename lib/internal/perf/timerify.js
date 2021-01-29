'use strict';

const {
  FunctionPrototypeBind,
  ObjectDefineProperties,
  ReflectApply,
  ReflectConstruct,
  Symbol,
} = primordials;

const {
  InternalPerformanceEntry,
  now,
} = require('internal/perf/perf');

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

function processComplete(name, start, args) {
  const duration = now() - start;
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

function timerify(fn) {
  if (typeof fn !== 'function')
    throw new ERR_INVALID_ARG_TYPE('fn', 'function', fn);

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
          args));
    }
    processComplete(fn.name, start, args);
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

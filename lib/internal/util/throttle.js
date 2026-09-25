'use strict';

const {
  ArrayPrototypePush,
  ArrayPrototypeSlice,
  ObjectDefineProperties,
  PromisePrototypeThen,
  PromiseReject,
  PromiseResolve,
  PromiseWithResolvers,
  ReflectApply,
} = primordials;

const {
  AbortError,
  codes: {
    ERR_THROTTLED,
  },
} = require('internal/errors');
const {
  validateAbortSignal,
  validateBoolean,
  validateFunction,
  validateInteger,
  validateObject,
  validateOneOf,
} = require('internal/validators');
const { addAbortListener } = require('internal/events/abort_listener');
const { kEmptyObject } = require('internal/util');
const { TIMEOUT_MAX } = require('internal/timers');
const { clearTimeout, setTimeout } = require('timers');

const timersBinding = internalBinding('timers');
const { markPromiseAsHandled } = internalBinding('util');

/**
 * @typedef {object} ThrottleOptions
 * @property {number} [concurrency] The maximum number of unsettled invocations.
 * @property {number} [maxPending] The maximum number of queued calls.
 * @property {'queue'|'drop'} [overflow] How calls exceeding the limit are handled.
 * @property {AbortSignal} [signal] An AbortSignal that cancels pending and future calls.
 * @property {boolean} [strict] Whether to enforce the limit over a rolling interval.
 */

/**
 * Creates a function that invokes `fn` at most `limit` times per `interval`.
 * @param {Function} fn
 * @param {number} limit
 * @param {number} interval
 * @param {ThrottleOptions} [options]
 * @returns {Function}
 */
function throttle(fn, limit, interval, options = kEmptyObject) {
  validateFunction(fn, 'fn');
  validateInteger(limit, 'limit', 1);
  validateInteger(interval, 'interval', 0, TIMEOUT_MAX);
  validateObject(options, 'options');

  const {
    concurrency = Infinity,
    maxPending = Infinity,
    overflow = 'queue',
    signal,
    strict = false,
  } = options;

  if (concurrency !== Infinity) {
    validateInteger(concurrency, 'options.concurrency', 1);
  }
  if (maxPending !== Infinity) {
    validateInteger(maxPending, 'options.maxPending', 0);
  }
  validateOneOf(overflow, 'options.overflow', ['queue', 'drop']);
  validateAbortSignal(signal, 'options.signal');
  validateBoolean(strict, 'options.strict');

  if (signal?.aborted) {
    throw new AbortError(undefined, { __proto__: null, cause: signal.reason });
  }

  let abortError;
  let activeCount = 0;
  let pendingCalls = [];
  let pendingIndex = 0;
  let refed = true;
  let strictIndex = 0;
  let strictTicks = [];
  let timeout;
  let windowCount = 0;
  let windowStart;

  function pendingCount() {
    return pendingCalls.length - pendingIndex;
  }

  function dequeue() {
    const call = pendingCalls[pendingIndex];
    pendingCalls[pendingIndex++] = undefined;

    if (pendingIndex === pendingCalls.length) {
      pendingCalls = [];
      pendingIndex = 0;
    } else if (pendingIndex > 1024 && pendingIndex * 2 >= pendingCalls.length) {
      pendingCalls = ArrayPrototypeSlice(pendingCalls, pendingIndex);
      pendingIndex = 0;
    }

    return call;
  }

  function rejectPending(error) {
    const calls = pendingCalls;
    const start = pendingIndex;
    pendingCalls = [];
    pendingIndex = 0;
    for (let i = start; i < calls.length; i++) {
      calls[i].reject(error);
    }
  }

  function resetLimiter() {
    strictIndex = 0;
    strictTicks = [];
    windowCount = 0;
    windowStart = undefined;
  }

  function cancelWithError(error) {
    if (timeout !== undefined) {
      clearTimeout(timeout);
      timeout = undefined;
    }
    resetLimiter();
    rejectPending(error);
  }

  function cancel(reason) {
    const error = reason === undefined ?
      new AbortError() :
      new AbortError(undefined, { __proto__: null, cause: reason });
    cancelWithError(error);
  }

  function abort() {
    if (abortError === undefined) {
      abortError = signal.reason === undefined ?
        new AbortError() :
        new AbortError(undefined, { __proto__: null, cause: signal.reason });
    }
    cancelWithError(abortError);
  }

  function ref() {
    refed = true;
    timeout?.ref();
    return throttled;
  }

  function unref() {
    refed = false;
    timeout?.unref();
    return throttled;
  }

  function hasImmediateCapacity() {
    if (signal?.aborted ||
        pendingCount() !== 0 ||
        activeCount >= concurrency) {
      return false;
    }

    const now = timersBinding.getLibuvNow();
    if (strict) {
      let index = strictIndex;
      while (index < strictTicks.length &&
             now - strictTicks[index] >= interval) {
        index++;
      }
      return strictTicks.length - index < limit;
    }

    return windowStart === undefined ||
      now - windowStart >= interval ||
      windowCount < limit;
  }

  function pruneStrictTicks(now) {
    while (strictIndex < strictTicks.length &&
           now - strictTicks[strictIndex] >= interval) {
      strictIndex++;
    }

    if (strictIndex === strictTicks.length) {
      strictIndex = 0;
      strictTicks = [];
    } else if (strictIndex > 1024 && strictIndex * 2 >= strictTicks.length) {
      strictTicks = ArrayPrototypeSlice(strictTicks, strictIndex);
      strictIndex = 0;
    }
  }

  function hasCapacity(now) {
    if (strict) {
      pruneStrictTicks(now);
      return strictTicks.length - strictIndex < limit;
    }

    if (windowStart === undefined || now - windowStart >= interval) {
      windowCount = 0;
      windowStart = now;
    }
    return windowCount < limit;
  }

  function reserve(now) {
    if (strict) {
      ArrayPrototypePush(strictTicks, now);
    } else {
      windowCount++;
    }
  }

  function onSettled() {
    activeCount--;
    drain();
  }

  function invoke(call) {
    activeCount++;
    let resultPromise;
    try {
      resultPromise = PromiseResolve(ReflectApply(fn, throttled, call.args));
    } catch (error) {
      resultPromise = PromiseReject(error);
    }
    call.resolve(resultPromise);
    PromisePrototypeThen(resultPromise, onSettled, onSettled);
  }

  function drop() {
    const promise = PromiseReject(new ERR_THROTTLED());
    markPromiseAsHandled(promise);
    return promise;
  }

  function schedule() {
    if (timeout !== undefined ||
        pendingCount() === 0 ||
        activeCount >= concurrency) {
      return;
    }

    const now = timersBinding.getLibuvNow();
    if (hasCapacity(now)) {
      drain();
      return;
    }

    const delay = strict ?
      strictTicks[strictIndex] + interval - now :
      windowStart + interval - now;
    timeout = setTimeout(onTimeout, delay);
    if (!refed) timeout.unref();
  }

  function drain() {
    if (signal?.aborted) {
      abort();
      return;
    }

    while (pendingCount() !== 0 && activeCount < concurrency) {
      const now = timersBinding.getLibuvNow();
      if (!hasCapacity(now)) break;
      const call = dequeue();
      reserve(now);
      invoke(call);
    }

    schedule();
  }

  function onTimeout() {
    timeout = undefined;
    drain();
  }

  function throttled(...callArgs) {
    if (signal?.aborted) abort();
    if (abortError !== undefined) return PromiseReject(abortError);

    const now = timersBinding.getLibuvNow();
    const canInvoke = pendingCount() === 0 &&
      activeCount < concurrency &&
      hasCapacity(now);
    if (!canInvoke &&
        (overflow === 'drop' || pendingCount() >= maxPending)) {
      return drop();
    }

    const call = PromiseWithResolvers();
    call.args = callArgs;

    if (canInvoke) {
      reserve(now);
      invoke(call);
    } else {
      ArrayPrototypePush(pendingCalls, call);
      schedule();
    }

    return call.promise;
  }

  function createFn(value) {
    return {
      __proto__: null,
      configurable: true,
      enumerable: true,
      writable: true,
      value,
    };
  }

  ObjectDefineProperties(throttled, {
    __proto__: null,
    cancel: createFn(cancel),
    hasImmediateCapacity: createFn(hasImmediateCapacity),
    ref: createFn(ref),
    unref: createFn(unref),
    pending: {
      __proto__: null,
      enumerable: false,
      get() {
        return pendingCount() === 0 ? null :
          pendingCalls[pendingCalls.length - 1].promise;
      },
    },
    pendingCount: {
      __proto__: null,
      enumerable: false,
      get: pendingCount,
    },
    activeCount: {
      __proto__: null,
      enumerable: false,
      get() {
        return activeCount;
      },
    },
    length: {
      __proto__: null,
      configurable: true,
      value: fn.length,
    },
    name: {
      __proto__: null,
      configurable: true,
      value: fn.name,
    },
  });

  if (signal !== undefined) {
    addAbortListener(signal, abort);
  }

  return throttled;
}

module.exports = throttle;

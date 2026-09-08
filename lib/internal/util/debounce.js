'use strict';

const {
  ArrayPrototypePush,
  ObjectDefineProperties,
  PromiseReject,
  PromiseWithResolvers,
  ReflectApply,
} = primordials;

const {
  AbortError,
} = require('internal/errors');
const {
  validateAbortSignal,
  validateBoolean,
  validateFunction,
  validateInteger,
  validateObject,
} = require('internal/validators');
const { addAbortListener } = require('internal/events/abort_listener');
const { kEmptyObject } = require('internal/util');
const { TIMEOUT_MAX } = require('internal/timers');
const { clearTimeout, setTimeout } = require('timers');
const { markPromiseAsHandled } = internalBinding('util');

/**
 * @typedef {object} DebounceOptions
 * @property {AbortSignal} [signal] An AbortSignal that cancels pending and future calls.
 * @property {boolean} [leading] Whether to invoke on the leading edge.
 * @property {boolean} [rejectOnCancel] Whether to reject superseded calls.
 */

/**
 * Creates a function that delays calling `fn` until `wait` milliseconds have
 * elapsed since its most recent invocation.
 * @param {Function} fn
 * @param {number} wait
 * @param {DebounceOptions} [options]
 * @returns {Function}
 */
function debounce(fn, wait, options = kEmptyObject) {
  validateFunction(fn, 'fn');
  validateInteger(wait, 'wait', 0, TIMEOUT_MAX);
  validateObject(options, 'options');

  const {
    leading = false,
    rejectOnCancel = false,
    signal,
  } = options;

  validateBoolean(leading, 'options.leading');
  validateBoolean(rejectOnCancel, 'options.rejectOnCancel');
  validateAbortSignal(signal, 'options.signal');

  if (signal?.aborted) {
    throw new AbortError(undefined, { __proto__: null, cause: signal.reason });
  }

  let args;
  let abortError;
  let pendingCalls = [];
  let refed = true;
  let timeout;

  function rejectPending(error) {
    const calls = pendingCalls;
    pendingCalls = [];
    for (let i = 0; i < calls.length; i++) {
      calls[i].reject(error);
    }
  }

  function cancelWithError(error) {
    if (timeout === undefined) return;
    clearTimeout(timeout);
    timeout = undefined;
    args = undefined;
    rejectPending(error);
  }

  function cancel(reason) {
    if (timeout === undefined) return;
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
    return debounced;
  }

  function unref() {
    refed = false;
    timeout?.unref();
    return debounced;
  }

  function invoke(callArgs, calls) {
    if (signal?.aborted) {
      abort();
      for (let i = 0; i < calls.length; i++) {
        calls[i].reject(abortError);
      }
      return;
    }

    let result;
    try {
      result = ReflectApply(fn, debounced, callArgs);
    } catch (error) {
      for (let i = 0; i < calls.length; i++) {
        calls[i].reject(error);
      }
      return;
    }

    for (let i = 0; i < calls.length; i++) {
      calls[i].resolve(result);
    }
  }

  function invokePending() {
    const callArgs = args;
    args = undefined;
    const calls = pendingCalls;
    pendingCalls = [];
    if (calls.length !== 0) invoke(callArgs, calls);
  }

  function onTimeout() {
    timeout = undefined;
    invokePending();
  }

  function flush() {
    if (pendingCalls.length === 0) return;

    clearTimeout(timeout);
    timeout = undefined;
    invokePending();
  }

  function debounced(...callArgs) {
    if (signal?.aborted) abort();
    if (abortError !== undefined) return PromiseReject(abortError);

    const invokeNow = leading && timeout === undefined;
    if (timeout !== undefined) {
      if (pendingCalls.length !== 0) {
        markPromiseAsHandled(pendingCalls[pendingCalls.length - 1].promise);
        if (rejectOnCancel) {
          rejectPending(new AbortError('The debounced call was superseded'));
        }
      }
      timeout.refresh();
    } else {
      timeout = setTimeout(onTimeout, wait);
      if (!refed) timeout.unref();
    }

    if (signal?.aborted) {
      abort();
      return PromiseReject(abortError);
    }

    const call = PromiseWithResolvers();
    if (invokeNow) {
      invoke(callArgs, [call]);
    } else {
      ArrayPrototypePush(pendingCalls, call);
      args = callArgs;
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

  ObjectDefineProperties(debounced, {
    __proto__: null,
    cancel: createFn(cancel),
    flush: createFn(flush),
    ref: createFn(ref),
    unref: createFn(unref),
    pending: {
      __proto__: null,
      enumerable: false,
      get() {
        return pendingCalls.length === 0 ? null : pendingCalls[pendingCalls.length - 1].promise;
      },
    },
    pendingCount: {
      __proto__: null,
      enumerable: false,
      get() { return pendingCalls.length; },
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

  return debounced;
}

module.exports = debounce;

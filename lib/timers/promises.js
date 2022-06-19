'use strict';

const {
  FunctionPrototypeBind,
  Promise,
  PromiseReject,
  ReflectConstruct,
  SafePromisePrototypeFinally,
  Symbol,
} = primordials;

const {
  Timeout,
  Immediate,
  insert
} = require('internal/timers');
const {
  clearImmediate,
  clearInterval,
  clearTimeout,
} = require('timers');

const {
  AbortError,
  codes: {
    ERR_ILLEGAL_CONSTRUCTOR,
    ERR_INVALID_ARG_TYPE,
    ERR_INVALID_THIS,
  }
} = require('internal/errors');

const {
  validateAbortSignal,
  validateBoolean,
  validateObject,
} = require('internal/validators');

const {
  kEmptyObject,
} = require('internal/util');

const kScheduler = Symbol('kScheduler');

function cancelListenerHandler(clear, reject, signal) {
  if (!this._destroyed) {
    clear(this);
    reject(new AbortError(undefined, { cause: signal?.reason }));
  }
}

function setTimeout(after, value, options = kEmptyObject) {
  const args = value !== undefined ? [value] : value;
  if (options == null || typeof options !== 'object') {
    return PromiseReject(
      new ERR_INVALID_ARG_TYPE(
        'options',
        'Object',
        options));
  }
  const { signal, ref = true } = options;
  try {
    validateAbortSignal(signal, 'options.signal');
  } catch (err) {
    return PromiseReject(err);
  }
  if (typeof ref !== 'boolean') {
    return PromiseReject(
      new ERR_INVALID_ARG_TYPE(
        'options.ref',
        'boolean',
        ref));
  }
  // TODO(@jasnell): If a decision is made that this cannot be backported
  // to 12.x, then this can be converted to use optional chaining to
  // simplify the check.
  if (signal && signal.aborted) {
    return PromiseReject(new AbortError(undefined, { cause: signal.reason }));
  }
  let oncancel;
  const ret = new Promise((resolve, reject) => {
    const timeout = new Timeout(resolve, after, args, false, ref);
    insert(timeout, timeout._idleTimeout);
    if (signal) {
      oncancel = FunctionPrototypeBind(cancelListenerHandler,
                                       timeout, clearTimeout, reject, signal);
      signal.addEventListener('abort', oncancel);
    }
  });
  return oncancel !== undefined ?
    SafePromisePrototypeFinally(
      ret,
      () => signal.removeEventListener('abort', oncancel)) : ret;
}

function setImmediate(value, options = kEmptyObject) {
  if (options == null || typeof options !== 'object') {
    return PromiseReject(
      new ERR_INVALID_ARG_TYPE(
        'options',
        'Object',
        options));
  }
  const { signal, ref = true } = options;
  try {
    validateAbortSignal(signal, 'options.signal');
  } catch (err) {
    return PromiseReject(err);
  }
  if (typeof ref !== 'boolean') {
    return PromiseReject(
      new ERR_INVALID_ARG_TYPE(
        'options.ref',
        'boolean',
        ref));
  }
  // TODO(@jasnell): If a decision is made that this cannot be backported
  // to 12.x, then this can be converted to use optional chaining to
  // simplify the check.
  if (signal && signal.aborted) {
    return PromiseReject(new AbortError(undefined, { cause: signal.reason }));
  }
  let oncancel;
  const ret = new Promise((resolve, reject) => {
    const immediate = new Immediate(resolve, [value]);
    if (!ref) immediate.unref();
    if (signal) {
      oncancel = FunctionPrototypeBind(cancelListenerHandler,
                                       immediate, clearImmediate, reject,
                                       signal);
      signal.addEventListener('abort', oncancel);
    }
  });
  return oncancel !== undefined ?
    SafePromisePrototypeFinally(
      ret,
      () => signal.removeEventListener('abort', oncancel)) : ret;
}

async function* setInterval(after, value, options = kEmptyObject) {
  validateObject(options, 'options');
  const { signal, ref = true } = options;
  validateAbortSignal(signal, 'options.signal');
  validateBoolean(ref, 'options.ref');

  if (signal?.aborted)
    throw new AbortError(undefined, { cause: signal?.reason });

  let onCancel;
  let interval;
  try {
    let notYielded = 0;
    let callback;
    interval = new Timeout(() => {
      notYielded++;
      if (callback) {
        callback();
        callback = undefined;
      }
    }, after, undefined, true, ref);
    insert(interval, interval._idleTimeout);
    if (signal) {
      onCancel = () => {
        clearInterval(interval);
        if (callback) {
          callback(
            PromiseReject(
              new AbortError(undefined, { cause: signal.reason })));
          callback = undefined;
        }
      };
      signal.addEventListener('abort', onCancel, { once: true });
    }

    while (!signal?.aborted) {
      if (notYielded === 0) {
        await new Promise((resolve) => callback = resolve);
      }
      for (; notYielded > 0; notYielded--) {
        yield value;
      }
    }
    throw new AbortError(undefined, { cause: signal?.reason });
  } finally {
    clearInterval(interval);
    signal?.removeEventListener('abort', onCancel);
  }
}

// TODO(@jasnell): Scheduler is an API currently being discussed by WICG
// for Web Platform standardization: https://github.com/WICG/scheduling-apis
// The scheduler.yield() and scheduler.wait() methods correspond roughly to
// the awaitable setTimeout and setImmediate implementations here. This api
// should be considered to be experimental until the spec for these are
// finalized. Note, also, that Scheduler is expected to be defined as a global,
// but while the API is experimental we shouldn't expose it as such.
class Scheduler {
  constructor() {
    throw new ERR_ILLEGAL_CONSTRUCTOR();
  }

  /**
   * @returns {Promise<void>}
   */
  yield() {
    if (!this[kScheduler])
      throw new ERR_INVALID_THIS('Scheduler');
    return setImmediate();
  }

  /**
   * @typedef {import('../internal/abort_controller').AbortSignal} AbortSignal
   * @param {number} delay
   * @param {{ signal?: AbortSignal }} [options]
   * @returns {Promise<void>}
   */
  wait(delay, options) {
    if (!this[kScheduler])
      throw new ERR_INVALID_THIS('Scheduler');
    return setTimeout(delay, undefined, { signal: options?.signal });
  }
}

module.exports = {
  setTimeout,
  setImmediate,
  setInterval,
  scheduler: ReflectConstruct(function() {
    this[kScheduler] = true;
  }, [], Scheduler),
};

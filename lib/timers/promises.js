'use strict';

const {
  FunctionPrototypeBind,
  Promise,
  PromiseReject,
  SafePromisePrototypeFinally,
} = primordials;

const {
  Timeout,
  Immediate,
  insert
} = require('internal/timers');

const {
  AbortError,
  codes: { ERR_INVALID_ARG_TYPE }
} = require('internal/errors');

const {
  validateAbortSignal,
  validateBoolean,
  validateObject,
} = require('internal/validators');

function cancelListenerHandler(clear, reject) {
  if (!this._destroyed) {
    clear(this);
    reject(new AbortError());
  }
}

function setTimeout(after, value, options = {}) {
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
    return PromiseReject(new AbortError());
  }
  let oncancel;
  const ret = new Promise((resolve, reject) => {
    const timeout = new Timeout(resolve, after, args, false, ref);
    insert(timeout, timeout._idleTimeout);
    if (signal) {
      oncancel = FunctionPrototypeBind(cancelListenerHandler,
                                       // eslint-disable-next-line no-undef
                                       timeout, clearTimeout, reject);
      signal.addEventListener('abort', oncancel);
    }
  });
  return oncancel !== undefined ?
    SafePromisePrototypeFinally(
      ret,
      () => signal.removeEventListener('abort', oncancel)) : ret;
}

function setImmediate(value, options = {}) {
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
    return PromiseReject(new AbortError());
  }
  let oncancel;
  const ret = new Promise((resolve, reject) => {
    const immediate = new Immediate(resolve, [value]);
    if (!ref) immediate.unref();
    if (signal) {
      oncancel = FunctionPrototypeBind(cancelListenerHandler,
                                       // eslint-disable-next-line no-undef
                                       immediate, clearImmediate, reject);
      signal.addEventListener('abort', oncancel);
    }
  });
  return oncancel !== undefined ?
    SafePromisePrototypeFinally(
      ret,
      () => signal.removeEventListener('abort', oncancel)) : ret;
}

async function* setInterval(after, value, options = {}) {
  validateObject(options, 'options');
  const { signal, ref = true } = options;
  validateAbortSignal(signal, 'options.signal');
  validateBoolean(ref, 'options.ref');

  if (signal?.aborted)
    throw new AbortError();

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
        // eslint-disable-next-line no-undef
        clearInterval(interval);
        if (callback) {
          callback(PromiseReject(new AbortError()));
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
    throw new AbortError();
  } finally {
    // eslint-disable-next-line no-undef
    clearInterval(interval);
    signal?.removeEventListener('abort', onCancel);
  }
}

module.exports = {
  setTimeout,
  setImmediate,
  setInterval,
};

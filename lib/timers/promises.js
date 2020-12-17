'use strict';

const {
  Symbol,
  FunctionPrototypeBind,
  Promise,
  PromisePrototypeFinally,
  PromiseResolve,
  PromiseReject,
} = primordials;

const {
  Timeout,
  Immediate,
  insert
} = require('internal/timers');

const {
  AbortError,
  codes: { ERR_INVALID_ARG_TYPE, ERR_INVALID_ARG_VALUE }
} = require('internal/errors');

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
  if (signal !== undefined &&
      (signal === null ||
       typeof signal !== 'object' ||
       !('aborted' in signal))) {
    return PromiseReject(
      new ERR_INVALID_ARG_TYPE(
        'options.signal',
        'AbortSignal',
        signal));
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
    const timeout = new Timeout(resolve, after, args, false, true);
    if (!ref) timeout.unref();
    insert(timeout, timeout._idleTimeout);
    if (signal) {
      oncancel = FunctionPrototypeBind(cancelListenerHandler,
                                       // eslint-disable-next-line no-undef
                                       timeout, clearTimeout, reject);
      signal.addEventListener('abort', oncancel);
    }
  });
  return oncancel !== undefined ?
    PromisePrototypeFinally(
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
  if (signal !== undefined &&
    (signal === null ||
     typeof signal !== 'object' ||
     !('aborted' in signal))) {
    return PromiseReject(
      new ERR_INVALID_ARG_TYPE(
        'options.signal',
        'AbortSignal',
        signal));
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
    PromisePrototypeFinally(
      ret,
      () => signal.removeEventListener('abort', oncancel)) : ret;
}

function setInterval(after, value, options = {}) {
  const args = value !== undefined ? [value] : value;
  if (options == null || typeof options !== 'object') {
    throw new ERR_INVALID_ARG_TYPE(
      'options',
      'Object',
      options);
  }
  const {
    signal,
    ref = true,
    // Defers start of timers until the first iteration
    wait = false,
    // This has each invocation of iterator.next set up a new timeout
    timeout: asTimeout = false,
    // Skips intervals that are missed
    skip = false
    // Clears entire queue of callbacks when skip = true and the callbacks well missed the timeout
  } = options;
  if (signal !== undefined &&
    (signal === null ||
      typeof signal !== 'object' ||
      !('aborted' in signal))) {
    throw new ERR_INVALID_ARG_TYPE(
      'options.signal',
      'AbortSignal',
      signal);
  }
  if (typeof ref !== 'boolean') {
    throw new ERR_INVALID_ARG_TYPE(
      'options.ref',
      'boolean',
      ref);
  }
  return {
    [Symbol.asyncIterator]() {
      // asTimeout can't skip as they always have intervals between each iteration
      const resolveEarlyEnabled = !asTimeout && skip;
      let timeout,
        callbacks = [],
        active = true,
        missed = 0;

      setIntervalCycle();

      const iterator = {
        async next() {
          if (!active) {
            return {
              done: true,
              value: undefined
            };
          }
          // TODO(@jasnell): If a decision is made that this cannot be backported
          // to 12.x, then this can be converted to use optional chaining to
          // simplify the check.
          if (signal && signal.aborted) {
            return PromiseReject(new AbortError());
          }
          return new Promise(
            (resolve, reject) => {
              callbacks.push({ resolve, reject });
              if (missed > 0) {
                resolveNext();
              }
              setIntervalCycle();
            }
          );
        },
        async return() {
          active = false;
          clear();
          resolveAll({
            done: true,
            value: undefined
          });
          if (signal) {
            signal.removeEventListener('abort', oncancel);
          }
          return {
            done: true,
            value: undefined
          };
        }
      };
      if (signal) {
        signal.addEventListener('abort', oncancel, { once: true });
      }
      return iterator;

      function setIntervalCycle() {
        if (!active) {
          return;
        }
        if (timeout) {
          return;
        }
        // Wait and asTimeout both imply a callback is required before setting up a timeout
        if (!callbacks.length && (wait || asTimeout)) {
          return;
        }
        missed = 0;
        const currentTimeout = timeout = new Timeout(() => {
          if (asTimeout && currentTimeout === timeout) {
            // No need to clear here as we set to not repeat for asTimeout
            timeout = undefined;
          }
          resolveNext();
        }, after, undefined, !asTimeout, true);
        if (!ref) timeout.unref();
        insert(timeout, timeout._idleTimeout);
      }

      function resolveNext() {
        if (!callbacks.length) {
          if (resolveEarlyEnabled) {
            missed += 1;
          }
          return;
        }
        const deferred = callbacks.shift();
        if (deferred) {
          const { resolve } = deferred;
          resolve({
            done: false,
            value
          });
          missed -= 1;
        }
        if (missed > 0 && callbacks.length) {
          // Loop till we have completed each missed interval that we have a callback for
          resolveNext();
        }
      }

      function resolveAll(value) {
        callbacks.forEach(({ resolve }) => resolve(value));
        callbacks = [];
      }

      function rejectAll(error) {
        callbacks.forEach(({ reject }) => reject(error));
        callbacks = [];
      }

      function clear() {
        if (timeout) {
          // eslint-disable-next-line no-undef
          clearTimeout(timeout);
          timeout = undefined;
        }
      }

      function oncancel() {
        clear();
        rejectAll(new AbortError());
      }

    }
  };
}

module.exports = {
  setTimeout,
  setImmediate,
  setInterval,
};

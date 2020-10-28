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
  codes: { ERR_INVALID_ARG_TYPE }
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
  const { signal, ref = true } = options;
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
      let timeout,
        deferred;
      let active = true;
      const iterator = {
        next() {
          if (!active) {
            return PromiseResolve({
              done: true,
              value: undefined
            });
          }
          // TODO(@jasnell): If a decision is made that this cannot
          // be backported to 12.x, then this can be converted to
          // use optional chaining to simplify the check.
          if (signal && signal.aborted) {
            return PromiseReject(
              lazyDOMException('The operation was aborted', 'AbortError')
            );
          }
          const promise = new Promise(
            (resolve, reject) => {
              deferred = { resolve, reject };
            }
          );
          timeout = new Timeout((value) => {
            if (deferred) {
              deferred.resolve({
                done: false,
                value
              });
              deferred = undefined;
            }
          }, after, args, false, true);
          if (!ref) timeout.unref();
          insert(timeout, timeout._idleTimeout);
          return promise;
        },
        return() {
          active = false;
          if (timeout) {
            // eslint-disable-next-line no-undef
            clearTimeout(timeout);
          }
          if (deferred) {
            deferred.resolve({
              done: true,
              value: undefined
            });
            deferred = undefined;
          }
          if (signal) {
            signal.removeEventListener('abort', onAbort);
          }
          return PromiseResolve({
            done: true,
            value: undefined
          });
        }
      };
      if (signal) {
        signal.addEventListener('abort', onAbort, { once: true });
      }
      return iterator;

      function onAbort() {
        if (deferred) {
          deferred.reject(
            lazyDOMException('The operation was aborted', 'AbortError')
          );
          deferred = undefined;
        }
      }
    }
  };
}

module.exports = {
  setTimeout,
  setImmediate,
  setInterval,
};

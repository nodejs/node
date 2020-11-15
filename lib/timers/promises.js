'use strict';

const {
  FunctionPrototypeBind,
  Promise,
  PromisePrototypeFinally,
  PromiseReject,
} = primordials;

const {
  Timeout,
  Immediate,
  insert
} = require('internal/timers');

const {
  hideStackFrames,
  codes: { ERR_INVALID_ARG_TYPE }
} = require('internal/errors');

let DOMException;

const lazyDOMException = hideStackFrames((message, name) => {
  if (DOMException === undefined)
    DOMException = internalBinding('messaging').DOMException;
  return new DOMException(message, name);
});

function cancelListenerHandler(clear, reject) {
  if (!this._destroyed) {
    clear(this);
    reject(lazyDOMException('The operation was aborted', 'AbortError'));
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
    return PromiseReject(
      lazyDOMException('The operation was aborted', 'AbortError'));
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
    return PromiseReject(
      lazyDOMException('The operation was aborted', 'AbortError'));
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

module.exports = {
  setTimeout,
  setImmediate,
};

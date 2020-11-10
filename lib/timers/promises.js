'use strict';

const {
  Promise,
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

const lazyDOMException = hideStackFrames((message) => {
  if (DOMException === undefined)
    DOMException = internalBinding('messaging').DOMException;
  return new DOMException(message);
});

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
  if (signal && signal.aborted)
    return PromiseReject(lazyDOMException('AbortError'));
  return new Promise((resolve, reject) => {
    const timeout = new Timeout(resolve, after, args, false, true);
    if (!ref) timeout.unref();
    insert(timeout, timeout._idleTimeout);
    if (signal) {
      signal.addEventListener('abort', () => {
        if (!timeout._destroyed) {
          // eslint-disable-next-line no-undef
          clearTimeout(timeout);
          reject(lazyDOMException('AbortError'));
        }
      }, { once: true });
    }
  });
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
  if (signal && signal.aborted)
    return PromiseReject(lazyDOMException('AbortError'));
  return new Promise((resolve, reject) => {
    const immediate = new Immediate(resolve, [value]);
    if (!ref) immediate.unref();
    if (signal) {
      signal.addEventListener('abort', () => {
        if (!immediate._destroyed) {
          // eslint-disable-next-line no-undef
          clearImmediate(immediate);
          reject(lazyDOMException('AbortError'));
        }
      }, { once: true });
    }
  });
}

module.exports = {
  setTimeout,
  setImmediate,
};

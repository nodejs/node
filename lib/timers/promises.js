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

const lazyDOMException = hideStackFrames((message, name) => {
  if (DOMException === undefined)
    DOMException = internalBinding('messaging').DOMException;
  return new DOMException(message, name);
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
  if (signal && signal.aborted) {
    return PromiseReject(
      lazyDOMException('The operation was aborted', 'AbortError'));
  }
  return new Promise((resolve, reject) => {
    let signalHandler = null;
    function cleanupSignalHandlerThenResolve(arg) {
      signal.removeEventListener('abort', signalHandler, { once: true });
      resolve(arg);
    }
    const onDone = signal ? cleanupSignalHandlerThenResolve : resolve;
    const timeout = new Timeout(onDone, after, args, false, true);
    if (!ref) timeout.unref();
    insert(timeout, timeout._idleTimeout);
    if (signal) {
      signalHandler = () => {
        if (!timeout._destroyed) {
          // eslint-disable-next-line no-undef
          clearTimeout(timeout);
          reject(lazyDOMException('The operation was aborted', 'AbortError'));
        }
      };
      signal.addEventListener('abort', signalHandler, { once: true });
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
  if (signal && signal.aborted) {
    return PromiseReject(
      lazyDOMException('The operation was aborted', 'AbortError'));
  }
  return new Promise((resolve, reject) => {
    const immediate = new Immediate(resolve, [value]);
    if (!ref) immediate.unref();
    if (signal) {
      signal.addEventListener('abort', () => {
        if (!immediate._destroyed) {
          // eslint-disable-next-line no-undef
          clearImmediate(immediate);
          reject(lazyDOMException('The operation was aborted', 'AbortError'));
        }
      }, { once: true });
    }
  });
}

module.exports = {
  setTimeout,
  setImmediate,
};

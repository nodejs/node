// Ported from https://github.com/mafintosh/end-of-stream with
// permission from the author, Mathias Buus (@mafintosh).

'use strict';

const {
  Promise,
  PromisePrototypeThen,
  ReflectApply,
  Symbol,
  SymbolDispose,
} = primordials;

const {
  AbortError,
  codes: {
    ERR_INVALID_ARG_TYPE,
    ERR_STREAM_PREMATURE_CLOSE,
  },
} = require('internal/errors');
const {
  kEmptyObject,
  once,
} = require('internal/util');
const {
  validateAbortSignal,
  validateFunction,
  validateObject,
  validateBoolean,
} = require('internal/validators');

const {
  isClosed,
  isReadable,
  isReadableNodeStream,
  isReadableStream,
  isReadableFinished,
  isReadableErrored,
  isWritable,
  isWritableNodeStream,
  isWritableStream,
  isWritableFinished,
  isWritableErrored,
  isNodeStream,
  willEmitClose: _willEmitClose,
  kIsClosedPromise,
} = require('internal/streams/utils');

const { enabledHooksExist } = require('internal/async_hooks');
const AsyncContextFrame = require('internal/async_context_frame');

// Lazy load
let AsyncResource;
let addAbortListener;

function isRequest(stream) {
  return stream.setHeader && typeof stream.abort === 'function';
}

const nop = () => {};

function bindAsyncResource(fn, type) {
  AsyncResource ??= require('async_hooks').AsyncResource;
  const resource = new AsyncResource(type);
  return function(...args) {
    return resource.runInAsyncScope(fn, this, ...args);
  };
}

const kEosImmediateClose = Symbol('kEosImmediateClose');

/**
 * Returns the current stream error tracked by eos(), if any.
 * @param {import('stream').Stream} stream
 * @returns {Error | null}
 */
function getEosErrored(stream) {
  const errored = isWritableErrored(stream) || isReadableErrored(stream);
  return typeof errored !== 'boolean' && errored || null;
}

/**
 * Returns the error eos() would report from an immediate close, including
 * premature close detection for unfinished readable or writable sides.
 * @param {import('stream').Stream} stream
 * @param {boolean} readable
 * @param {boolean | null} readableFinished
 * @param {boolean} writable
 * @param {boolean | null} writableFinished
 * @returns {Error | typeof kEosImmediateClose}
 */
function getEosOnCloseError(stream, readable, readableFinished, writable, writableFinished) {
  const errored = getEosErrored(stream);
  if (errored) {
    return errored;
  }

  if (readable && !readableFinished && isReadableNodeStream(stream, true)) {
    if (!isReadableFinished(stream, false)) {
      return new ERR_STREAM_PREMATURE_CLOSE();
    }
  }
  if (writable && !writableFinished) {
    if (!isWritableFinished(stream, false)) {
      return new ERR_STREAM_PREMATURE_CLOSE();
    }
  }

  return kEosImmediateClose;
}

function getEosInitialState(stream, options = kEmptyObject) {
  const readable = options.readable ?? isReadableNodeStream(stream);
  const writable = options.writable ?? isWritableNodeStream(stream);

  // TODO (ronag): Improve soft detection to include core modules and
  // common ecosystem modules that do properly emit 'close' but fail
  // this generic check.
  const willEmitClose = (
    _willEmitClose(stream) &&
    isReadableNodeStream(stream) === readable &&
    isWritableNodeStream(stream) === writable
  );

  return {
    readable,
    writable,
    willEmitClose,
    writableFinished: isWritableFinished(stream, false),
    readableFinished: isReadableFinished(stream, false),
    closed: isClosed(stream),
  };
}

/**
 * Classifies whether eos() can synchronously determine the result for the
 * current stream snapshot, or if it must defer to future events.
 * @param {import('stream').Stream} stream
 * @param {object} [options]
 * @param {boolean} [options.readable]
 * @param {boolean} [options.writable]
 * @param {ReturnType<typeof getEosInitialState>} [state]
 * @returns {Error | typeof kEosImmediateClose | null}
 */
function getEosImmediateResult(stream, options = kEmptyObject, state = getEosInitialState(stream, options)) {
  const {
    readable,
    writable,
    willEmitClose,
    writableFinished,
    readableFinished,
    closed,
  } = state;
  const wState = stream._writableState;
  const rState = stream._readableState;

  if (closed) {
    return getEosOnCloseError(
      stream,
      readable,
      readableFinished,
      writable,
      writableFinished,
    );
  } else if (wState?.errorEmitted || rState?.errorEmitted) {
    if (!willEmitClose) {
      return getEosErrored(stream) ?? kEosImmediateClose;
    }
  } else if (
    !readable &&
    (!willEmitClose || isReadable(stream)) &&
    (writableFinished || isWritable(stream) === false) &&
    (wState == null || wState.pendingcb === undefined || wState.pendingcb === 0)
  ) {
    return getEosErrored(stream) ?? kEosImmediateClose;
  } else if (
    !writable &&
    (!willEmitClose || isWritable(stream)) &&
    (readableFinished || isReadable(stream) === false)
  ) {
    return getEosErrored(stream) ?? kEosImmediateClose;
  } else if ((rState && stream.req && stream.aborted)) {
    return getEosErrored(stream) ?? kEosImmediateClose;
  }

  return null;
}

function eos(stream, options, callback) {
  if (arguments.length === 2) {
    callback = options;
    options = kEmptyObject;
  } else if (options == null) {
    options = kEmptyObject;
  } else {
    validateObject(options, 'options');
  }
  validateFunction(callback, 'callback');
  validateAbortSignal(options.signal, 'options.signal');

  if (AsyncContextFrame.current() || enabledHooksExist()) {
    // Avoid AsyncResource.bind() because it calls ObjectDefineProperties which
    // is a bottleneck here.
    callback = once(bindAsyncResource(callback, 'STREAM_END_OF_STREAM'));
  } else {
    callback = once(callback);
  }

  if (isReadableStream(stream) || isWritableStream(stream)) {
    return eosWeb(stream, options, callback);
  }

  if (!isNodeStream(stream)) {
    throw new ERR_INVALID_ARG_TYPE('stream', ['ReadableStream', 'WritableStream', 'Stream'], stream);
  }

  const eosState = getEosInitialState(stream, options);
  const wState = stream._writableState;

  const onlegacyfinish = () => {
    if (!stream.writable) {
      onfinish();
    }
  };

  const { readable, writable } = eosState;
  let {
    willEmitClose,
    writableFinished,
    readableFinished,
    closed,
  } = eosState;
  const onfinish = () => {
    writableFinished = true;
    // Stream should not be destroyed here. If it is that
    // means that user space is doing something differently and
    // we cannot trust willEmitClose.
    if (stream.destroyed) {
      willEmitClose = false;
    }

    if (willEmitClose && (!stream.readable || readable)) {
      return;
    }

    if (!readable || readableFinished) {
      callback.call(stream);
    }
  };

  const onend = () => {
    readableFinished = true;
    // Stream should not be destroyed here. If it is that
    // means that user space is doing something differently and
    // we cannot trust willEmitClose.
    if (stream.destroyed) {
      willEmitClose = false;
    }

    if (willEmitClose && (!stream.writable || writable)) {
      return;
    }

    if (!writable || writableFinished) {
      callback.call(stream);
    }
  };

  const onerror = (err) => {
    callback.call(stream, err);
  };

  const onclose = () => {
    closed = true;

    const error = getEosOnCloseError(stream, readable, readableFinished, writable, writableFinished);
    if (error === kEosImmediateClose) {
      callback.call(stream);
    } else {
      callback.call(stream, error);
    }
  };

  const onrequest = () => {
    stream.req.on('finish', onfinish);
  };

  if (isRequest(stream)) {
    stream.on('complete', onfinish);
    if (!willEmitClose) {
      stream.on('abort', onclose);
    }
    if (stream.req) {
      onrequest();
    } else {
      stream.on('request', onrequest);
    }
  } else if (writable && !wState) { // legacy streams
    stream.on('end', onlegacyfinish);
    stream.on('close', onlegacyfinish);
  }

  // Not all streams will emit 'close' after 'aborted'.
  if (!willEmitClose && typeof stream.aborted === 'boolean') {
    stream.on('aborted', onclose);
  }

  stream.on('end', onend);
  stream.on('finish', onfinish);
  if (options.error !== false) {
    stream.on('error', onerror);
  }
  stream.on('close', onclose);

  const immediateResult = getEosImmediateResult(stream, options, eosState);
  if (immediateResult !== null) {
    if (immediateResult === kEosImmediateClose) {
      process.nextTick(() => callback.call(stream));
    } else {
      process.nextTick(() => callback.call(stream, immediateResult));
    }
  }

  const cleanup = () => {
    callback = nop;
    stream.removeListener('aborted', onclose);
    stream.removeListener('complete', onfinish);
    stream.removeListener('abort', onclose);
    stream.removeListener('request', onrequest);
    if (stream.req) stream.req.removeListener('finish', onfinish);
    stream.removeListener('end', onlegacyfinish);
    stream.removeListener('close', onlegacyfinish);
    stream.removeListener('finish', onfinish);
    stream.removeListener('end', onend);
    stream.removeListener('error', onerror);
    stream.removeListener('close', onclose);
  };

  if (options.signal && !closed) {
    const abort = () => {
      // Keep it because cleanup removes it.
      const endCallback = callback;
      cleanup();
      endCallback.call(
        stream,
        new AbortError(undefined, { cause: options.signal.reason }));
    };
    if (options.signal.aborted) {
      process.nextTick(abort);
    } else {
      addAbortListener ??= require('internal/events/abort_listener').addAbortListener;
      const disposable = addAbortListener(options.signal, abort);
      const originalCallback = callback;
      callback = once((...args) => {
        disposable[SymbolDispose]();
        ReflectApply(originalCallback, stream, args);
      });
    }
  }

  return cleanup;
}

function eosWeb(stream, options, callback) {
  let isAborted = false;
  let abort = nop;
  if (options.signal) {
    abort = () => {
      isAborted = true;
      callback.call(stream, new AbortError(undefined, { cause: options.signal.reason }));
    };
    if (options.signal.aborted) {
      process.nextTick(abort);
    } else {
      addAbortListener ??= require('internal/events/abort_listener').addAbortListener;
      const disposable = addAbortListener(options.signal, abort);
      const originalCallback = callback;
      callback = once((...args) => {
        disposable[SymbolDispose]();
        ReflectApply(originalCallback, stream, args);
      });
    }
  }
  const resolverFn = (...args) => {
    if (!isAborted) {
      process.nextTick(() => ReflectApply(callback, stream, args));
    }
  };
  PromisePrototypeThen(
    stream[kIsClosedPromise].promise,
    resolverFn,
    resolverFn,
  );
  return nop;
}

function finished(stream, opts) {
  let autoCleanup = false;
  if (opts === null) {
    opts = kEmptyObject;
  }
  if (opts?.cleanup) {
    validateBoolean(opts.cleanup, 'cleanup');
    autoCleanup = opts.cleanup;
  }
  return new Promise((resolve, reject) => {
    const cleanup = eos(stream, opts, (err) => {
      if (autoCleanup) {
        cleanup();
      }
      if (err) {
        reject(err);
      } else {
        resolve();
      }
    });
  });
}

module.exports = {
  eos,
  finished,
  getEosImmediateResult,
  kEosImmediateClose,
};

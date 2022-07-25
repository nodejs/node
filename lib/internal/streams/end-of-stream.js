// Ported from https://github.com/mafintosh/end-of-stream with
// permission from the author, Mathias Buus (@mafintosh).

'use strict';

const {
  AbortError,
  codes,
} = require('internal/errors');
const {
  ERR_INVALID_ARG_TYPE,
  ERR_STREAM_PREMATURE_CLOSE
} = codes;
const {
  kEmptyObject,
  once,
} = require('internal/util');
const {
  validateAbortSignal,
  validateFunction,
  validateObject,
} = require('internal/validators');

const { Promise } = primordials;

const {
  isClosed,
  isReadable,
  isReadableNodeStream,
  isReadableFinished,
  isReadableErrored,
  isWritable,
  isWritableNodeStream,
  isWritableFinished,
  isWritableErrored,
  isNodeStream,
  willEmitClose: _willEmitClose,
} = require('internal/streams/utils');

function isRequest(stream) {
  return stream.setHeader && typeof stream.abort === 'function';
}

const nop = () => {};

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

  callback = once(callback);

  const readable = options.readable ?? isReadableNodeStream(stream);
  const writable = options.writable ?? isWritableNodeStream(stream);

  if (!isNodeStream(stream)) {
    // TODO: Webstreams.
    throw new ERR_INVALID_ARG_TYPE('stream', 'Stream', stream);
  }

  const wState = stream._writableState;
  const rState = stream._readableState;

  const onlegacyfinish = () => {
    if (!stream.writable) {
      onfinish();
    }
  };

  // TODO (ronag): Improve soft detection to include core modules and
  // common ecosystem modules that do properly emit 'close' but fail
  // this generic check.
  let willEmitClose = (
    _willEmitClose(stream) &&
    isReadableNodeStream(stream) === readable &&
    isWritableNodeStream(stream) === writable
  );

  let writableFinished = isWritableFinished(stream, false);
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

  let readableFinished = isReadableFinished(stream, false);
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

  let closed = isClosed(stream);

  const onclose = () => {
    closed = true;

    const errored = isWritableErrored(stream) || isReadableErrored(stream);

    if (errored && typeof errored !== 'boolean') {
      return callback.call(stream, errored);
    }

    if (readable && !readableFinished && isReadableNodeStream(stream, true)) {
      if (!isReadableFinished(stream, false))
        return callback.call(stream,
                             new ERR_STREAM_PREMATURE_CLOSE());
    }
    if (writable && !writableFinished) {
      if (!isWritableFinished(stream, false))
        return callback.call(stream,
                             new ERR_STREAM_PREMATURE_CLOSE());
    }

    callback.call(stream);
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

  if (closed) {
    process.nextTick(onclose);
  } else if (wState?.errorEmitted || rState?.errorEmitted) {
    if (!willEmitClose) {
      process.nextTick(onclose);
    }
  } else if (
    !readable &&
    (!willEmitClose || isReadable(stream)) &&
    (writableFinished || isWritable(stream) === false)
  ) {
    process.nextTick(onclose);
  } else if (
    !writable &&
    (!willEmitClose || isWritable(stream)) &&
    (readableFinished || isReadable(stream) === false)
  ) {
    process.nextTick(onclose);
  } else if ((rState && stream.req && stream.aborted)) {
    process.nextTick(onclose);
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
      const originalCallback = callback;
      callback = once((...args) => {
        options.signal.removeEventListener('abort', abort);
        originalCallback.apply(stream, args);
      });
      options.signal.addEventListener('abort', abort);
    }
  }

  return cleanup;
}

function finished(stream, opts) {
  return new Promise((resolve, reject) => {
    eos(stream, opts, (err) => {
      if (err) {
        reject(err);
      } else {
        resolve();
      }
    });
  });
}

module.exports = eos;
module.exports.finished = finished;

// Ported from https://github.com/mafintosh/end-of-stream with
// permission from the author, Mathias Buus (@mafintosh).

'use strict';

const {
  ERR_INVALID_ARG_TYPE,
  ERR_STREAM_PREMATURE_CLOSE
} = require('internal/errors').codes;
const { once } = require('internal/util');

function isRequest(stream) {
  return stream.setHeader && typeof stream.abort === 'function';
}

function isReadable(stream) {
  return typeof stream.readable === 'boolean' ||
    typeof stream.readableEnded === 'boolean' ||
    !!stream._readableState;
}

function isWritable(stream) {
  return typeof stream.writable === 'boolean' ||
    typeof stream.writableEnded === 'boolean' ||
    !!stream._writableState;
}

function isWritableFinished(stream) {
  if (stream.writableFinished) return true;
  const wState = stream._writableState;
  if (!wState || wState.errored) return false;
  return wState.finished || (wState.ended && wState.length === 0);
}

function nop() {}

function isReadableEnded(stream) {
  if (stream.readableEnded) return true;
  const rState = stream._readableState;
  if (!rState || rState.errored) return false;
  return rState.endEmitted || (rState.ended && rState.length === 0);
}

function eos(stream, options, callback) {
  if (arguments.length === 2) {
    callback = options;
    options = {};
  } else if (options == null) {
    options = {};
  } else if (typeof options !== 'object') {
    throw new ERR_INVALID_ARG_TYPE('options', 'object', options);
  }
  if (typeof callback !== 'function') {
    throw new ERR_INVALID_ARG_TYPE('callback', 'function', callback);
  }

  callback = once(callback);

  const readable = options.readable ||
    (options.readable !== false && isReadable(stream));
  const writable = options.writable ||
    (options.writable !== false && isWritable(stream));

  const wState = stream._writableState;
  const rState = stream._readableState;
  const state = wState || rState;

  const onlegacyfinish = () => {
    if (!stream.writable) onfinish();
  };

  // TODO (ronag): Improve soft detection to include core modules and
  // common ecosystem modules that do properly emit 'close' but fail
  // this generic check.
  let willEmitClose = (
    state &&
    state.autoDestroy &&
    state.emitClose &&
    state.closed === false &&
    isReadable(stream) === readable &&
    isWritable(stream) === writable
  );

  let writableFinished = stream.writableFinished ||
    (wState && wState.finished);
  const onfinish = () => {
    writableFinished = true;
    // Stream should not be destroyed here. If it is that
    // means that user space is doing something differently and
    // we cannot trust willEmitClose.
    if (stream.destroyed) willEmitClose = false;

    if (willEmitClose && (!stream.readable || readable)) return;
    if (!readable || readableEnded) callback.call(stream);
  };

  let readableEnded = stream.readableEnded ||
    (rState && rState.endEmitted);
  const onend = () => {
    readableEnded = true;
    // Stream should not be destroyed here. If it is that
    // means that user space is doing something differently and
    // we cannot trust willEmitClose.
    if (stream.destroyed) willEmitClose = false;

    if (willEmitClose && (!stream.writable || writable)) return;
    if (!writable || writableFinished) callback.call(stream);
  };

  const onerror = (err) => {
    callback.call(stream, err);
  };

  const onclose = () => {
    if (readable && !readableEnded) {
      if (!isReadableEnded(stream))
        return callback.call(stream, new ERR_STREAM_PREMATURE_CLOSE());
    }
    if (writable && !writableFinished) {
      if (!isWritableFinished(stream))
        return callback.call(stream, new ERR_STREAM_PREMATURE_CLOSE());
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
    if (stream.req) onrequest();
    else stream.on('request', onrequest);
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
  if (options.error !== false) stream.on('error', onerror);
  stream.on('close', onclose);

  const closed = (
    (wState && wState.closed) ||
    (rState && rState.closed) ||
    (wState && wState.errorEmitted) ||
    (rState && rState.errorEmitted) ||
    (rState && stream.req && stream.aborted) ||
    (
      (!writable || (wState && wState.finished)) &&
      (!readable || (rState && rState.endEmitted))
    )
  );

  if (closed) {
    // TODO(ronag): Re-throw error if errorEmitted?
    // TODO(ronag): Throw premature close as if finished was called?
    // before being closed? i.e. if closed but not errored, ended or finished.
    // TODO(ronag): Throw some kind of error? Does it make sense
    // to call finished() on a "finished" stream?
    // TODO(ronag): willEmitClose?
    process.nextTick(() => {
      callback();
    });
  }

  return function() {
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
}

module.exports = eos;

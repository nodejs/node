'use strict';

const { validateFunction } = require('internal/validators');
const { isWebstream } = require('internal/webstreams/util');
const { isTransformStream } = require('internal/webstreams/transformstream');
const {
  isWritableStreamClosed,
  isWritableStreamErrored,
  writableStreamAddFinishedCallback,
  writableStreamRemoveAllFinishedCallbacks,
} = require('internal/webstreams/writablestream');
const {
  isReadableStream,
  isReadableStreamClosed,
  isReadableStreamErrored,
  readableStreamAddFinishedCallback,
  readableStreamRemoveAllFinishedCallbacks,
} = require('internal/webstreams/readablestream');
const { once } = require('internal/util');
const { codes } = require('internal/errors');
const { ERR_INVALID_ARG_TYPE, ERR_MISSING_ARGS } = codes;

function finished(stream, callback) {
  validateAndFixInputs(stream, callback);

  if (isTransformStream(stream)) {
    return finishedForTransformStream(stream, callback);
  }

  addCallbackToStream(stream, callback);
  callCallbackIfStreamIsFinished(stream, callback);

  return getCleanupFuncForStream(stream);
}

function validateAndFixInputs(stream, callback) {
  throwErrorIfNullStream(stream);
  callback = replaceWithNoOpIfNull(callback);
  validateInputs(stream, callback);
  return stream, once(callback);
}

function throwErrorIfNullStream(stream) {
  if (!stream) {
    throw new ERR_MISSING_ARGS('stream');
  }
}

function replaceWithNoOpIfNull(callback) {
  if (!callback) {
    callback = () => {};
  }
  return callback;
}

function validateInputs(stream, callback) {
  validateFunction(callback, 'callback');
  throwErrorIfNotWebstream(stream);
}

function throwErrorIfNotWebstream(stream) {
  if (isWebstream(stream)) {
    return;
  }

  throw new ERR_INVALID_ARG_TYPE(
    'stream',
    'ReadableStream|WritableStream|TransformStream',
    stream
  );
}

function finishedForTransformStream(transformStream, callback) {
  return finished(transformStream.readable, callback);
}

function addCallbackToStream(stream, callback) {
  if (isReadableStream(stream)) {
    readableStreamAddFinishedCallback(stream, callback);
    return;
  }
  writableStreamAddFinishedCallback(stream, callback);
}

function callCallbackIfStreamIsFinished(stream, callback) {
  if (isWebstreamFinished(stream)) {
    callback();
  }
}

function isWebstreamFinished(stream) {
  if (isReadableStream(stream)) {
    return isReadableStreamFinished(stream);
  }
  return isWritableStreamFinished(stream);
}

function isReadableStreamFinished(rs) {
  return isReadableStreamClosed(rs) || isReadableStreamErrored(rs);
}

function isWritableStreamFinished(ws) {
  return isWritableStreamClosed(ws) || isWritableStreamErrored(ws);
}

function getCleanupFuncForStream(stream) {
  if (isReadableStream(stream)) {
    return () => {
      readableStreamRemoveAllFinishedCallbacks(stream);
    };
  }

  return () => {
    writableStreamRemoveAllFinishedCallbacks(stream);
  };
}

module.exports = { finished };

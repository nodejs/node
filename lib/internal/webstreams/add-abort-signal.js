'use strict';

const { validateAbortSignal } = require('internal/validators');
const { isWebstream } = require('internal/webstreams/util');
const { finished } = require('internal/webstreams/finished');
const { AbortError, codes } = require('internal/errors');
const { ERR_INVALID_ARG_TYPE } = codes;

function addAbortSignal(signal, stream) {
  validateInputs(signal, stream);

  const onAbort = () => {
    stream.abort(new AbortError());
  };

  if (signalIsAborted(signal)) {
    onAbort();
  } else {
    addAbortCallbackToSignal(signal, stream, onAbort);
  }

  return stream;
}

function validateInputs(signal, stream) {
  validateAbortSignal(signal, 'signal');
  throwErrorIfNotWebstream(stream);
}

function throwErrorIfNotWebstream(stream) {
  if (isWebstream(stream)) {
    return;
  }

  throw new ERR_INVALID_ARG_TYPE(
    'stream',
    'ReadableStream|WritableStream|TransformStream',
    stream,
  );
}

function signalIsAborted(signal) {
  return signal.aborted;
}

function addAbortCallbackToSignal(signal, stream, callback) {
  signal.addEventListener('abort', callback);
  finished(stream, () => {
    signal.removeEventListener('abort', callback);
  });
}

module.exports = {
  addAbortSignal
};

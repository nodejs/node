'use strict';

const {
  SymbolDispose,
} = primordials;

const {
  AbortError,
  codes: {
    ERR_INVALID_ARG_TYPE,
  },
} = require('internal/errors');

const {
  isNodeStream,
  isWebStream,
  kControllerErrorFunction,
} = require('internal/streams/utils');

const eos = require('internal/streams/end-of-stream');
let addAbortListener;

// This method is inlined here for readable-stream
// It also does not allow for signal to not exist on the stream
// https://github.com/nodejs/node/pull/36061#discussion_r533718029
const validateAbortSignal = (signal, name) => {
  if (typeof signal !== 'object' ||
       !('aborted' in signal)) {
    throw new ERR_INVALID_ARG_TYPE(name, 'AbortSignal', signal);
  }
};

module.exports.addAbortSignal = function addAbortSignal(signal, stream) {
  validateAbortSignal(signal, 'signal');
  if (!isNodeStream(stream) && !isWebStream(stream)) {
    throw new ERR_INVALID_ARG_TYPE('stream', ['ReadableStream', 'WritableStream', 'Stream'], stream);
  }
  return module.exports.addAbortSignalNoValidate(signal, stream);
};

module.exports.addAbortSignalNoValidate = function(signal, stream) {
  if (typeof signal !== 'object' || !('aborted' in signal)) {
    return stream;
  }
  const onAbort = isNodeStream(stream) ?
    () => {
      stream.destroy(new AbortError(undefined, { cause: signal.reason }));
    } :
    () => {
      stream[kControllerErrorFunction](new AbortError(undefined, { cause: signal.reason }));
    };
  if (signal.aborted) {
    onAbort();
  } else {
    addAbortListener ??= require('internal/events/abort_listener').addAbortListener;
    const disposable = addAbortListener(signal, onAbort);
    eos(stream, disposable[SymbolDispose]);
  }
  return stream;
};

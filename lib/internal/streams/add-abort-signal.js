'use strict';

const {
  AbortError,
  codes,
} = require('internal/errors');

const eos = require('internal/streams/end-of-stream');
const { ERR_INVALID_ARG_TYPE } = codes;

// This method is inlined here for readable-stream
// https://github.com/nodejs/node/pull/36061#discussion_r533718029
const validateAbortSignal = (signal, name) => {
  if (signal !== undefined &&
      (signal === null ||
       typeof signal !== 'object' ||
       !('aborted' in signal))) {
    throw new ERR_INVALID_ARG_TYPE(name, 'AbortSignal', signal);
  }
};

function isStream(obj) {
  return !!(obj && typeof obj.pipe === 'function');
}

module.exports = function addAbortSignal(signal, stream) {
  validateAbortSignal(signal, 'signal');
  if (!isStream(stream)) {
    throw new ERR_INVALID_ARG_TYPE('stream', 'stream.Stream', stream);
  }
  const onAbort = () => {
    stream.destroy(new AbortError());
  };
  if (signal.aborted) {
    onAbort();
  } else {
    signal.addEventListener('abort', onAbort);
    eos(stream, () => signal.removeEventListener('abort', onAbort));
  }
  return stream;
};

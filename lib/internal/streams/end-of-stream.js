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

function eos(stream, opts, callback) {
  if (arguments.length === 2) {
    callback = opts;
    opts = {};
  } else if (opts == null) {
    opts = {};
  } else if (typeof opts !== 'object') {
    throw new ERR_INVALID_ARG_TYPE('opts', 'object', opts);
  }
  if (typeof callback !== 'function') {
    throw new ERR_INVALID_ARG_TYPE('callback', 'function', callback);
  }

  callback = once(callback);

  let readable = opts.readable || (opts.readable !== false && stream.readable);
  let writable = opts.writable || (opts.writable !== false && stream.writable);

  const onlegacyfinish = () => {
    if (!stream.writable) onfinish();
  };

  var writableEnded = stream._writableState && stream._writableState.finished;
  const onfinish = () => {
    writable = false;
    writableEnded = true;
    if (!readable) callback.call(stream);
  };

  var readableEnded = stream._readableState && stream._readableState.endEmitted;
  const onend = () => {
    readable = false;
    readableEnded = true;
    if (!writable) callback.call(stream);
  };

  const onerror = (err) => {
    callback.call(stream, err);
  };

  const onclose = () => {
    let err;
    if (readable && !readableEnded) {
      if (!stream._readableState || !stream._readableState.ended)
        err = new ERR_STREAM_PREMATURE_CLOSE();
      return callback.call(stream, err);
    }
    if (writable && !writableEnded) {
      if (!stream._writableState || !stream._writableState.ended)
        err = new ERR_STREAM_PREMATURE_CLOSE();
      return callback.call(stream, err);
    }
  };

  const onrequest = () => {
    stream.req.on('finish', onfinish);
  };

  if (isRequest(stream)) {
    stream.on('complete', onfinish);
    stream.on('abort', onclose);
    if (stream.req) onrequest();
    else stream.on('request', onrequest);
  } else if (writable && !stream._writableState) { // legacy streams
    stream.on('end', onlegacyfinish);
    stream.on('close', onlegacyfinish);
  }

  stream.on('end', onend);
  stream.on('finish', onfinish);
  if (opts.error !== false) stream.on('error', onerror);
  stream.on('close', onclose);

  return function() {
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

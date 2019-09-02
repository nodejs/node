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

  const onerror = (err) => {
    callback.call(stream, err);
  };

  let writableFinished = stream.writableFinished ||
    (stream._writableState && stream._writableState.finished);
  let readableEnded = stream.readableEnded ||
    (stream._readableState && stream._readableState.endEmitted);

  if (writableFinished || readableEnded || stream.destroyed ||
      stream.aborted) {
    if (opts.error !== false) stream.on('error', onerror);
    // A destroy(err) call emits error in nextTick.
    process.nextTick(callback.bind(stream));
    return () => {
      stream.removeListener('error', onerror);
    };
  }

  let readable = opts.readable ||
    (opts.readable !== false && isReadable(stream));
  let writable = opts.writable ||
    (opts.writable !== false && isWritable(stream));

  const onlegacyfinish = () => {
    if (!stream.writable) onfinish();
  };

  const onfinish = () => {
    writable = false;
    writableFinished = true;
    if (!readable) callback.call(stream);
  };

  const onend = () => {
    readable = false;
    readableEnded = true;
    if (!writable) callback.call(stream);
  };

  const onclose = () => {
    if (readable && !readableEnded) {
      callback.call(stream, new ERR_STREAM_PREMATURE_CLOSE());
    } else if (writable && !writableFinished) {
      callback.call(stream, new ERR_STREAM_PREMATURE_CLOSE());
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

  // Not all streams will emit 'close' after 'aborted'.
  if (typeof stream.aborted === 'boolean') {
    stream.on('aborted', onclose);
  }

  stream.on('end', onend);
  stream.on('finish', onfinish);
  if (opts.error !== false) stream.on('error', onerror);
  stream.on('close', onclose);

  return function() {
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

// Ported from https://github.com/mafintosh/end-of-stream with
// permission from the author, Mathias Buus (@mafintosh).

'use strict';

const {
  ERR_STREAM_PREMATURE_CLOSE
} = require('internal/errors').codes;

function noop() {}

function isRequest(stream) {
  return stream.setHeader && typeof stream.abort === 'function';
}

function once(callback) {
  let called = false;
  return function(err) {
    if (called) return;
    called = true;
    callback.call(this, err);
  };
}

function eos(stream, opts, callback) {
  if (typeof opts === 'function') return eos(stream, null, opts);
  if (!opts) opts = {};

  callback = once(callback || noop);

  const ws = stream._writableState;
  const rs = stream._readableState;
  let readable = opts.readable || (opts.readable !== false && stream.readable);
  let writable = opts.writable || (opts.writable !== false && stream.writable);

  const onlegacyfinish = () => {
    if (!stream.writable) onfinish();
  };

  const onfinish = () => {
    writable = false;
    if (!readable) callback.call(stream);
  };

  const onend = () => {
    readable = false;
    if (!writable) callback.call(stream);
  };

  const onerror = (err) => {
    callback.call(stream, err);
  };

  const onclose = () => {
    if (readable && !(rs && rs.ended)) {
      return callback.call(stream, new ERR_STREAM_PREMATURE_CLOSE());
    }
    if (writable && !(ws && ws.ended)) {
      return callback.call(stream, new ERR_STREAM_PREMATURE_CLOSE());
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
  } else if (writable && !ws) { // legacy streams
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

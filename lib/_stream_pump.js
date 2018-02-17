'use strict';

function noop() {}

function isRequest(stream) {
  return stream.setHeader && typeof stream.abort === 'function';
}

function isChildProcess(stream) {
  return stream.stdio &&
    Array.isArray(stream.stdio) && stream.stdio.length === 3;
}

function eos(stream, opts, _callback) {
  if (typeof opts === 'function') return eos(stream, null, opts);
  if (!opts) opts = {};

  let callbackCalled = false;
  const callback = (err) => {
    if (!_callback || callbackCalled) {
      return;
    }
    callbackCalled = true;
    _callback.call(stream, err);
  };

  const ws = stream._writableState;
  const rs = stream._readableState;
  let readable = opts.readable || opts.readable !== false && stream.readable;
  let writable = opts.writable || opts.writable !== false && stream.writable;

  const onlegacyfinish = () => {
    if (!stream.writable) onfinish();
  };

  const onfinish = () => {
    writable = false;
    if (!readable) callback();
  };

  const onend = () => {
    readable = false;
    if (!writable) callback();
  };

  const onexit = (exitCode) => {
    if (exitCode) {
      callback(new Error(`Exited with error code: ${exitCode}`));
    } else {
      callback(null);
    }
  };

  const onclose = () => {
    if (readable && !(rs && rs.ended) || writable && !(ws && ws.ended))
      return callback(new Error('Premature close'));
  };

  const onrequest = () =>
    stream.req.on('finish', onfinish);

  if (isRequest(stream)) {
    stream.on('complete', onfinish);
    stream.on('abort', onclose);
    if (stream.req) onrequest();
    else stream.on('request', onrequest);
  } else if (writable && !ws) { // legacy streams
    stream.on('end', onlegacyfinish);
    stream.on('close', onlegacyfinish);
  }

  if (isChildProcess(stream)) stream.on('exit', onexit);

  stream.on('end', onend);
  stream.on('finish', onfinish);
  if (opts.error !== false) stream.on('error', callback);
  stream.on('close', onclose);

  return () => {
    stream.removeListener('complete', onfinish);
    stream.removeListener('abort', onclose);
    stream.removeListener('request', onrequest);
    if (stream.req) stream.req.removeListener('finish', onfinish);
    stream.removeListener('end', onlegacyfinish);
    stream.removeListener('close', onlegacyfinish);
    stream.removeListener('finish', onfinish);
    stream.removeListener('exit', onexit);
    stream.removeListener('end', onend);
    stream.removeListener('error', callback);
    stream.removeListener('close', onclose);
  };
}

function destroyer(stream, readable, writable, _callback) {
  let callbackCalled = false;
  const callback = (err) => {
    if (callbackCalled) return;
    callbackCalled = true;
    return _callback(err);
  };
  let closed = false;
  stream.on('close', () => {
    closed = true;
  });

  eos(stream, { readable, writable }, (err) => {
    if (err) return callback(err);
    closed = true;
    callback();
  });

  let destroyed = false;
  return (err) => {
    if (closed || destroyed) return;
    destroyed = true;

    if (isRequest(stream))
      return stream.abort();

    if (typeof stream.destroy === 'function') return stream.destroy(err);

    callback(err || new Error('Stream was destroyed'));
  };
}

const call = (fn) => fn();
const callErr = (err) => (fn) => fn(err);
const pipe = (from, to) => from.pipe(to);

function pump(...streams) {
  const callback = streams.pop() || noop;

  if (Array.isArray(streams[0])) streams = streams[0];

  if (streams.length < 2)
    throw new Error('Pump requires two streams per minimum.');

  let firstError;
  const destroys = streams.map((stream, i) => {
    var reading = i < streams.length - 1;
    var writing = i > 0;
    return destroyer(stream, reading, writing, (err) => {
      if (!firstError) firstError = err;

      if (err) destroys.forEach(callErr(err));

      if (reading) return;

      destroys.forEach(call);
      callback(firstError);
    });
  });

  return streams.reduce(pipe);
}

module.exports = pump;

// The MIT License (MIT)
//
// Copyright (c) 2014 Mathias Buus
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

'use strict';

function noop() {}

function isRequest(stream) {
  return stream.setHeader && isFn(stream.abort);
}

function isChildProcess(stream) {
  return stream.stdio &&
    Array.isArray(stream.stdio) && stream.stdio.length === 3;
}

function isFn(fn) {
  return typeof fn === 'function';
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
  let readable = opts.readable ||
    (opts.readable !== false && stream.readable);
  let writable = opts.writable ||
    (opts.writable !== false && stream.writable);

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
      callback(new Error('exited with error code: ' + exitCode));
    } else {
      callback();
    }
  };

  const onclose = () => {
    if (readable && !(rs && rs.ended))
      return callback(new Error('premature close'));

    if (writable && !(ws && ws.ended))
      return callback(new Error('premature close'));
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

function destroyer(stream, reading, writing, _callback) {
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

  eos(stream, {readable: reading, writable: writing}, (err) => {
    if (err) return callback(err);
    closed = true;
    callback();
  });

  var destroyed = false;
  return (err) => {
    if (closed) return;
    if (destroyed) return;
    destroyed = true;

    if (isRequest(stream))
      return stream.abort();
      // request.destroy just do .end - .abort is what we want

    if (isFn(stream.destroy)) return stream.destroy(err);

    callback(err || new Error('stream was destroyed'));
  };
}

const call = (fn) => fn();
const callErr = (err) => (fn) => fn(err);
const pipe = (from, to) => from.pipe(to);

function pump(...streams) {
  const callback = isFn(streams[streams.length - 1] || noop) &&
    streams.pop() || noop;

  if (Array.isArray(streams[0])) streams = streams[0];

  if (streams.length < 2)
    throw new Error('pump requires two streams per minimum');

  let error;
  const destroys = streams.map((stream, i) => {
    var reading = i < streams.length - 1;
    var writing = i > 0;
    return destroyer(stream, reading, writing, (err) => {
      if (!error) error = err;

      if (err) destroys.forEach(callErr(err));

      if (reading) return;

      destroys.forEach(call);
      callback(error);
    });
  });

  return streams.reduce(pipe);
}

module.exports = pump;

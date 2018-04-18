// Ported from https://github.com/mafintosh/pump with
// permission from the author, Mathias Buus (@mafintosh).

'use strict';

const eos = require('internal/streams/end-of-stream');

const {
  ERR_MISSING_ARGS,
  ERR_STREAM_DESTROYED
} = require('internal/errors').codes;

function once(callback) {
  let called = false;
  return function(err) {
    if (called) return;
    called = true;
    callback(err);
  };
}

function noop() {}

function isRequest(stream) {
  return stream.setHeader && typeof stream.abort === 'function';
}

function destroyer(stream, reading, writing, callback) {
  callback = once(callback);

  let closed = false;
  stream.on('close', () => {
    closed = true;
  });

  eos(stream, { readable: reading, writable: writing }, (err) => {
    if (err) return callback(err);
    closed = true;
    callback();
  });

  let destroyed = false;
  return (err) => {
    if (closed) return;
    if (destroyed) return;
    destroyed = true;

    // request.destroy just do .end - .abort is what we want
    if (isRequest(stream)) return stream.abort();
    if (typeof stream.destroy === 'function') return stream.destroy();

    callback(err || new ERR_STREAM_DESTROYED('pipe'));
  };
}

function call(fn) {
  fn();
}

function pipe(from, to) {
  return from.pipe(to);
}

function popCallback(streams) {
  if (!streams.length) return noop;
  if (typeof streams[streams.length - 1] !== 'function') return noop;
  return streams.pop();
}

function pipeline(...streams) {
  const callback = popCallback(streams);

  if (Array.isArray(streams[0])) streams = streams[0];

  if (streams.length < 2) {
    throw new ERR_MISSING_ARGS('streams');
  }

  let error;
  const destroys = streams.map(function(stream, i) {
    const reading = i < streams.length - 1;
    const writing = i > 0;
    return destroyer(stream, reading, writing, function(err) {
      if (!error) error = err;
      if (err) destroys.forEach(call);
      if (reading) return;
      destroys.forEach(call);
      callback(error);
    });
  });

  return streams.reduce(pipe);
}

module.exports = pipeline;

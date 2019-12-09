// Ported from https://github.com/mafintosh/pump with
// permission from the author, Mathias Buus (@mafintosh).

'use strict';

const {
  ArrayIsArray,
} = primordials;

let eos;

const { once } = require('internal/util');
const {
  ERR_INVALID_CALLBACK,
  ERR_MISSING_ARGS,
  ERR_STREAM_DESTROYED
} = require('internal/errors').codes;

function isRequest(stream) {
  return stream.setHeader && typeof stream.abort === 'function';
}

function destroyer(stream, reading, writing, callback) {
  callback = once(callback);

  let closed = false;
  stream.on('close', () => {
    closed = true;
  });

  if (eos === undefined) eos = require('internal/streams/end-of-stream');
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
    if (typeof stream.destroy === 'function') {
      if (stream.req && stream._writableState === undefined) {
        // This is a ClientRequest
        // TODO(mcollina): backward compatible fix to avoid crashing.
        // Possibly remove in a later semver-major change.
        stream.req.on('error', noop);
      }
      return stream.destroy(err);
    }

    callback(err || new ERR_STREAM_DESTROYED('pipe'));
  };
}

function noop() {}

function pipe(from, to) {
  return from.pipe(to);
}

function popCallback(streams) {
  // Streams should never be an empty array. It should always contain at least
  // a single stream. Therefore optimize for the average case instead of
  // checking for length === 0 as well.
  if (typeof streams[streams.length - 1] !== 'function')
    throw new ERR_INVALID_CALLBACK(streams[streams.length - 1]);
  return streams.pop();
}

function pipeline(...streams) {
  const callback = popCallback(streams);

  if (ArrayIsArray(streams[0])) streams = streams[0];

  if (streams.length < 2) {
    throw new ERR_MISSING_ARGS('streams');
  }

  let error;
  const destroys = streams.map(function(stream, i) {
    const reading = i < streams.length - 1;
    const writing = i > 0;
    return destroyer(stream, reading, writing, function(err) {
      if (!error) error = err;
      if (err) {
        for (const destroy of destroys) {
          destroy(err);
        }
      }
      if (reading) return;
      for (const destroy of destroys) {
        destroy();
      }
      callback(error);
    });
  });

  return streams.reduce(pipe);
}

module.exports = pipeline;

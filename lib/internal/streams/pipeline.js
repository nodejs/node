// Ported from https://github.com/mafintosh/pump with
// permission from the author, Mathias Buus (@mafintosh).

'use strict';

const {
  ArrayIsArray,
  SymbolAsyncIterator,
  SymbolIterator
} = primordials;

let eos;

const { once } = require('internal/util');
const destroyImpl = require('internal/streams/destroy');
const {
  ERR_INVALID_ARG_TYPE,
  ERR_INVALID_RETURN_VALUE,
  ERR_INVALID_CALLBACK,
  ERR_MISSING_ARGS,
  ERR_STREAM_DESTROYED
} = require('internal/errors').codes;

let EE;
let PassThrough;
let createReadableStreamAsyncIterator;

function isIncoming(stream) {
  return (
    stream.socket &&
    typeof stream.complete === 'boolean' &&
    ArrayIsArray(stream.rawTrailers) &&
    ArrayIsArray(stream.rawHeaders)
  );
}

function isOutgoing(stream) {
  return (
    stream.socket &&
    typeof stream.setHeader === 'function'
  );
}

function destroyer(stream, reading, writing, final, callback) {
  const _destroy = once((err) => {
    if (!err && (isIncoming(stream) || isOutgoing(stream))) {
      // http/1 request objects have a coupling to their response and should
      // not be prematurely destroyed. Assume they will handle their own
      // lifecycle.
      return callback();
    }

    if (!err && reading && !writing && stream.writable) {
      return callback();
    }

    if (err || !final || !stream.readable) {
      destroyImpl.destroyer(stream, err);
    }
    callback(err);
  });

  if (eos === undefined) eos = require('internal/streams/end-of-stream');
  eos(stream, { readable: reading, writable: writing }, (err) => {
    const rState = stream._readableState;
    if (
      err &&
      err.code === 'ERR_STREAM_PREMATURE_CLOSE' &&
      reading &&
      (rState && rState.ended && !rState.errored && !rState.errorEmitted)
    ) {
      // Some readable streams will emit 'close' before 'end'. However, since
      // this is on the readable side 'end' should still be emitted if the
      // stream has been ended and no error emitted. This should be allowed in
      // favor of backwards compatibility. Since the stream is piped to a
      // destination this should not result in any observable difference.
      // We don't need to check if this is a writable premature close since
      // eos will only fail with premature close on the reading side for
      // duplex streams.
      stream
        .once('end', _destroy)
        .once('error', _destroy);
    } else {
      _destroy(err);
    }
  });

  return (err) => _destroy(err || new ERR_STREAM_DESTROYED('pipe'));
}

function popCallback(streams) {
  // Streams should never be an empty array. It should always contain at least
  // a single stream. Therefore optimize for the average case instead of
  // checking for length === 0 as well.
  if (typeof streams[streams.length - 1] !== 'function')
    throw new ERR_INVALID_CALLBACK(streams[streams.length - 1]);
  return streams.pop();
}

function isPromise(obj) {
  return !!(obj && typeof obj.then === 'function');
}

function isReadable(obj) {
  return !!(obj && typeof obj.pipe === 'function');
}

function isWritable(obj) {
  return !!(obj && typeof obj.write === 'function');
}

function isStream(obj) {
  return isReadable(obj) || isWritable(obj);
}

function isIterable(obj, isAsync) {
  if (!obj) return false;
  if (isAsync === true) return typeof obj[SymbolAsyncIterator] === 'function';
  if (isAsync === false) return typeof obj[SymbolIterator] === 'function';
  return typeof obj[SymbolAsyncIterator] === 'function' ||
    typeof obj[SymbolIterator] === 'function';
}

function makeAsyncIterable(val) {
  if (isIterable(val)) {
    return val;
  } else if (isReadable(val)) {
    // Legacy streams are not Iterable.
    return fromReadable(val);
  } else {
    throw new ERR_INVALID_ARG_TYPE(
      'val', ['Readable', 'Iterable', 'AsyncIterable'], val);
  }
}

async function* fromReadable(val) {
  if (!createReadableStreamAsyncIterator) {
    createReadableStreamAsyncIterator =
      require('internal/streams/async_iterator');
  }
  yield* createReadableStreamAsyncIterator(val);
}

async function pump(iterable, writable, finish) {
  if (!EE) {
    EE = require('events');
  }
  let error;
  try {
    for await (const chunk of iterable) {
      if (!writable.write(chunk)) {
        if (writable.destroyed) return;
        await EE.once(writable, 'drain');
      }
    }
    writable.end();
  } catch (err) {
    error = err;
  } finally {
    finish(error);
  }
}

function pipeline(...streams) {
  const callback = once(popCallback(streams));

  if (ArrayIsArray(streams[0])) streams = streams[0];

  if (streams.length < 2) {
    throw new ERR_MISSING_ARGS('streams');
  }

  let error;
  let value;
  const destroys = [];

  let finishCount = 0;

  function finish(err) {
    const final = --finishCount === 0;

    if (err && (!error || error.code === 'ERR_STREAM_PREMATURE_CLOSE')) {
      error = err;
    }

    if (!error && !final) {
      return;
    }

    while (destroys.length) {
      destroys.shift()(error);
    }

    if (final) {
      callback(error, value);
    }
  }

  let ret;
  for (let i = 0; i < streams.length; i++) {
    const stream = streams[i];
    const reading = i < streams.length - 1;
    const writing = i > 0;

    if (isStream(stream)) {
      finishCount++;
      destroys.push(destroyer(stream, reading, writing, !reading, finish));
    }

    if (i === 0) {
      if (typeof stream === 'function') {
        ret = stream();
        if (!isIterable(ret)) {
          throw new ERR_INVALID_RETURN_VALUE(
            'Iterable, AsyncIterable or Stream', 'source', ret);
        }
      } else if (isIterable(stream) || isReadable(stream)) {
        ret = stream;
      } else {
        throw new ERR_INVALID_ARG_TYPE(
          'source', ['Stream', 'Iterable', 'AsyncIterable', 'Function'],
          stream);
      }
    } else if (typeof stream === 'function') {
      ret = makeAsyncIterable(ret);
      ret = stream(ret);

      if (reading) {
        if (!isIterable(ret, true)) {
          throw new ERR_INVALID_RETURN_VALUE(
            'AsyncIterable', `transform[${i - 1}]`, ret);
        }
      } else {
        if (!PassThrough) {
          PassThrough = require('_stream_passthrough');
        }

        // If the last argument to pipeline is not a stream
        // we must create a proxy stream so that pipeline(...)
        // always returns a stream which can be further
        // composed through `.pipe(stream)`.

        const pt = new PassThrough({
          objectMode: true
        });
        if (isPromise(ret)) {
          ret
            .then((val) => {
              value = val;
              pt.end(val);
            }, (err) => {
              pt.destroy(err);
            });
        } else if (isIterable(ret, true)) {
          finishCount++;
          pump(ret, pt, finish);
        } else {
          throw new ERR_INVALID_RETURN_VALUE(
            'AsyncIterable or Promise', 'destination', ret);
        }

        ret = pt;

        finishCount++;
        destroys.push(destroyer(ret, false, true, true, finish));
      }
    } else if (isStream(stream)) {
      if (isReadable(ret)) {
        ret.pipe(stream);

        // Compat. Before node v10.12.0 stdio used to throw an error so
        // pipe() did/does not end() stdio destinations.
        // Now they allow it but "secretly" don't close the underlying fd.
        if (stream === process.stdout || stream === process.stderr) {
          ret.on('end', () => stream.end());
        }
      } else {
        ret = makeAsyncIterable(ret);

        finishCount++;
        pump(ret, stream, finish);
      }
      ret = stream;
    } else {
      const name = reading ? `transform[${i - 1}]` : 'destination';
      throw new ERR_INVALID_ARG_TYPE(
        name, ['Stream', 'Function'], ret);
    }
  }

  // TODO(ronag): Consider returning a Duplex proxy if the first argument
  // is a writable. Would improve composability.
  // See, https://github.com/nodejs/node/issues/32020
  return ret;
}

module.exports = pipeline;

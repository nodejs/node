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
const {
  ERR_INVALID_ARG_TYPE,
  ERR_INVALID_RETURN_VALUE,
  ERR_INVALID_CALLBACK,
  ERR_MISSING_ARGS,
  ERR_STREAM_DESTROYED
} = require('internal/errors').codes;

let EE;
let Readable;
let Passthrough;

function nop() {}

function isRequest(stream) {
  return stream && stream.setHeader && typeof stream.abort === 'function';
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
    if (isRequest(stream.req)) return stream.req.abort();
    if (typeof stream.destroy === 'function') return stream.destroy(err);

    callback(err || new ERR_STREAM_DESTROYED('pipe'));
  };
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
  if (isAsync === true) return !!obj[SymbolAsyncIterator];
  if (isAsync === false) return !!obj[SymbolIterator];
  return !!(obj[SymbolAsyncIterator] || obj[SymbolIterator]);
}

function makeAsyncIterable(val, name) {
  if (isIterable(val)) {
    return val;
  } else if (isReadable(val)) {
    return _fromReadable(val);
  } else if (isPromise(val)) {
    // Generator will subscribe to the promise in a lazy fashion
    // while the promise executes eagerly. Thus we need to register
    // a rejection handler to avoid unhandled rejection errors.
    val.catch(nop);
    return _fromPromise(val);
  } else {
    throw new ERR_INVALID_RETURN_VALUE(
      'AsyncIterable, Readable or Promise', name, val);
  }
}

async function* _fromPromise(val) {
  yield await val;
}

async function* _fromReadable(val) {
  if (isReadable(val)) {
    // Legacy streams are not Iterable.

    if (!Readable) {
      Readable = require('_stream_readable');
    }

    try {
      const it = Readable.prototype[SymbolAsyncIterator].call(val);
      while (true) {
        const { value, done } = await it.next();
        if (done) return;
        yield value;
      }
    } finally {
      val.destroy();
    }
  }
}

async function pump(iterable, writable, finish) {
  if (!EE) {
    EE = require('events');
  }
  try {
    for await (const chunk of iterable) {
      if (!writable.write(chunk)) {
        if (writable.destroyed) return;
        await EE.once(writable, 'drain');
      }
    }
    writable.end();
  } catch (err) {
    finish(err);
  }
}

function pipeline(...streams) {
  const callback = once(popCallback(streams));

  if (ArrayIsArray(streams[0])) streams = streams[0];

  if (streams.length < 2) {
    throw new ERR_MISSING_ARGS('streams');
  }

  let error;
  const destroys = [];

  function finish(err, val, final) {
    if (!error && err) {
      error = err;
    }

    if (error || final) {
      for (const destroy of destroys) {
        destroy(error);
      }
    }

    if (final) {
      callback(error, val);
    }
  }

  function wrap(stream, reading, writing, final) {
    destroys.push(destroyer(stream, reading, writing, (err) => {
      finish(err, null, final);
    }));
  }

  let ret;
  for (let i = 0; i < streams.length; i++) {
    const stream = streams[i];
    const reading = i < streams.length - 1;
    const writing = i > 0;

    if (isStream(stream)) {
      wrap(stream, reading, writing, !reading);
    }

    if (i === 0) {
      if (typeof stream === 'function') {
        ret = stream();
      } else if (isIterable(stream) || isReadable(stream)) {
        ret = stream;
      } else {
        throw new ERR_INVALID_ARG_TYPE(
          `streams[${i}]`, ['Stream', 'Iterable', 'AsyncIterable', 'Function'],
          stream);
      }
    } else if (typeof stream === 'function') {
      ret = stream(makeAsyncIterable(ret, `streams[${i - 1}]`));
    } else if (isStream(stream)) {
      if (isReadable(ret)) {
        ret.pipe(stream);
      } else {
        pump(makeAsyncIterable(ret, `streams[${i - 1}]`), stream, finish);
      }
      ret = stream;
    } else {
      throw new ERR_INVALID_ARG_TYPE(
        `streams[${i}]`, ['Stream', 'Function'], ret);
    }
  }

  if (!isStream(ret)) {
    if (!Passthrough) {
      Passthrough = require('_stream_passthrough');
    }

    const pt = new Passthrough();

    if (isPromise(ret)) {
      // Finish eagerly
      ret
        .then((val) => {
          pt.end(val);
          finish(null, val, true);
        })
        .catch((err) => {
          finish(err, null, true);
        });
    } else {
      pump(makeAsyncIterable(ret, `streams[${streams.length - 1}]`), pt, finish);
    }

    ret = pt;
    wrap(ret, true, false, true);
  }

  return ret;
}

module.exports = pipeline;

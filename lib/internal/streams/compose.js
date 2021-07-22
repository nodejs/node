'use strict';

const pipeline = require('internal/streams/pipeline');
const Duplex = require('internal/streams/duplex');
const { createDeferredPromise } = require('internal/util');
const { destroyer } = require('internal/streams/destroy');
const from = require('internal/streams/from');
const {
  isNodeStream,
  isIterable,
  isReadable,
  isWritable,
} = require('internal/streams/utils');
const {
  PromiseResolve,
} = primordials;
const {
  AbortError,
  codes: {
    ERR_INVALID_ARG_TYPE,
    ERR_INVALID_ARG_VALUE,
    ERR_INVALID_RETURN_VALUE,
    ERR_MISSING_ARGS,
  },
} = require('internal/errors');
const assert = require('internal/assert');

// This is needed for pre node 17.
class ComposeDuplex extends Duplex {
  constructor(options) {
    super(options);

    // https://github.com/nodejs/node/pull/34385

    if (options?.readable === false) {
      this._readableState.readable = false;
      this._readableState.ended = true;
      this._readableState.endEmitted = true;
    }

    if (options?.writable === false) {
      this._writableState.writable = false;
      this._writableState.ending = true;
      this._writableState.ended = true;
      this._writableState.finished = true;
    }
  }
}

module.exports = function compose(...streams) {
  if (streams.length === 0) {
    throw new ERR_MISSING_ARGS('streams');
  }

  if (streams.length === 1) {
    return makeDuplex(streams[0], 'streams[0]');
  }

  const orgStreams = [...streams];

  if (typeof streams[0] === 'function') {
    streams[0] = makeDuplex(streams[0], 'streams[0]');
  }

  if (typeof streams[streams.length - 1] === 'function') {
    const idx = streams.length - 1;
    streams[idx] = makeDuplex(streams[idx], `streams[${idx}]`);
  }

  for (let n = 0; n < streams.length; ++n) {
    if (!isNodeStream(streams[n])) {
      // TODO(ronag): Add checks for non streams.
      continue;
    }
    if (n < streams.length - 1 && !isReadable(streams[n])) {
      throw new ERR_INVALID_ARG_VALUE(
        `streams[${n}]`,
        orgStreams[n],
        'must be readable'
      );
    }
    if (n > 0 && !isWritable(streams[n])) {
      throw new ERR_INVALID_ARG_VALUE(
        `streams[${n}]`,
        orgStreams[n],
        'must be writable'
      );
    }
  }

  let ondrain;
  let onfinish;
  let onreadable;
  let onclose;
  let d;

  function onfinished(err) {
    const cb = onclose;
    onclose = null;

    if (cb) {
      cb(err);
    } else if (err) {
      d.destroy(err);
    } else if (!readable && !writable) {
      d.destroy();
    }
  }

  const head = streams[0];
  const tail = pipeline(streams, onfinished);

  const writable = !!isWritable(head);
  const readable = !!isReadable(tail);

  // TODO(ronag): Avoid double buffering.
  // Implement Writable/Readable/Duplex traits.
  // See, https://github.com/nodejs/node/pull/33515.
  d = new ComposeDuplex({
    highWaterMark: 1,
    writableObjectMode: !!head?.writableObjectMode,
    readableObjectMode: !!tail?.writableObjectMode,
    writable,
    readable,
  });

  if (writable) {
    d._write = function(chunk, encoding, callback) {
      if (head.write(chunk, encoding)) {
        callback();
      } else {
        ondrain = callback;
      }
    };

    d._final = function(callback) {
      head.end();
      onfinish = callback;
    };

    head.on('drain', function() {
      if (ondrain) {
        const cb = ondrain;
        ondrain = null;
        cb();
      }
    });

    tail.on('finish', function() {
      if (onfinish) {
        const cb = onfinish;
        onfinish = null;
        cb();
      }
    });
  }

  if (readable) {
    tail.on('readable', function() {
      if (onreadable) {
        const cb = onreadable;
        onreadable = null;
        cb();
      }
    });

    tail.on('end', function() {
      d.push(null);
    });

    d._read = function() {
      while (true) {
        const buf = tail.read();

        if (buf === null) {
          onreadable = d._read;
          return;
        }

        if (!d.push(buf)) {
          return;
        }
      }
    };
  }

  d._destroy = function(err, callback) {
    if (!err && onclose !== null) {
      err = new AbortError();
    }

    onreadable = null;
    ondrain = null;
    onfinish = null;

    if (onclose === null) {
      callback(err);
    } else {
      onclose = callback;
      destroyer(tail, err);
    }
  };

  return d;
};

function makeDuplex(stream, name) {
  let ret;
  if (typeof stream === 'function') {
    assert(stream.length > 0);

    const { value, write, final } = fromAsyncGen(stream);

    if (isIterable(value)) {
      ret = from(ComposeDuplex, value, {
        objectMode: true,
        highWaterMark: 1,
        write,
        final
      });
    } else if (typeof value?.then === 'function') {
      const promise = PromiseResolve(value)
        .then((val) => {
          if (val != null) {
            throw new ERR_INVALID_RETURN_VALUE('nully', name, val);
          }
        })
        .catch((err) => {
          destroyer(ret, err);
        });

      ret = new ComposeDuplex({
        objectMode: true,
        highWaterMark: 1,
        readable: false,
        write,
        final(cb) {
          final(() => promise.then(cb, cb));
        }
      });
    } else {
      throw new ERR_INVALID_RETURN_VALUE(
        'Iterable, AsyncIterable or AsyncFunction', name, value);
    }
  } else if (isNodeStream(stream)) {
    ret = stream;
  } else if (isIterable(stream)) {
    ret = from(ComposeDuplex, stream, {
      objectMode: true,
      highWaterMark: 1,
      writable: false
    });
  } else {
    throw new ERR_INVALID_ARG_TYPE(
      name,
      ['Stream', 'Iterable', 'AsyncIterable', 'Function'],
      stream)
    ;
  }
  return ret;
}

function fromAsyncGen(fn) {
  let { promise, resolve } = createDeferredPromise();
  const value = fn(async function*() {
    while (true) {
      const { chunk, done, cb } = await promise;
      process.nextTick(cb);
      if (done) return;
      yield chunk;
      ({ promise, resolve } = createDeferredPromise());
    }
  }());

  return {
    value,
    write(chunk, encoding, cb) {
      resolve({ chunk, done: false, cb });
    },
    final(cb) {
      resolve({ done: true, cb });
    }
  };
}

'use strict';

const {
  ObjectDefineProperties,
  ObjectCreate,
  Symbol
} = primordials;

const EE = require('events');
const Stream = require('stream');
const { Buffer } = require('buffer');
const destroyImpl = require('internal/streams/destroy');

const {
  ERR_INVALID_ARG_TYPE,
  ERR_STREAM_CANNOT_PIPE,
  ERR_STREAM_DESTROYED,
  ERR_STREAM_ALREADY_FINISHED,
  ERR_STREAM_NULL_VALUES,
  ERR_STREAM_WRITE_AFTER_END,
  ERR_UNKNOWN_ENCODING
} = require('internal/errors').codes;

const { errorOrDestroy } = destroyImpl;

class WritableStateBase {
  constructor(options) {
    // Object stream flag to indicate whether or not this stream
    // contains buffers or objects.
    this.objectMode = !!(options && options.objectMode);

    // At the start of calling end()
    this.ending = false;

    // When end() has been called, and returned.
    this.ended = false;

    // When 'finish' is emitted.
    this.finished = false;

    // Has it been destroyed
    this.destroyed = false;

    // Stream is still being constructed and cannot be
    // destroyed until construction finished or failed.
    // Async construction is opt in, therefore we start as
    // constructed.
    this.constructed = true;

    // Should we decode strings into buffers before passing to _write?
    // this is here so that some node-core streams can optimize string
    // handling at a lower level.
    const noDecode = !!(options && options.decodeStrings === false);
    this.decodeStrings = !noDecode;

    // Crypto is kind of old and crusty.  Historically, its default string
    // encoding is 'binary' so we have to make this configurable.
    // Everything else in the universe uses 'utf8', though.
    this.defaultEncoding = (options && options.defaultEncoding) || 'utf8';

    // True if the error was already emitted and should not be thrown again.
    this.errorEmitted = false;

    // Should close be emitted on destroy. Defaults to true.
    this.emitClose = !options || options.emitClose !== false;

    // Should .destroy() be called after 'finish' (and potentially 'end').
    this.autoDestroy = !options || options.autoDestroy !== false;

    // Indicates whether the stream has errored. When true all write() calls
    // should return false. This is needed since when autoDestroy
    // is disabled we need a way to tell whether the stream has failed.
    this.errored = false;

    // Indicates whether the stream has finished destroying.
    this.closed = false;
  }
}

const kWrite = Symbol('kWrite');
const kFlush = Symbol('kFlush');

function WritableBase(options, isDuplex, State) {
  if (typeof options.write !== 'function') {
    throw new ERR_INVALID_ARG_TYPE('option.write', 'function', options.write);
  }
  if (typeof options.flush !== 'function') {
    throw new ERR_INVALID_ARG_TYPE('option.flush', 'function', options.flush);
  }

  Stream.call(this, options);

  this[kWrite] = options.write;
  this[kFlush] = options.flush;

  this._writableState = new State(options, this, isDuplex);

  destroyImpl.construct(this, options.start);
}
WritableBase.prototype = ObjectCreate(Stream.prototype);
WritableBase.errorOrDestroy = errorOrDestroy;
WritableBase.WritableState = WritableStateBase;
WritableBase.prototype.write = function(chunk, encoding, cb) {
  const state = this._writableState;

  if (typeof encoding === 'function') {
    cb = encoding;
    encoding = state.defaultEncoding;
  } else if (!encoding) {
    encoding = state.defaultEncoding;
  }

  if (chunk === null) {
    throw new ERR_STREAM_NULL_VALUES();
  } else if (!state.objectMode) {
    if (typeof chunk === 'string') {
      if (state.decodeStrings !== false) {
        chunk = Buffer.from(chunk, encoding);
        encoding = 'buffer';
      }
    } else if (chunk instanceof Buffer) {
      encoding = 'buffer';
    } else if (Stream._isUint8Array(chunk)) {
      chunk = Stream._uint8ArrayToBuffer(chunk);
      encoding = 'buffer';
    } else {
      throw new ERR_INVALID_ARG_TYPE(
        'chunk', ['string', 'Buffer', 'Uint8Array'], chunk);
    }
  }

  let err;
  if (state.ending) {
    err = new ERR_STREAM_WRITE_AFTER_END();
  } else if (state.destroyed) {
    err = new ERR_STREAM_DESTROYED('write');
  }

  if (err) {
    if (typeof cb === 'function') {
      process.nextTick(cb, err);
    }
    errorOrDestroy(this, err, true);
    return false;
  }

  // TODO(ronag): Move more logic from Writable into custom cb.
  const ret = this[kWrite](state, chunk, encoding, cb);
  // Return false if errored or destroyed in order to break
  // any synchronous while(stream.write(data)) loops.
  return ret && !state.errored && !state.destroyed;
};

// Otherwise people can pipe Writable streams, which is just wrong.
WritableBase.prototype.pipe = function() {
  errorOrDestroy(this, new ERR_STREAM_CANNOT_PIPE());
};

WritableBase.prototype.setDefaultEncoding = function(encoding) {
  // node::ParseEncoding() requires lower case.
  if (typeof encoding === 'string')
    encoding = encoding.toLowerCase();
  if (!Buffer.isEncoding(encoding))
    throw new ERR_UNKNOWN_ENCODING(encoding);
  this._writableState.defaultEncoding = encoding;
  return this;
};

WritableBase.prototype.end = function(chunk, encoding, cb) {
  const state = this._writableState;

  if (typeof chunk === 'function') {
    cb = chunk;
    chunk = null;
    encoding = null;
  } else if (typeof encoding === 'function') {
    cb = encoding;
    encoding = null;
  }

  if (chunk !== null && chunk !== undefined)
    this.write(chunk, encoding);

  // This is forgiving in terms of unnecessary calls to end() and can hide
  // logic errors. However, usually such errors are harmless and causing a
  // hard error can be disproportionately destructive. It is not always
  // trivial for the user to determine whether end() needs to be called or not.
  let err;
  if (!state.errored && !state.ending) {
    state.ending = true;
    let called = false;
    this[kFlush](state, (err) => {
      if (called) {
        // TODO(ronag): ERR_MULTIPLE_CALLBACK?
        return;
      }
      called = true;

      const sync = !state.ended;
      if (err) {
        errorOrDestroy(this, err, sync);
      } else if (sync) {
        process.nextTick(finish, this, state);
      } else {
        finish(this, state);
      }
    });
    state.ended = true;
  } else if (state.finished) {
    err = new ERR_STREAM_ALREADY_FINISHED('end');
  } else if (state.destroyed) {
    err = new ERR_STREAM_DESTROYED('end');
  }

  if (typeof cb === 'function') {
    if (err || state.finished)
      process.nextTick(cb, err);
    else
      onFinished(this, cb);
  }

  return this;
};

function finish(stream, state) {
  // TODO(ronag): state.closed, state.errored, state.destroyed?

  if (state.errorEmitted)
    return;

  // TODO(ronag): This could occur after 'close' is emitted.

  state.finished = true;
  stream.emit('finish');

  if (state.autoDestroy) {
    // In case of duplex streams we need a way to detect
    // if the readable side is ready for autoDestroy as well.
    const rState = stream._readableState;
    const autoDestroy = !rState || (
      rState.autoDestroy &&
      // We don't expect the readable to ever 'end'
      // if readable is explicitly set to false.
      (rState.endEmitted || rState.readable === false)
    );
    if (autoDestroy) {
      stream.destroy();
    }
  }
}

WritableBase.prototype[EE.captureRejectionSymbol] = function(err) {
  this.destroy(err);
};
WritableBase.prototype.destroy = destroyImpl.destroy;
WritableBase.prototype._undestroy = destroyImpl.undestroy;
WritableBase.prototype._destroy = function(err, cb) {
  cb(err);
};

// TODO(ronag): Avoid using events to implement internal logic.
function onFinished(stream, cb) {
  function onerror(err) {
    stream.removeListener('finish', onfinish);
    stream.removeListener('error', onerror);
    cb(err);
    if (stream.listenerCount('error') === 0) {
      stream.emit('error', err);
    }
  }

  function onfinish() {
    stream.removeListener('finish', onfinish);
    stream.removeListener('error', onerror);
    cb();
  }
  stream.on('finish', onfinish);
  stream.prependListener('error', onerror);
}

ObjectDefineProperties(WritableBase.prototype, {
  destroyed: {
    get() {
      return this._writableState ? this._writableState.destroyed : false;
    },
    set(value) {
      // Backward compatibility, the user is explicitly managing destroyed.
      if (this._writableState) {
        this._writableState.destroyed = value;
      }
    }
  },

  writable: {
    get() {
      const w = this._writableState;
      // w.writable === false means that this is part of a Duplex stream
      // where the writable side was disabled upon construction.
      // Compat. The user might manually disable writable side through
      // deprecated setter.
      return !!w && w.writable !== false && !w.destroyed && !w.errored &&
        !w.ending && !w.ended;
    },
    set(val) {
      // Backwards compatible.
      if (this._writableState) {
        this._writableState.writable = !!val;
      }
    }
  },

  writableFinished: {
    get() {
      return this._writableState ? this._writableState.finished : false;
    }
  },

  writableObjectMode: {
    get() {
      return this._writableState ? this._writableState.objectMode : false;
    }
  },

  writableEnded: {
    get() {
      return this._writableState ? this._writableState.ending : false;
    }
  }
});

module.exports = {
  WritableBase
};

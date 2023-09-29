// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

// A bit simpler than readable streams.
// Implement an async ._write(chunk, encoding, cb), and it'll handle all
// the drain event emission and buffering.

'use strict';

const {
  ArrayPrototypeSlice,
  Error,
  FunctionPrototypeSymbolHasInstance,
  ObjectDefineProperty,
  ObjectDefineProperties,
  ObjectSetPrototypeOf,
  StringPrototypeToLowerCase,
  Symbol,
  SymbolHasInstance,
} = primordials;

module.exports = Writable;
Writable.WritableState = WritableState;

const EE = require('events');
const Stream = require('internal/streams/legacy').Stream;
const { Buffer } = require('buffer');
const destroyImpl = require('internal/streams/destroy');

const {
  addAbortSignal,
} = require('internal/streams/add-abort-signal');

const {
  getHighWaterMark,
  getDefaultHighWaterMark,
} = require('internal/streams/state');
const {
  ERR_INVALID_ARG_TYPE,
  ERR_METHOD_NOT_IMPLEMENTED,
  ERR_MULTIPLE_CALLBACK,
  ERR_STREAM_CANNOT_PIPE,
  ERR_STREAM_DESTROYED,
  ERR_STREAM_ALREADY_FINISHED,
  ERR_STREAM_NULL_VALUES,
  ERR_STREAM_WRITE_AFTER_END,
  ERR_UNKNOWN_ENCODING,
} = require('internal/errors').codes;

const { errorOrDestroy } = destroyImpl;

ObjectSetPrototypeOf(Writable.prototype, Stream.prototype);
ObjectSetPrototypeOf(Writable, Stream);

function nop() {}

const kOnFinishedValue = Symbol('kOnFinishedValue');
const kErroredValue = Symbol('kErroredValue');
const kDefaultEncodingValue = Symbol('kDefaultEncodingValue');
const kWriteCbValue = Symbol('kWriteCbValue');
const kAfterWriteTickInfoValue = Symbol('kAfterWriteTickInfoValue');

const kObjectMode = 1 << 0;
const kEnded = 1 << 1;
const kConstructed = 1 << 2;
const kSync = 1 << 3;
const kErrorEmitted = 1 << 4;
const kEmitClose = 1 << 5;
const kAutoDestroy = 1 << 6;
const kDestroyed = 1 << 7;
const kClosed = 1 << 8;
const kCloseEmitted = 1 << 9;
const kFinalCalled = 1 << 10;
const kNeedDrain = 1 << 11;
const kEnding = 1 << 12;
const kFinished = 1 << 13;
const kDecodeStrings = 1 << 14;
const kWriting = 1 << 15;
const kBufferProcessing = 1 << 16;
const kPrefinished = 1 << 17;
const kAllBuffers = 1 << 18;
const kAllNoop = 1 << 19;
const kOnFinished = 1 << 20;
const kErrored = 1 << 21;
const kHasWritable = 1 << 22;
const kWritable = 1 << 23;
const kCorked = 1 << 24;
const kDefaultUTF8Encoding = 1 << 25;
const kWriteCb = 1 << 26;
const kExpectWriteCb = 1 << 27;
const kAfterWriteTickInfo = 1 << 28;
const kAfterWritePending = 1 << 29;

// TODO(benjamingr) it is likely slower to do it this way than with free functions
function makeBitMapDescriptor(bit) {
  return {
    enumerable: false,
    get() { return (this.state & bit) !== 0; },
    set(value) {
      if (value) this.state |= bit;
      else this.state &= ~bit;
    },
  };
}
ObjectDefineProperties(WritableState.prototype, {
  // Object stream flag to indicate whether or not this stream
  // contains buffers or objects.
  objectMode: makeBitMapDescriptor(kObjectMode),

  // if _final has been called.
  finalCalled: makeBitMapDescriptor(kFinalCalled),

  // drain event flag.
  needDrain: makeBitMapDescriptor(kNeedDrain),

  // At the start of calling end()
  ending: makeBitMapDescriptor(kEnding),

  // When end() has been called, and returned.
  ended: makeBitMapDescriptor(kEnded),

  // When 'finish' is emitted.
  finished: makeBitMapDescriptor(kFinished),

  // Has it been destroyed.
  destroyed: makeBitMapDescriptor(kDestroyed),

  // Should we decode strings into buffers before passing to _write?
  // this is here so that some node-core streams can optimize string
  // handling at a lower level.
  decodeStrings: makeBitMapDescriptor(kDecodeStrings),

  // A flag to see when we're in the middle of a write.
  writing: makeBitMapDescriptor(kWriting),

  // A flag to be able to tell if the onwrite cb is called immediately,
  // or on a later tick.  We set this to true at first, because any
  // actions that shouldn't happen until "later" should generally also
  // not happen before the first write call.
  sync: makeBitMapDescriptor(kSync),

  // A flag to know if we're processing previously buffered items, which
  // may call the _write() callback in the same tick, so that we don't
  // end up in an overlapped onwrite situation.
  bufferProcessing: makeBitMapDescriptor(kBufferProcessing),

  // Stream is still being constructed and cannot be
  // destroyed until construction finished or failed.
  // Async construction is opt in, therefore we start as
  // constructed.
  constructed: makeBitMapDescriptor(kConstructed),

  // Emit prefinish if the only thing we're waiting for is _write cbs
  // This is relevant for synchronous Transform streams.
  prefinished: makeBitMapDescriptor(kPrefinished),

  // True if the error was already emitted and should not be thrown again.
  errorEmitted: makeBitMapDescriptor(kErrorEmitted),

  // Should close be emitted on destroy. Defaults to true.
  emitClose: makeBitMapDescriptor(kEmitClose),

  // Should .destroy() be called after 'finish' (and potentially 'end').
  autoDestroy: makeBitMapDescriptor(kAutoDestroy),

  // Indicates whether the stream has finished destroying.
  closed: makeBitMapDescriptor(kClosed),

  // True if close has been emitted or would have been emitted
  // depending on emitClose.
  closeEmitted: makeBitMapDescriptor(kCloseEmitted),

  allBuffers: makeBitMapDescriptor(kAllBuffers),
  allNoop: makeBitMapDescriptor(kAllNoop),

  // Indicates whether the stream has errored. When true all write() calls
  // should return false. This is needed since when autoDestroy
  // is disabled we need a way to tell whether the stream has failed.
  // This is/should be a cold path.
  errored: {
    __proto__: null,
    enumerable: false,
    get() { return (this.state & kErrored) !== 0 ? this[kErroredValue] : null; },
    set(value) {
      if (value) {
        this[kErroredValue] = value;
        this.state |= kErrored;
      } else {
        this.state &= ~kErrored;
      }
    },
  },

  writable: {
    __proto__: null,
    enumerable: false,
    get() { return (this.state & kHasWritable) !== 0 ? (this.state & kWritable) !== 0 : undefined; },
    set(value) {
      if (value == null) {
        this.state &= ~(kHasWritable | kWritable);
      } else if (value) {
        this.state |= (kHasWritable | kWritable);
      } else {
        this.state |= kHasWritable;
        this.state &= ~kWritable;
      }
    },
  },

  defaultEncoding: {
    __proto__: null,
    enumerable: false,
    get() { return (this.state & kDefaultUTF8Encoding) !== 0 ? 'utf8' : this[kDefaultEncodingValue]; },
    set(value) {
      if (value === 'utf8' || value === 'utf-8') {
        this.state |= kDefaultUTF8Encoding;
      } else {
        this.state &= ~kDefaultUTF8Encoding;
        this[kDefaultEncodingValue] = value;
      }
    },
  },

  // The callback that the user supplies to write(chunk, encoding, cb).
  writecb: {
    __proto__: null,
    enumerable: false,
    get() { return (this.state & kWriteCb) !== 0 ? this[kWriteCbValue] : nop; },
    set(value) {
      if (value) {
        this[kWriteCbValue] = value;
        this.state |= kWriteCb;
      } else {
        this.state &= ~kWriteCb;
      }
    },
  },

  // Storage for data passed to the afterWrite() callback in case of
  // synchronous _write() completion.
  afterWriteTickInfo: {
    __proto__: null,
    enumerable: false,
    get() { return (this.state & kAfterWriteTickInfo) !== 0 ? this[kAfterWriteTickInfoValue] : null; },
    set(value) {
      if (value) {
        this[kAfterWriteTickInfoValue] = value;
        this.state |= kAfterWriteTickInfo;
      } else {
        this.state &= ~kAfterWriteTickInfo;
      }
    },
  },
});

function WritableState(options, stream, isDuplex) {
  // Duplex streams are both readable and writable, but share
  // the same options object.
  // However, some cases require setting options to different
  // values for the readable and the writable sides of the duplex stream,
  // e.g. options.readableObjectMode vs. options.writableObjectMode, etc.
  if (typeof isDuplex !== 'boolean')
    isDuplex = stream instanceof Stream.Duplex;

  // Bit map field to store WritableState more effciently with 1 bit per field
  // instead of a V8 slot per field.
  this.state = kSync | kConstructed | kEmitClose | kAutoDestroy;

  if (options && options.objectMode) this.state |= kObjectMode;
  if (isDuplex && options && options.writableObjectMode) this.state |= kObjectMode;

  // The point at which write() starts returning false
  // Note: 0 is a valid value, means that we always return false if
  // the entire buffer is not flushed immediately on write().
  this.highWaterMark = options ?
    getHighWaterMark(this, options, 'writableHighWaterMark', isDuplex) :
    getDefaultHighWaterMark(false);

  if (!options || options.decodeStrings !== false) this.state |= kDecodeStrings;

  // Should close be emitted on destroy. Defaults to true.
  if (options && options.emitClose === false) this.state &= ~kEmitClose;

  // Should .destroy() be called after 'end' (and potentially 'finish').
  if (options && options.autoDestroy === false) this.state &= ~kAutoDestroy;

  // Crypto is kind of old and crusty.  Historically, its default string
  // encoding is 'binary' so we have to make this configurable.
  // Everything else in the universe uses 'utf8', though.
  const defaultEncoding = options?.defaultEncoding;
  if (defaultEncoding == null || defaultEncoding === 'utf8' || defaultEncoding === 'utf-8') {
    this.state |= kDefaultUTF8Encoding;
  } else if (Buffer.isEncoding(defaultEncoding)) {
    this.state &= ~kDefaultUTF8Encoding;
    this[kDefaultEncodingValue] = defaultEncoding;
  } else {
    throw new ERR_UNKNOWN_ENCODING(defaultEncoding);
  }

  // Not an actual buffer we keep track of, but a measurement
  // of how much we're waiting to get pushed to some underlying
  // socket or file.
  this.length = 0;

  // When true all writes will be buffered until .uncork() call.
  this.corked = 0;

  // The callback that's passed to _write(chunk, cb).
  this.onwrite = onwrite.bind(undefined, stream);

  // The amount that is being written when _write is called.
  this.writelen = 0;

  resetBuffer(this);

  // Number of pending user-supplied write callbacks
  // this must be 0 before 'finish' can be emitted.
  this.pendingcb = 0;
}

function resetBuffer(state) {
  state.buffered = [];
  state.bufferedIndex = 0;
  state.state |= kAllBuffers | kAllNoop;
}

WritableState.prototype.getBuffer = function getBuffer() {
  return ArrayPrototypeSlice(this.buffered, this.bufferedIndex);
};

ObjectDefineProperty(WritableState.prototype, 'bufferedRequestCount', {
  __proto__: null,
  get() {
    return this.buffered.length - this.bufferedIndex;
  },
});

function Writable(options) {
  // Writable ctor is applied to Duplexes, too.
  // `realHasInstance` is necessary because using plain `instanceof`
  // would return false, as no `_writableState` property is attached.

  // Trying to use the custom `instanceof` for Writable here will also break the
  // Node.js LazyTransform implementation, which has a non-trivial getter for
  // `_writableState` that would lead to infinite recursion.

  // Checking for a Stream.Duplex instance is faster here instead of inside
  // the WritableState constructor, at least with V8 6.5.
  const isDuplex = (this instanceof Stream.Duplex);

  if (!isDuplex && !FunctionPrototypeSymbolHasInstance(Writable, this))
    return new Writable(options);

  this._writableState = new WritableState(options, this, isDuplex);

  if (options) {
    if (typeof options.write === 'function')
      this._write = options.write;

    if (typeof options.writev === 'function')
      this._writev = options.writev;

    if (typeof options.destroy === 'function')
      this._destroy = options.destroy;

    if (typeof options.final === 'function')
      this._final = options.final;

    if (typeof options.construct === 'function')
      this._construct = options.construct;

    if (options.signal)
      addAbortSignal(options.signal, this);
  }

  Stream.call(this, options);

  destroyImpl.construct(this, () => {
    const state = this._writableState;

    if (!state.writing) {
      clearBuffer(this, state);
    }

    finishMaybe(this, state);
  });
}

ObjectDefineProperty(Writable, SymbolHasInstance, {
  __proto__: null,
  value: function(object) {
    if (FunctionPrototypeSymbolHasInstance(this, object)) return true;
    if (this !== Writable) return false;

    return object && object._writableState instanceof WritableState;
  },
});

// Otherwise people can pipe Writable streams, which is just wrong.
Writable.prototype.pipe = function() {
  errorOrDestroy(this, new ERR_STREAM_CANNOT_PIPE());
};

function _write(stream, chunk, encoding, cb) {
  const state = stream._writableState;

  if (typeof encoding === 'function') {
    cb = encoding;
    encoding = (state.state & kDefaultUTF8Encoding) !== 0 ? 'utf8' : state.defaultEncoding;
  } else {
    if (!encoding)
      encoding = (state.state & kDefaultUTF8Encoding) !== 0 ? 'utf8' : state.defaultEncoding;
    else if (encoding !== 'buffer' && !Buffer.isEncoding(encoding))
      throw new ERR_UNKNOWN_ENCODING(encoding);
    if (typeof cb !== 'function')
      cb = nop;
  }

  if (chunk === null) {
    throw new ERR_STREAM_NULL_VALUES();
  } else if ((state.state & kObjectMode) === 0) {
    if (typeof chunk === 'string') {
      if ((state.state & kDecodeStrings) !== 0) {
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
  if ((state.state & kEnding) !== 0) {
    err = new ERR_STREAM_WRITE_AFTER_END();
  } else if ((state.state & kDestroyed) !== 0) {
    err = new ERR_STREAM_DESTROYED('write');
  }

  if (err) {
    process.nextTick(cb, err);
    errorOrDestroy(stream, err, true);
    return err;
  }
  state.pendingcb++;
  return writeOrBuffer(stream, state, chunk, encoding, cb);
}

Writable.prototype.write = function(chunk, encoding, cb) {
  return _write(this, chunk, encoding, cb) === true;
};

Writable.prototype.cork = function() {
  const state = this._writableState;

  state.state |= kCorked;
  state.corked++;
};

Writable.prototype.uncork = function() {
  const state = this._writableState;

  if (state.corked) {
    state.corked--;

    if (!state.corked) {
      state.state &= ~kCorked;
    }

    if ((state.state & kWriting) === 0)
      clearBuffer(this, state);
  }
};

Writable.prototype.setDefaultEncoding = function setDefaultEncoding(encoding) {
  // node::ParseEncoding() requires lower case.
  if (typeof encoding === 'string')
    encoding = StringPrototypeToLowerCase(encoding);
  if (!Buffer.isEncoding(encoding))
    throw new ERR_UNKNOWN_ENCODING(encoding);
  this._writableState.defaultEncoding = encoding;
  return this;
};

// If we're already writing something, then just put this
// in the queue, and wait our turn.  Otherwise, call _write
// If we return false, then we need a drain event, so set that flag.
function writeOrBuffer(stream, state, chunk, encoding, callback) {
  const len = (state.state & kObjectMode) !== 0 ? 1 : chunk.length;

  state.length += len;

  // stream._write resets state.length
  const ret = state.length < state.highWaterMark;

  // We must ensure that previous needDrain will not be reset to false.
  if (!ret) {
    state.state |= kNeedDrain;
  }

  if ((state.state & (kWriting | kErrored | kCorked | kConstructed)) !== kConstructed) {
    state.buffered.push({ chunk, encoding, callback });
    if ((state.state & kAllBuffers) !== 0 && encoding !== 'buffer') {
      state.state &= ~kAllBuffers;
    }
    if ((state.state & kAllNoop) !== 0 && callback !== nop) {
      state.state &= ~kAllNoop;
    }
  } else {
    state.writelen = len;
    if (callback !== nop) {
      state.writecb = callback;
    }
    state.state |= kWriting | kSync | kExpectWriteCb;
    stream._write(chunk, encoding, state.onwrite);
    state.state &= ~kSync;
  }

  // Return false if errored or destroyed in order to break
  // any synchronous while(stream.write(data)) loops.
  return ret && (state.state & (kDestroyed | kErrored)) === 0;
}

function doWrite(stream, state, writev, len, chunk, encoding, cb) {
  state.writelen = len;
  if (cb !== nop) {
    state.writecb = cb;
  }
  state.state |= kWriting | kSync | kExpectWriteCb;
  if ((state.state & kDestroyed) !== 0)
    state.onwrite(new ERR_STREAM_DESTROYED('write'));
  else if (writev)
    stream._writev(chunk, state.onwrite);
  else
    stream._write(chunk, encoding, state.onwrite);
  state.state &= ~kSync;
}

function onwriteError(stream, state, er, cb) {
  --state.pendingcb;

  cb(er);
  // Ensure callbacks are invoked even when autoDestroy is
  // not enabled. Passing `er` here doesn't make sense since
  // it's related to one specific write, not to the buffered
  // writes.
  errorBuffer(state);
  // This can emit error, but error must always follow cb.
  errorOrDestroy(stream, er);
}

function onwrite(stream, er) {
  const state = stream._writableState;

  if ((state.state & kExpectWriteCb) === 0) {
    errorOrDestroy(stream, new ERR_MULTIPLE_CALLBACK());
    return;
  }

  const sync = (state.state & kSync) !== 0;
  const cb = (state.state & kWriteCb) !== 0 ? state[kWriteCbValue] : nop;

  state.state &= ~(kWriting | kExpectWriteCb | kWriteCb);
  state.length -= state.writelen;
  state.writelen = 0;

  if (er) {
    // Avoid V8 leak, https://github.com/nodejs/node/pull/34103#issuecomment-652002364
    er.stack; // eslint-disable-line no-unused-expressions

    if (!state.errored) {
      state.errored = er;
    }

    // In case of duplex streams we need to notify the readable side of the
    // error.
    if (stream._readableState && !stream._readableState.errored) {
      stream._readableState.errored = er;
    }

    if (sync) {
      process.nextTick(onwriteError, stream, state, er, cb);
    } else {
      onwriteError(stream, state, er, cb);
    }
  } else {
    if (state.buffered.length > state.bufferedIndex) {
      clearBuffer(stream, state);
    }

    if (sync) {
      // It is a common case that the callback passed to .write() is always
      // the same. In that case, we do not schedule a new nextTick(), but
      // rather just increase a counter, to improve performance and avoid
      // memory allocations.
      if (cb === nop) {
        if ((state.state & kAfterWritePending) === 0) {
          process.nextTick(afterWrite, stream, state, 1, cb);
          state.state |= kAfterWritePending;
        } else {
          state.pendingcb -= 1;
        }
      } else if (state.afterWriteTickInfo !== null &&
                 state.afterWriteTickInfo.cb === cb) {
        state.afterWriteTickInfo.count++;
      } else {
        state.afterWriteTickInfo = { count: 1, cb, stream, state };
        process.nextTick(afterWriteTick, state.afterWriteTickInfo);
        state.state |= kAfterWritePending;
      }
    } else {
      afterWrite(stream, state, 1, cb);
    }
  }
}

function afterWriteTick({ stream, state, count, cb }) {
  state.afterWriteTickInfo = null;
  return afterWrite(stream, state, count, cb);
}

function afterWrite(stream, state, count, cb) {
  state.state &= ~kAfterWritePending;

  const needDrain = (state.state & (kEnding | kNeedDrain | kDestroyed)) === kNeedDrain && state.length === 0;
  if (needDrain) {
    state.state &= ~kNeedDrain;
    stream.emit('drain');
  }

  while (count-- > 0) {
    state.pendingcb--;
    cb(null);
  }

  if ((state.state & kDestroyed) !== 0) {
    errorBuffer(state);
  }

  finishMaybe(stream, state);
}

// If there's something in the buffer waiting, then invoke callbacks.
function errorBuffer(state) {
  if ((state.state & kWriting) !== 0) {
    return;
  }

  for (let n = state.bufferedIndex; n < state.buffered.length; ++n) {
    const { chunk, callback } = state.buffered[n];
    const len = (state.state & kObjectMode) !== 0 ? 1 : chunk.length;
    state.length -= len;
    callback(state.errored ?? new ERR_STREAM_DESTROYED('write'));
  }


  callFinishedCallbacks(state, state.errored ?? new ERR_STREAM_DESTROYED('end'));

  resetBuffer(state);
}

// If there's something in the buffer waiting, then process it.
function clearBuffer(stream, state) {
  if ((state.state & (kDestroyed | kBufferProcessing | kCorked)) !== 0 ||
    (state.state & kConstructed) === 0) {
    return;
  }

  const objectMode = (state.state & kObjectMode) !== 0;
  const { buffered, bufferedIndex } = state;
  const bufferedLength = buffered.length - bufferedIndex;

  if (!bufferedLength) {
    return;
  }

  let i = bufferedIndex;

  state.state |= kBufferProcessing;
  if (bufferedLength > 1 && stream._writev) {
    state.pendingcb -= bufferedLength - 1;

    const callback = (state.state & kAllNoop) !== 0 ? nop : (err) => {
      for (let n = i; n < buffered.length; ++n) {
        buffered[n].callback(err);
      }
    };
    // Make a copy of `buffered` if it's going to be used by `callback` above,
    // since `doWrite` will mutate the array.
    const chunks = (state.state & kAllNoop) !== 0 && i === 0 ?
      buffered : ArrayPrototypeSlice(buffered, i);
    chunks.allBuffers = (state.state & kAllBuffers) !== 0;

    doWrite(stream, state, true, state.length, chunks, '', callback);

    resetBuffer(state);
  } else {
    do {
      const { chunk, encoding, callback } = buffered[i];
      buffered[i++] = null;
      const len = objectMode ? 1 : chunk.length;
      doWrite(stream, state, false, len, chunk, encoding, callback);
    } while (i < buffered.length && (state.state & kWriting) === 0);

    if (i === buffered.length) {
      resetBuffer(state);
    } else if (i > 256) {
      buffered.splice(0, i);
      state.bufferedIndex = 0;
    } else {
      state.bufferedIndex = i;
    }
  }
  state.state &= ~kBufferProcessing;
}

Writable.prototype._write = function(chunk, encoding, cb) {
  if (this._writev) {
    this._writev([{ chunk, encoding }], cb);
  } else {
    throw new ERR_METHOD_NOT_IMPLEMENTED('_write()');
  }
};

Writable.prototype._writev = null;

Writable.prototype.end = function(chunk, encoding, cb) {
  const state = this._writableState;

  if (typeof chunk === 'function') {
    cb = chunk;
    chunk = null;
    encoding = null;
  } else if (typeof encoding === 'function') {
    cb = encoding;
    encoding = null;
  }

  let err;

  if (chunk != null) {
    const ret = _write(this, chunk, encoding);
    if (ret instanceof Error) {
      err = ret;
    }
  }

  // .end() fully uncorks.
  if ((state.state & kCorked) !== 0) {
    state.corked = 1;
    this.uncork();
  }

  if (err) {
    // Do nothing...
  } else if ((state.state & (kEnding | kErrored)) === 0) {
    // This is forgiving in terms of unnecessary calls to end() and can hide
    // logic errors. However, usually such errors are harmless and causing a
    // hard error can be disproportionately destructive. It is not always
    // trivial for the user to determine whether end() needs to be called
    // or not.

    state.state |= kEnding;
    finishMaybe(this, state, true);
    state.state |= kEnded;
  } else if ((state.state & kFinished) !== 0) {
    err = new ERR_STREAM_ALREADY_FINISHED('end');
  } else if ((state.state & kDestroyed) !== 0) {
    err = new ERR_STREAM_DESTROYED('end');
  }

  if (typeof cb === 'function') {
    if (err) {
      process.nextTick(cb, err);
    } else if ((state.state & kFinished) !== 0) {
      process.nextTick(cb, null);
    } else {
      state.state |= kOnFinished;
      state[kOnFinishedValue] ??= [];
      state[kOnFinishedValue].push(cb);
    }
  }

  return this;
};

function needFinish(state) {
  return (
    // State is ended && constructed but not destroyed, finished, writing, errorEmitted or closedEmitted
    (state.state & (
      kEnding |
          kDestroyed |
          kConstructed |
          kFinished |
          kWriting |
          kErrorEmitted |
          kCloseEmitted |
          kErrored
    )) === (kEnding | kConstructed) &&
          state.length === 0 &&
          state.buffered.length === 0);
}

function callFinal(stream, state) {
  let called = false;

  function onFinish(err) {
    if (called) {
      errorOrDestroy(stream, err ?? ERR_MULTIPLE_CALLBACK());
      return;
    }
    called = true;

    state.pendingcb--;
    if (err) {
      callFinishedCallbacks(state, err);
      errorOrDestroy(stream, err, (state.state & kSync) !== 0);
    } else if (needFinish(state)) {
      state.state |= kPrefinished;
      stream.emit('prefinish');
      // Backwards compat. Don't check state.sync here.
      // Some streams assume 'finish' will be emitted
      // asynchronously relative to _final callback.
      state.pendingcb++;
      process.nextTick(finish, stream, state);
    }
  }

  state.state |= kSync;
  state.pendingcb++;

  try {
    stream._final(onFinish);
  } catch (err) {
    onFinish(err);
  }

  state.state &= ~kSync;
}

function prefinish(stream, state) {
  if ((state.state & (kPrefinished | kFinalCalled)) === 0) {
    if (typeof stream._final === 'function' && (state.state & kDestroyed) === 0) {
      state.state |= kFinalCalled;
      callFinal(stream, state);
    } else {
      state.state |= kPrefinished;
      stream.emit('prefinish');
    }
  }
}

function finishMaybe(stream, state, sync) {
  if (needFinish(state)) {
    prefinish(stream, state);
    if (state.pendingcb === 0) {
      if (sync) {
        state.pendingcb++;
        process.nextTick((stream, state) => {
          if (needFinish(state)) {
            finish(stream, state);
          } else {
            state.pendingcb--;
          }
        }, stream, state);
      } else if (needFinish(state)) {
        state.pendingcb++;
        finish(stream, state);
      }
    }
  }
}

function finish(stream, state) {
  state.pendingcb--;
  state.state |= kFinished;

  callFinishedCallbacks(state, null);

  stream.emit('finish');

  if ((state.state & kAutoDestroy) !== 0) {
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

function callFinishedCallbacks(state, err) {
  if ((state.state & kOnFinished) === 0) {
    return;
  }

  const onfinishCallbacks = state[kOnFinishedValue];
  state[kOnFinishedValue] = null;
  state.state &= ~kOnFinished;
  for (let i = 0; i < onfinishCallbacks.length; i++) {
    onfinishCallbacks[i](err);
  }
}

ObjectDefineProperties(Writable.prototype, {
  closed: {
    __proto__: null,
    get() {
      return this._writableState ? (this._writableState.state & kClosed) !== 0 : false;
    },
  },

  destroyed: {
    __proto__: null,
    get() {
      return this._writableState ? (this._writableState.state & kDestroyed) !== 0 : false;
    },
    set(value) {
      // Backward compatibility, the user is explicitly managing destroyed.
      if (!this._writableState) return;

      if (value) this._writableState.state |= kDestroyed;
      else this._writableState.state &= ~kDestroyed;
    },
  },

  writable: {
    __proto__: null,
    get() {
      const w = this._writableState;
      // w.writable === false means that this is part of a Duplex stream
      // where the writable side was disabled upon construction.
      // Compat. The user might manually disable writable side through
      // deprecated setter.
      return !!w && w.writable !== false && !w.errored &&
        (w.state & (kEnding | kEnded | kDestroyed)) === 0;
    },
    set(val) {
      // Backwards compatible.
      if (this._writableState) {
        this._writableState.writable = !!val;
      }
    },
  },

  writableFinished: {
    __proto__: null,
    get() {
      const state = this._writableState;
      return state ? (state.state & kFinished) !== 0 : false;
    },
  },

  writableObjectMode: {
    __proto__: null,
    get() {
      const state = this._writableState;
      return state ? (state.state & kObjectMode) !== 0 : false;
    },
  },

  writableBuffer: {
    __proto__: null,
    get() {
      const state = this._writableState;
      return state && state.getBuffer();
    },
  },

  writableEnded: {
    __proto__: null,
    get() {
      const state = this._writableState;
      return state ? (state.state & kEnding) !== 0 : false;
    },
  },

  writableNeedDrain: {
    __proto__: null,
    get() {
      const state = this._writableState;
      return state ? (state.state & (kDestroyed | kEnding | kNeedDrain)) === kNeedDrain : false;
    },
  },

  writableHighWaterMark: {
    __proto__: null,
    get() {
      const state = this._writableState;
      return state && state.highWaterMark;
    },
  },

  writableCorked: {
    __proto__: null,
    get() {
      const state = this._writableState;
      return state ? state.corked : 0;
    },
  },

  writableLength: {
    __proto__: null,
    get() {
      const state = this._writableState;
      return state && state.length;
    },
  },

  errored: {
    __proto__: null,
    enumerable: false,
    get() {
      const state = this._writableState;
      return state ? state.errored : null;
    },
  },

  writableAborted: {
    __proto__: null,
    get: function() {
      const state = this._writableState;
      return (
        (state.state & (kHasWritable | kWritable)) !== kHasWritable &&
        (state.state & (kDestroyed | kErrored)) !== 0 &&
        (state.state & kFinished) === 0
      );
    },
  },
});

const destroy = destroyImpl.destroy;
Writable.prototype.destroy = function(err, cb) {
  const state = this._writableState;

  // Invoke pending callbacks.
  if ((state.state & kDestroyed) === 0 &&
    (state.bufferedIndex < state.buffered.length ||
      (state.state & kOnFinished) !== 0)) {
    process.nextTick(errorBuffer, state);
  }

  destroy.call(this, err, cb);
  return this;
};

Writable.prototype._undestroy = destroyImpl.undestroy;
Writable.prototype._destroy = function(err, cb) {
  cb(err);
};

Writable.prototype[EE.captureRejectionSymbol] = function(err) {
  this.destroy(err);
};

let webStreamsAdapters;

// Lazy to avoid circular references
function lazyWebStreams() {
  if (webStreamsAdapters === undefined)
    webStreamsAdapters = require('internal/webstreams/adapters');
  return webStreamsAdapters;
}

Writable.fromWeb = function(writableStream, options) {
  return lazyWebStreams().newStreamWritableFromWritableStream(
    writableStream,
    options);
};

Writable.toWeb = function(streamWritable) {
  return lazyWebStreams().newWritableStreamFromStreamWritable(streamWritable);
};

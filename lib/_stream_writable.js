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
  FunctionPrototype,
  ObjectDefineProperty,
  ObjectDefineProperties,
  ObjectGetOwnPropertyDescriptors,
  ObjectCreate,
  ObjectKeys,
  Symbol,
  SymbolHasInstance,
} = primordials;

module.exports = Writable;

const Stream = require('stream');
const {
  WritableBase,
  WritableStateBase,
  errorOrDestroy
} = require('internal/streams/base');

const {
  getHighWaterMark,
  getDefaultHighWaterMark
} = require('internal/streams/state');
const {
  ERR_METHOD_NOT_IMPLEMENTED,
  ERR_MULTIPLE_CALLBACK,
  ERR_STREAM_DESTROYED
} = require('internal/errors').codes;

function nop() {}

const kFlush = Symbol('kFlush');
class WritableState extends WritableStateBase {
  constructor(options, stream, isDuplex) {
    super(options);

    // Duplex streams are both readable and writable, but share
    // the same options object.
    // However, some cases require setting options to different
    // values for the readable and the writable sides of the duplex stream,
    // e.g. options.readableObjectMode vs. options.writableObjectMode, etc.
    if (typeof isDuplex !== 'boolean')
      isDuplex = stream instanceof Stream.Duplex;

    if (isDuplex)
      this.objectMode = this.objectMode ||
        !!(options && options.writableObjectMode);

    // The point at which write() starts returning false
    // Note: 0 is a valid value, means that we always return false if
    // the entire buffer is not flushed immediately on write().
    this.highWaterMark = options ?
      getHighWaterMark(this, options, 'writableHighWaterMark', isDuplex) :
      getDefaultHighWaterMark(false);

    // if _final has been called.
    this.finalCalled = false;

    // drain event flag.
    this.needDrain = false;

    // Not an actual buffer we keep track of, but a measurement
    // of how much we're waiting to get pushed to some underlying
    // socket or file.
    this.length = 0;

    // A flag to see when we're in the middle of a write.
    this.writing = false;

    // When true all writes will be buffered until .uncork() call.
    this.corked = 0;

    // A flag to be able to tell if the onwrite cb is called immediately,
    // or on a later tick.  We set this to true at first, because any
    // actions that shouldn't happen until "later" should generally also
    // not happen before the first write call.
    this.sync = true;

    // A flag to know if we're processing previously buffered items, which
    // may call the _write() callback in the same tick, so that we don't
    // end up in an overlapped onwrite situation.
    this.bufferProcessing = false;

    // The callback that's passed to _write(chunk, cb).
    this.onwrite = onwrite.bind(undefined, stream);

    // The callback that the user supplies to write(chunk, encoding, cb).
    this.writecb = null;

    // The amount that is being written when _write is called.
    this.writelen = 0;

    // Storage for data passed to the afterWrite() callback in case of
    // synchronous _write() completion.
    this.afterWriteTickInfo = null;

    resetBuffer(this);

    // Number of pending user-supplied write callbacks
    // this must be 0 before 'finish' can be emitted.
    this.pendingcb = 0;

    // Emit prefinish if the only thing we're waiting for is _write cbs
    // This is relevant for synchronous Transform streams.
    this.prefinished = false;

    this[kFlush] = null;
  }

  getBuffer() {
    return this.buffered.slice(this.bufferedIndex);
  }

  get bufferedRequestCount() {
    return this.buffered.length - this.bufferedIndex;
  }
}

function resetBuffer(state) {
  state.buffered = [];
  state.bufferedIndex = 0;
  state.allBuffers = true;
  state.allNoop = true;
}

// Test _writableState for inheritance to account for Duplex streams,
// whose prototype chain only points to Readable.
let realHasInstance;
if (typeof Symbol === 'function' && SymbolHasInstance) {
  realHasInstance = FunctionPrototype[SymbolHasInstance];
  ObjectDefineProperty(Writable, SymbolHasInstance, {
    value: function(object) {
      if (realHasInstance.call(this, object))
        return true;
      if (this !== Writable)
        return false;

      return object && object._writableState instanceof WritableState;
    }
  });
} else {
  realHasInstance = function(object) {
    return object instanceof this;
  };
}

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

  if (!isDuplex && !realHasInstance.call(Writable, this))
    return new Writable(options);

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
  }

  WritableBase.call(this, {
    ...options,
    start: function() {
      const state = this._writableState;
      if (!state.writing) {
        clearBuffer(this, state);
      }
      finishMaybe(this, state);
    },
    write: writeOrBuffer,
    flush: function(state, cb) {
      // .end() fully uncorks.
      if (state.corked) {
        state.corked = 1;
        this.uncork();
      }

      state[kFlush] = cb;

      finishMaybe(this, state);
    }
  }, isDuplex, WritableState);
}
Writable.WritableState = WritableState;
Writable.prototype = ObjectCreate(WritableBase.prototype);

Writable.prototype.cork = function() {
  this._writableState.corked++;
};

Writable.prototype.uncork = function() {
  const state = this._writableState;

  if (state.corked) {
    state.corked--;

    if (!state.writing)
      clearBuffer(this, state);
  }
};

// If we're already writing something, then just put this
// in the queue, and wait our turn.  Otherwise, call _write
// If we return false, then we need a drain event, so set that flag.
function writeOrBuffer(state, chunk, encoding, callback) {
  if (typeof callback !== 'function')
    callback = nop;

  state.pendingcb++;

  const len = state.objectMode ? 1 : chunk.length;

  state.length += len;

  // TODO(ronag): Move errored handling to Base.
  if (state.writing || state.corked || state.errored || !state.constructed) {
    state.buffered.push({ chunk, encoding, callback });
    if (state.allBuffers && encoding !== 'buffer') {
      state.allBuffers = false;
    }
    if (state.allNoop && callback !== nop) {
      state.allNoop = false;
    }
  } else {
    state.writelen = len;
    state.writecb = callback;
    state.writing = true;
    state.sync = true;
    this._write(chunk, encoding, state.onwrite);
    state.sync = false;
  }

  const ret = state.length < state.highWaterMark;

  // We must ensure that previous needDrain will not be reset to false.
  if (!ret)
    state.needDrain = true;

  return ret;
}

function doWrite(stream, state, writev, len, chunk, encoding, cb) {
  state.writelen = len;
  state.writecb = cb;
  state.writing = true;
  state.sync = true;
  if (state.destroyed)
    state.onwrite(new ERR_STREAM_DESTROYED('write'));
  else if (writev)
    stream._writev(chunk, state.onwrite);
  else
    stream._write(chunk, encoding, state.onwrite);
  state.sync = false;
}

function onwriteError(stream, state, er, cb) {
  --state.pendingcb;

  cb(er);
  // Ensure callbacks are invoked even when autoDestroy is
  // not enabled. Passing `er` here doesn't make sense since
  // it's related to one specific write, not to the buffered
  // writes.
  errorBuffer(state, new ERR_STREAM_DESTROYED('write'));
  // This can emit error, but error must always follow cb.
  errorOrDestroy(stream, er);
}

function onwrite(stream, er) {
  const state = stream._writableState;
  const sync = state.sync;
  const cb = state.writecb;

  if (typeof cb !== 'function') {
    errorOrDestroy(stream, new ERR_MULTIPLE_CALLBACK());
    return;
  }

  state.writing = false;
  state.writecb = null;
  state.length -= state.writelen;
  state.writelen = 0;

  if (er) {
    // TODO(ronag): Move errored handling to Base.

    state.errored = true;

    // In case of duplex streams we need to notify the readable side of the
    // error.
    if (stream._readableState) {
      stream._readableState.errored = true;
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
      if (state.afterWriteTickInfo !== null &&
          state.afterWriteTickInfo.cb === cb) {
        state.afterWriteTickInfo.count++;
      } else {
        state.afterWriteTickInfo = { count: 1, cb, stream, state };
        process.nextTick(afterWriteTick, state.afterWriteTickInfo);
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
  const needDrain = !state.ending && !stream.destroyed && state.length === 0 &&
    state.needDrain;
  if (needDrain) {
    state.needDrain = false;
    stream.emit('drain');
  }

  while (count-- > 0) {
    state.pendingcb--;
    cb();
  }

  if (state.destroyed) {
    errorBuffer(state, new ERR_STREAM_DESTROYED('write'));
  }

  finishMaybe(stream, state);
}

// If there's something in the buffer waiting, then invoke callbacks.
function errorBuffer(state, err) {
  if (state.writing) {
    return;
  }

  for (let n = state.bufferedIndex; n < state.buffered.length; ++n) {
    const { chunk, callback } = state.buffered[n];
    const len = state.objectMode ? 1 : chunk.length;
    state.length -= len;
    callback(err);
  }

  resetBuffer(state);
}

// If there's something in the buffer waiting, then process it.
function clearBuffer(stream, state) {
  if (state.corked ||
      state.bufferProcessing ||
      state.destroyed ||
      !state.constructed) {
    return;
  }

  const { buffered, bufferedIndex, objectMode } = state;
  const bufferedLength = buffered.length - bufferedIndex;

  if (!bufferedLength) {
    return;
  }

  let i = bufferedIndex;

  state.bufferProcessing = true;
  if (bufferedLength > 1 && stream._writev) {
    state.pendingcb -= bufferedLength - 1;

    const callback = state.allNoop ? nop : (err) => {
      for (let n = i; n < buffered.length; ++n) {
        buffered[n].callback(err);
      }
    };
    // Make a copy of `buffered` if it's going to be used by `callback` above,
    // since `doWrite` will mutate the array.
    const chunks = state.allNoop && i === 0 ? buffered : buffered.slice(i);
    chunks.allBuffers = state.allBuffers;

    doWrite(stream, state, true, state.length, chunks, '', callback);

    resetBuffer(state);
  } else {
    do {
      const { chunk, encoding, callback } = buffered[i];
      buffered[i++] = null;
      const len = objectMode ? 1 : chunk.length;
      doWrite(stream, state, false, len, chunk, encoding, callback);
    } while (i < buffered.length && !state.writing);

    if (i === buffered.length) {
      resetBuffer(state);
    } else if (i > 256) {
      buffered.splice(0, i);
      state.bufferedIndex = 0;
    } else {
      state.bufferedIndex = i;
    }
  }
  state.bufferProcessing = false;
}

Writable.prototype._write = function(chunk, encoding, cb) {
  if (this._writev) {
    this._writev([{ chunk, encoding }], cb);
  } else {
    throw new ERR_METHOD_NOT_IMPLEMENTED('_write()');
  }
};

Writable.prototype._writev = null;

function needFinish(state) {
  // TODO(ronag): Move errored and finished handling to Base.
  return (state[kFlush] &&
          state.constructed &&
          state.length === 0 &&
          !state.errored &&
          state.buffered.length === 0 &&
          !state.finished &&
          !state.writing);
}

function callFinal(stream, state) {
  state.pendingcb++;
  stream._final((err) => {
    state.pendingcb--;
    if (err) {
      state[kFlush](err);
    } else if (needFinish(state)) {
      state.prefinished = true;
      stream.emit('prefinish');
      // Backwards compat. Some streams assume 'finish' will be emitted
      // asynchronously relative to _final callback.
      process.nextTick(state[kFlush]);
    }
  });
}

function prefinish(stream, state) {
  if (!state.prefinished && !state.finalCalled) {
    if (typeof stream._final === 'function' && !state.destroyed) {
      state.finalCalled = true;
      callFinal(stream, state);
    } else {
      state.prefinished = true;
      stream.emit('prefinish');
    }
  }
}

function finishMaybe(stream, state) {
  if (needFinish(state)) {
    prefinish(stream, state);
    if (state.pendingcb === 0 && needFinish(state) && !state.finalCalled) {
      state[kFlush]();
    }
  }
}

for (const method of ObjectKeys(WritableBase.prototype)) {
  if (!Writable.prototype[method])
    Writable.prototype[method] = WritableBase.prototype[method];
}

ObjectDefineProperties(Writable.prototype, {
  ...ObjectGetOwnPropertyDescriptors(WritableBase.prototype),

  writableBuffer: {
    get() {
      return this._writableState && this._writableState.getBuffer();
    }
  },

  writableHighWaterMark: {
    get() {
      return this._writableState && this._writableState.highWaterMark;
    }
  },

  writableCorked: {
    get() {
      return this._writableState ? this._writableState.corked : 0;
    }
  },

  writableLength: {
    get() {
      return this._writableState && this._writableState.length;
    }
  }
});

Writable.prototype.destroy = function(err, cb) {
  const state = this._writableState;
  if (!state.destroyed) {
    process.nextTick(errorBuffer, state, new ERR_STREAM_DESTROYED('write'));
  }
  WritableBase.prototype.destroy.call(this, err, cb);
  return this;
};

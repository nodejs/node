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
  Array,
  FunctionPrototype,
  ObjectDefineProperty,
  ObjectDefineProperties,
  ObjectSetPrototypeOf,
  Symbol,
  SymbolHasInstance,
} = primordials;

module.exports = Writable;
Writable.WritableState = WritableState;

const EE = require('events');
const Stream = require('stream');
const { Buffer } = require('buffer');
const destroyImpl = require('internal/streams/destroy');
const {
  getHighWaterMark,
  getDefaultHighWaterMark
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
  ERR_UNKNOWN_ENCODING
} = require('internal/errors').codes;

const { errorOrDestroy } = destroyImpl;

ObjectSetPrototypeOf(Writable.prototype, Stream.prototype);
ObjectSetPrototypeOf(Writable, Stream);

function nop() {}

function WritableState(options, stream, isDuplex) {
  // Duplex streams are both readable and writable, but share
  // the same options object.
  // However, some cases require setting options to different
  // values for the readable and the writable sides of the duplex stream,
  // e.g. options.readableObjectMode vs. options.writableObjectMode, etc.
  if (typeof isDuplex !== 'boolean')
    isDuplex = stream instanceof Stream.Duplex;

  // Object stream flag to indicate whether or not this stream
  // contains buffers or objects.
  this.objectMode = !!(options && options.objectMode);

  if (isDuplex)
    this.objectMode = this.objectMode ||
      !!(options && options.writableObjectMode);

  // The point at which write() starts returning false
  // Note: 0 is a valid value, means that we always return false if
  // the entire buffer is not flushed immediately on write()
  this.highWaterMark = options ?
    getHighWaterMark(this, options, 'writableHighWaterMark', isDuplex) :
    getDefaultHighWaterMark(false);

  // if _final has been called
  this.finalCalled = false;

  // drain event flag.
  this.needDrain = false;
  // At the start of calling end()
  this.ending = false;
  // When end() has been called, and returned
  this.ended = false;
  // When 'finish' is emitted
  this.finished = false;

  // Has it been destroyed
  this.destroyed = false;

  // Should we decode strings into buffers before passing to _write?
  // this is here so that some node-core streams can optimize string
  // handling at a lower level.
  const noDecode = !!(options && options.decodeStrings === false);
  this.decodeStrings = !noDecode;

  // Crypto is kind of old and crusty.  Historically, its default string
  // encoding is 'binary' so we have to make this configurable.
  // Everything else in the universe uses 'utf8', though.
  this.defaultEncoding = (options && options.defaultEncoding) || 'utf8';

  // Not an actual buffer we keep track of, but a measurement
  // of how much we're waiting to get pushed to some underlying
  // socket or file.
  this.length = 0;

  // A flag to see when we're in the middle of a write.
  this.writing = false;

  // When true all writes will be buffered until .uncork() call
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

  // The callback that's passed to _write(chunk,cb)
  this.onwrite = onwrite.bind(undefined, stream);

  // The callback that the user supplies to write(chunk,encoding,cb)
  this.writecb = null;

  // The amount that is being written when _write is called.
  this.writelen = 0;

  // Storage for data passed to the afterWrite() callback in case of
  // synchronous _write() completion.
  this.afterWriteTickInfo = null;

  this.bufferedRequest = null;
  this.lastBufferedRequest = null;

  // Number of pending user-supplied write callbacks
  // this must be 0 before 'finish' can be emitted
  this.pendingcb = 0;

  // Emit prefinish if the only thing we're waiting for is _write cbs
  // This is relevant for synchronous Transform streams
  this.prefinished = false;

  // True if the error was already emitted and should not be thrown again
  this.errorEmitted = false;

  // Should close be emitted on destroy. Defaults to true.
  this.emitClose = !options || options.emitClose !== false;

  // Should .destroy() be called after 'finish' (and potentially 'end')
  this.autoDestroy = !options || options.autoDestroy !== false;

  // Indicates whether the stream has errored. When true all write() calls
  // should return false. This is needed since when autoDestroy
  // is disabled we need a way to tell whether the stream has failed.
  this.errored = false;

  // Indicates whether the stream has finished destroying.
  this.closed = false;

  // Count buffered requests
  this.bufferedRequestCount = 0;

  // Allocate the first CorkedRequest, there is always
  // one allocated and free to use, and we maintain at most two
  const corkReq = { next: null, entry: null, finish: undefined };
  corkReq.finish = onCorkedFinish.bind(undefined, corkReq, this);
  this.corkedRequestsFree = corkReq;
}

WritableState.prototype.getBuffer = function getBuffer() {
  let current = this.bufferedRequest;
  const out = [];
  while (current) {
    out.push(current);
    current = current.next;
  }
  return out;
};

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
  // the WritableState constructor, at least with V8 6.5
  const isDuplex = (this instanceof Stream.Duplex);

  if (!isDuplex && !realHasInstance.call(Writable, this))
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
  }

  Stream.call(this, options);
}

// Otherwise people can pipe Writable streams, which is just wrong.
Writable.prototype.pipe = function() {
  errorOrDestroy(this, new ERR_STREAM_CANNOT_PIPE());
};

Writable.prototype.write = function(chunk, encoding, cb) {
  const state = this._writableState;

  if (typeof encoding === 'function') {
    cb = encoding;
    encoding = state.defaultEncoding;
  } else {
    if (!encoding)
      encoding = state.defaultEncoding;
    if (typeof cb !== 'function')
      cb = nop;
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
    process.nextTick(cb, err);
    errorOrDestroy(this, err, true);
    return false;
  } else {
    state.pendingcb++;
    return writeOrBuffer(this, state, chunk, encoding, cb);
  }
};

Writable.prototype.cork = function() {
  this._writableState.corked++;
};

Writable.prototype.uncork = function() {
  const state = this._writableState;

  if (state.corked) {
    state.corked--;

    if (!state.writing &&
        !state.corked &&
        !state.bufferProcessing &&
        state.bufferedRequest)
      clearBuffer(this, state);
  }
};

Writable.prototype.setDefaultEncoding = function setDefaultEncoding(encoding) {
  // node::ParseEncoding() requires lower case.
  if (typeof encoding === 'string')
    encoding = encoding.toLowerCase();
  if (!Buffer.isEncoding(encoding))
    throw new ERR_UNKNOWN_ENCODING(encoding);
  this._writableState.defaultEncoding = encoding;
  return this;
};

// If we're already writing something, then just put this
// in the queue, and wait our turn.  Otherwise, call _write
// If we return false, then we need a drain event, so set that flag.
function writeOrBuffer(stream, state, chunk, encoding, cb) {
  const len = state.objectMode ? 1 : chunk.length;

  state.length += len;

  const ret = state.length < state.highWaterMark;
  // We must ensure that previous needDrain will not be reset to false.
  if (!ret)
    state.needDrain = true;

  if (state.writing || state.corked || state.errored) {
    const last = state.lastBufferedRequest;
    state.lastBufferedRequest = {
      chunk,
      encoding,
      callback: cb,
      next: null
    };
    if (last) {
      last.next = state.lastBufferedRequest;
    } else {
      state.bufferedRequest = state.lastBufferedRequest;
    }
    state.bufferedRequestCount += 1;
  } else {
    doWrite(stream, state, false, len, chunk, encoding, cb);
  }

  // Return false if errored or destroyed in order to break
  // any synchronous while(stream.write(data)) loops.
  return ret && !state.errored && !state.destroyed;
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
    state.errored = true;
    if (sync) {
      process.nextTick(onwriteError, stream, state, er, cb);
    } else {
      onwriteError(stream, state, er, cb);
    }
  } else {
    // Check if we're actually ready to finish, but don't emit yet
    const finished = needFinish(state) || stream.destroyed;

    if (!finished &&
        !state.corked &&
        !state.bufferProcessing &&
        state.bufferedRequest) {
      clearBuffer(stream, state);
    }

    if (sync) {
      // It is a common case that the callback passed to .write() is always
      // the same. In that case, we do not schedule a new nextTick(), but rather
      // just increase a counter, to improve performance and avoid memory
      // allocations.
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
  if (state.writing || !state.bufferedRequest) {
    return;
  }

  for (let entry = state.bufferedRequest; entry; entry = entry.next) {
    const len = state.objectMode ? 1 : entry.chunk.length;
    state.length -= len;
    entry.callback(err);
  }
  state.bufferedRequest = null;
  state.lastBufferedRequest = null;
  state.bufferedRequestCount = 0;
}

// If there's something in the buffer waiting, then process it
function clearBuffer(stream, state) {
  state.bufferProcessing = true;
  let entry = state.bufferedRequest;

  if (stream._writev && entry && entry.next) {
    // Fast case, write everything using _writev()
    const l = state.bufferedRequestCount;
    const buffer = new Array(l);
    const holder = state.corkedRequestsFree;
    holder.entry = entry;

    let count = 0;
    let allBuffers = true;
    while (entry) {
      buffer[count] = entry;
      if (entry.encoding !== 'buffer')
        allBuffers = false;
      entry = entry.next;
      count += 1;
    }
    buffer.allBuffers = allBuffers;

    doWrite(stream, state, true, state.length, buffer, '', holder.finish);

    // doWrite is almost always async, defer these to save a bit of time
    // as the hot path ends with doWrite
    state.pendingcb++;
    state.lastBufferedRequest = null;
    if (holder.next) {
      state.corkedRequestsFree = holder.next;
      holder.next = null;
    } else {
      const corkReq = { next: null, entry: null, finish: undefined };
      corkReq.finish = onCorkedFinish.bind(undefined, corkReq, state);
      state.corkedRequestsFree = corkReq;
    }
    state.bufferedRequestCount = 0;
  } else {
    // Slow case, write chunks one-by-one
    while (entry) {
      const chunk = entry.chunk;
      const encoding = entry.encoding;
      const cb = entry.callback;
      const len = state.objectMode ? 1 : chunk.length;

      doWrite(stream, state, false, len, chunk, encoding, cb);
      entry = entry.next;
      state.bufferedRequestCount--;
      // If we didn't call the onwrite immediately, then
      // it means that we need to wait until it does.
      // also, that means that the chunk and cb are currently
      // being processed, so move the buffer counter past them.
      if (state.writing) {
        break;
      }
    }

    if (entry === null)
      state.lastBufferedRequest = null;
  }

  state.bufferedRequest = entry;
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

  if (chunk !== null && chunk !== undefined)
    this.write(chunk, encoding);

  // .end() fully uncorks
  if (state.corked) {
    state.corked = 1;
    this.uncork();
  }

  if (typeof cb !== 'function')
    cb = nop;

  // This is forgiving in terms of unnecessary calls to end() and can hide
  // logic errors. However, usually such errors are harmless and causing a
  // hard error can be disproportionately destructive. It is not always
  // trivial for the user to determine whether end() needs to be called or not.
  if (!state.errored && !state.ending) {
    endWritable(this, state, cb);
  } else if (state.finished) {
    process.nextTick(cb, new ERR_STREAM_ALREADY_FINISHED('end'));
  } else if (state.destroyed) {
    process.nextTick(cb, new ERR_STREAM_DESTROYED('end'));
  } else if (cb !== nop) {
    onFinished(this, state, cb);
  }

  return this;
};

function needFinish(state) {
  return (state.ending &&
          state.length === 0 &&
          !state.errored &&
          state.bufferedRequest === null &&
          !state.finished &&
          !state.writing);
}

function callFinal(stream, state) {
  stream._final((err) => {
    state.pendingcb--;
    if (err) {
      errorOrDestroy(stream, err);
    } else {
      state.prefinished = true;
      stream.emit('prefinish');
      finishMaybe(stream, state);
    }
  });
}

function prefinish(stream, state) {
  if (!state.prefinished && !state.finalCalled) {
    if (typeof stream._final === 'function' && !state.destroyed) {
      state.pendingcb++;
      state.finalCalled = true;
      process.nextTick(callFinal, stream, state);
    } else {
      state.prefinished = true;
      stream.emit('prefinish');
    }
  }
}

function finishMaybe(stream, state, sync) {
  const need = needFinish(state);
  if (need) {
    prefinish(stream, state);
    if (state.pendingcb === 0) {
      state.pendingcb++;
      if (sync) {
        process.nextTick(finish, stream, state);
      } else {
        finish(stream, state);
      }
    }
  }
  return need;
}

function finish(stream, state) {
  state.pendingcb--;
  if (state.errorEmitted)
    return;

  state.finished = true;
  stream.emit('finish');

  if (state.autoDestroy) {
    // In case of duplex streams we need a way to detect
    // if the readable side is ready for autoDestroy as well
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

function endWritable(stream, state, cb) {
  state.ending = true;
  finishMaybe(stream, state, true);
  if (cb !== nop) {
    if (state.finished)
      process.nextTick(cb);
    else
      onFinished(stream, state, cb);
  }
  state.ended = true;
}

function onCorkedFinish(corkReq, state, err) {
  let entry = corkReq.entry;
  corkReq.entry = null;
  while (entry) {
    const cb = entry.callback;
    state.pendingcb--;
    cb(err);
    entry = entry.next;
  }

  // Reuse the free corkReq.
  state.corkedRequestsFree.next = corkReq;
}

// TODO(ronag): Avoid using events to implement internal logic.
function onFinished(stream, state, cb) {
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

ObjectDefineProperties(Writable.prototype, {

  destroyed: {
    get() {
      return this._writableState ? this._writableState.destroyed : false;
    },
    set(value) {
      // Backward compatibility, the user is explicitly managing destroyed
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

  writableBuffer: {
    get() {
      return this._writableState && this._writableState.getBuffer();
    }
  },

  writableEnded: {
    get() {
      return this._writableState ? this._writableState.ending : false;
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

const destroy = destroyImpl.destroy;
Writable.prototype.destroy = function(err, cb) {
  const state = this._writableState;
  if (!state.destroyed) {
    process.nextTick(errorBuffer, state, new ERR_STREAM_DESTROYED('write'));
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

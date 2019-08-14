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

'use strict';

const { Object } = primordials;

module.exports = Readable;

const EE = require('events');
const Stream = require('stream');
const { Buffer } = require('buffer');

const debug = require('internal/util/debuglog').debuglog('stream');
const BufferList = require('internal/streams/buffer_list');
const destroyImpl = require('internal/streams/destroy');
const { getHighWaterMark } = require('internal/streams/state');
const {
  ERR_INVALID_ARG_TYPE,
  ERR_STREAM_PUSH_AFTER_EOF,
  ERR_METHOD_NOT_IMPLEMENTED,
  ERR_STREAM_UNSHIFT_AFTER_END_EVENT
} = require('internal/errors').codes;

// Lazy loaded to improve the startup performance.
let StringDecoder;
let createReadableStreamAsyncIterator;

Object.setPrototypeOf(Readable.prototype, Stream.prototype);
Object.setPrototypeOf(Readable, Stream);

const { errorOrDestroy } = destroyImpl;
const kProxyEvents = ['error', 'close', 'destroy', 'pause', 'resume'];

function prependListener(emitter, event, fn) {
  // Sadly this is not cacheable as some libraries bundle their own
  // event emitter implementation with them.
  if (typeof emitter.prependListener === 'function')
    return emitter.prependListener(event, fn);

  // This is a hack to make sure that our error handler is attached before any
  // userland ones.  NEVER DO THIS. This is here only because this code needs
  // to continue to work with older versions of Node.js that do not include
  // the prependListener() method. The goal is to eventually remove this hack.
  if (!emitter._events || !emitter._events[event])
    emitter.on(event, fn);
  else if (Array.isArray(emitter._events[event]))
    emitter._events[event].unshift(fn);
  else
    emitter._events[event] = [fn, emitter._events[event]];
}

const awaitDrainMask = 0xFFFF;
const objectModeMask = 1 << 16;
const endedMask = 1 << 17;
const endEmittedMask = 1 << 18;
const readingMask = 1 << 19;
const syncMask = 1 << 20;
const needReadableMask = 1 << 21;
const emittedReadableMask = 1 << 22;
const readableListeningMask = 1 << 23;
const resumeScheduledMask = 1 << 24;
const pausedMask = 1 << 25;
const emitCloseMask = 1 << 26;
const autoDestroyMask = 1 << 27;
const destroyedMask = 1 << 28;
const readingMoreMask = 1 << 29;
const flowingNullMask = 1 << 30;
const flowingTrueMask = 1 << 31;

const flowingMask = (flowingTrueMask | flowingNullMask);

const kBitfield = Symbol('kBitfield');

class ReadableState {
  constructor(options, stream, isDuplex) {
    options = options || {};

    // Duplex streams are both readable and writable, but share
    // the same options object.
    // However, some cases require setting options to different
    // values for the readable and the writable sides of the duplex stream.
    // These options can be provided separately as readableXXX and writableXXX.
    if (typeof isDuplex !== 'boolean')
      isDuplex = stream instanceof Stream.Duplex;

    this[kBitfield] =
      syncMask |
      pausedMask |
      flowingNullMask |
      (options.emitClose !== false ? emitCloseMask : 0) |
      (options.autoDestroy ? autoDestroyMask : 0) |
      (options.objectMode || (isDuplex && options.readableObjectMode) ?
        objectModeMask : 0);

    // Object stream flag. Used to make read(n) ignore n and to
    // make all the buffer merging and length checks go away
    // this.objectMode = !!options.objectMode;

    // if (isDuplex)
    //   this.objectMode = this.objectMode || !!options.readableObjectMode;

    // The point at which it stops calling _read() to fill the buffer
    // Note: 0 is a valid value, means "don't call _read preemptively ever"
    this.highWaterMark = getHighWaterMark(this, options,
                                          'readableHighWaterMark', isDuplex);

    // A linked list is used to store data chunks instead of an array because
    // the linked list can remove elements from the beginning faster than
    // array.shift()
    this.buffer = new BufferList();
    this.length = 0;
    this.pipes = [];
    // this.flowing = null;
    // this.ended = false;
    // this.endEmitted = false;
    // this.reading = false;

    // A flag to be able to tell if the event 'readable'/'data' is emitted
    // immediately, or on a later tick.  We set this to true at first, because
    // any actions that shouldn't happen until "later" should generally also
    // not happen before the first read call.
    // this.sync = true;

    // Whenever we return null, then we set a flag to say
    // that we're awaiting a 'readable' event emission.
    // this.needReadable = false;
    // this.emittedReadable = false;
    // this.readableListening = false;
    // this.resumeScheduled = false;
    // this.paused = true;

    // Should close be emitted on destroy. Defaults to true.
    // this.emitClose = options.emitClose !== false;

    // Should .destroy() be called after 'end' (and potentially 'finish')
    // this.autoDestroy = !!options.autoDestroy;

    // Has it been destroyed
    // this.destroyed = false;

    // Crypto is kind of old and crusty.  Historically, its default string
    // encoding is 'binary' so we have to make this configurable.
    // Everything else in the universe uses 'utf8', though.
    if (options.defaultEncoding) {
      this.defaultEncoding = options.defaultEncoding;
    }

    // The number of writers that are awaiting a drain event in .pipe()s
    // this.awaitDrain = 0;

    // If true, a maybeReadMore has been scheduled
    // this.readingMore = false;

    if (options.encoding) {
      if (!StringDecoder)
        StringDecoder = require('string_decoder').StringDecoder;
      this.decoder = new StringDecoder(options.encoding);
      this.encoding = options.encoding || null;
    }
  }

  // Legacy getter for `pipesCount`
  get pipesCount() {
    return this.pipes.length;
  }

  // 0 === false
  // 1 === null
  // 2 === true
  get flowing () {
    const flowing = this[kBitfield] & flowingMask;
    if (!flowing) {
      return false;
    } else if (flowing === flowingNullMask) {
      return null;
    } else {
      return true;
    }
  }
  set flowing (val) {
    this[kBitfield] = this[kBitfield] & ~flowingMask;
    if (val !== false) {
      this[kBitfield] = this[kBitfield] |
        (val ? flowingTrueMask : flowingNullMask);
    }
  }

  get awaitDrain () {
    return this[kBitfield] & awaitDrainMask;
  }
  set awaitDrain (val) {
    this[kBitfield] = (this[kBitfield] & ~awaitDrainMask) |
      (val & awaitDrainMask);
  }

  get objectMode() {
    return Boolean(this[kBitfield] & objectModeMask);
  }
  set objectMode(val) {
    this[kBitfield] = val ?
      this[kBitfield] | objectModeMask :
      this[kBitfield] & ~objectModeMask;
  }

  get ended() {
    return Boolean(this[kBitfield] & endedMask);
  }
  set ended(val) {
    this[kBitfield] = val ?
      this[kBitfield] | endedMask :
      this[kBitfield] & ~endedMask;
  }

  get endEmitted() {
    return Boolean(this[kBitfield] & endEmittedMask);
  }
  set endEmitted(val) {
    this[kBitfield] = val ?
      this[kBitfield] | endEmittedMask :
      this[kBitfield] & ~endEmittedMask;
  }

  get reading() {
    return Boolean(this[kBitfield] & readingMask);
  }
  set reading(val) {
    this[kBitfield] = val ?
      this[kBitfield] | readingMask :
      this[kBitfield] & ~readingMask;
  }

  get sync() {
    return Boolean(this[kBitfield] & syncMask);
  }
  set sync(val) {
    this[kBitfield] = val ?
      this[kBitfield] | syncMask :
      this[kBitfield] & ~syncMask;
  }

  get needReadable() {
    return Boolean(this[kBitfield] & needReadableMask);
  }
  set needReadable(val) {
    this[kBitfield] = val ?
      this[kBitfield] | needReadableMask :
      this[kBitfield] & ~needReadableMask;
  }

  get emittedReadable() {
    return Boolean(this[kBitfield] & emittedReadableMask);
  }
  set emittedReadable(val) {
    this[kBitfield] = val ?
      this[kBitfield] | emittedReadableMask :
      this[kBitfield] & ~emittedReadableMask;
  }

  get readableListening() {
    return Boolean(this[kBitfield] & readableListeningMask);
  }
  set readableListening(val) {
    this[kBitfield] = val ?
      this[kBitfield] | readableListeningMask :
      this[kBitfield] & ~readableListeningMask;
  }

  get resumeScheduled() {
    return Boolean(this[kBitfield] & resumeScheduledMask);
  }
  set resumeScheduled(val) {
    this[kBitfield] = val ?
      this[kBitfield] | resumeScheduledMask :
      this[kBitfield] & ~resumeScheduledMask;
  }

  get paused() {
    return Boolean(this[kBitfield] & pausedMask);
  }
  set paused(val) {
    this[kBitfield] = val ?
      this[kBitfield] | pausedMask :
      this[kBitfield] & ~pausedMask;
  }

  get emitClose() {
    return Boolean(this[kBitfield] & emitCloseMask);
  }
  set emitClose(val) {
    this[kBitfield] = val ?
      this[kBitfield] | emitCloseMask :
      this[kBitfield] & ~emitCloseMask;
  }

  get autoDestroy() {
    return Boolean(this[kBitfield] & autoDestroyMask);
  }
  set autoDestroy(val) {
    this[kBitfield] = val ?
      this[kBitfield] | autoDestroyMask :
      this[kBitfield] & ~autoDestroyMask;
  }

  get destroyed() {
    return Boolean(this[kBitfield] & destroyedMask);
  }
  set destroyed(val) {
    this[kBitfield] = val ?
      this[kBitfield] | destroyedMask :
      this[kBitfield] & ~destroyedMask;
  }

  get readingMore() {
    return Boolean(this[kBitfield] & readingMoreMask);
  }
  set readingMore(val) {
    this[kBitfield] = val ?
      this[kBitfield] | readingMoreMask :
      this[kBitfield] & ~readingMoreMask;
  }
}
Readable.ReadableState = ReadableState;

function Readable(options) {
  if (!(this instanceof Readable))
    return new Readable(options);

  // Checking for a Stream.Duplex instance is faster here instead of inside
  // the ReadableState constructor, at least with V8 6.5
  const isDuplex = this instanceof Stream.Duplex;

  this._readableState = new ReadableState(options, this, isDuplex);

  // legacy
  this.readable = true;

  if (options) {
    if (typeof options.read === 'function')
      this._read = options.read;

    if (typeof options.destroy === 'function')
      this._destroy = options.destroy;
  }

  Stream.call(this);
}

Object.defineProperty(Readable.prototype, 'destroyed', {
  // Making it explicit this property is not enumerable
  // because otherwise some prototype manipulation in
  // userland will fail
  enumerable: false,
  get() {
    if (this._readableState === undefined) {
      return false;
    }
    return this._readableState.destroyed;
  },
  set(value) {
    // We ignore the value if the stream
    // has not been initialized yet
    if (!this._readableState) {
      return;
    }

    // Backward compatibility, the user is explicitly
    // managing destroyed
    this._readableState.destroyed = value;
  }
});

Readable.prototype.destroy = destroyImpl.destroy;
Readable.prototype._undestroy = destroyImpl.undestroy;
Readable.prototype._destroy = function(err, cb) {
  cb(err);
};

// Manually shove something into the read() buffer.
// This returns true if the highWaterMark has not been hit yet,
// similar to how Writable.write() returns true if you should
// write() some more.
Readable.prototype.push = function(chunk, encoding) {
  return readableAddChunk(this, chunk, encoding, false);
};

// Unshift should *always* be something directly out of read()
Readable.prototype.unshift = function(chunk, encoding) {
  return readableAddChunk(this, chunk, encoding, true);
};

function readableAddChunk(stream, chunk, encoding, addToFront) {
  debug('readableAddChunk', chunk);
  const state = stream._readableState;

  let skipChunkCheck;

  const objectMode = state[kBitfield] & objectModeMask;

  if (!objectMode) {
    if (typeof chunk === 'string') {
      encoding = encoding || state.defaultEncoding || 'utf8';
      if (addToFront && state.encoding && state.encoding !== encoding) {
        // When unshifting, if state.encoding is set, we have to save
        // the string in the BufferList with the state encoding
        chunk = Buffer.from(chunk, encoding).toString(state.encoding);
      } else if (encoding !== state.encoding) {
        chunk = Buffer.from(chunk, encoding);
        encoding = '';
      }
      skipChunkCheck = true;
    }
  } else {
    skipChunkCheck = true;
  }

  if (chunk === null) {
    // state.reading = false;
    state[kBitfield] = state[kBitfield] & ~readingMask;
    onEofChunk(stream, state);
  } else {
    var er;
    if (!skipChunkCheck)
      er = chunkInvalid(state, chunk);
    if (er) {
      errorOrDestroy(stream, er);
    } else if (objectMode || chunk && chunk.length > 0) {
      if (typeof chunk !== 'string' &&
          !objectMode &&
          // Do not use Object.getPrototypeOf as it is slower since V8 7.3.
          !(chunk instanceof Buffer)) {
        chunk = Stream._uint8ArrayToBuffer(chunk);
      }

      if (addToFront) {
        if (state[kBitfield] & endEmittedMask)
          errorOrDestroy(stream, new ERR_STREAM_UNSHIFT_AFTER_END_EVENT());
        else
          addChunk(stream, state, chunk, true);
      } else if (state[kBitfield] & endedMask) {
        errorOrDestroy(stream, new ERR_STREAM_PUSH_AFTER_EOF());
      } else if (state[kBitfield] & destroyedMask) {
        return false;
      } else {
        // state.reading = false;
        state[kBitfield] = state[kBitfield] & ~readingMask;
        if (state.decoder && !encoding) {
          chunk = state.decoder.write(chunk);
          if (objectMode || chunk.length !== 0)
            addChunk(stream, state, chunk, false);
          else
            maybeReadMore(stream, state);
        } else {
          addChunk(stream, state, chunk, false);
        }
      }
    } else if (!addToFront) {
      // state.reading = false;
      state[kBitfield] = state[kBitfield] & ~readingMask;
      maybeReadMore(stream, state);
    }
  }

  // We can push more data if we are below the highWaterMark.
  // Also, if we have no data yet, we can stand some more bytes.
  // This is to work around cases where hwm=0, such as the repl.
  return !(state[kBitfield] & endedMask) &&
    (state.length < state.highWaterMark || state.length === 0);
}

function addChunk(stream, state, chunk, addToFront) {
  if ((state[kBitfield] & flowingTrueMask) && state.length === 0 &&
    !(state[kBitfield] & syncMask)) {
    // state.awaitDrain = 0;
    state[kBitfield] = state[kBitfield] & ~awaitDrainMask;
    stream.emit('data', chunk);
  } else {
    // Update the buffer info.
    state.length += (state[kBitfield] & objectModeMask) ? 1 : chunk.length;
    if (addToFront)
      state.buffer.unshift(chunk);
    else
      state.buffer.push(chunk);

    if (state[kBitfield] & needReadableMask)
      emitReadable(stream);
  }
  maybeReadMore(stream, state);
}

function chunkInvalid(state, chunk) {
  if (!Stream._isUint8Array(chunk) &&
      typeof chunk !== 'string' &&
      chunk !== undefined &&
      !(state[kBitfield] & objectModeMask)) {
    return new ERR_INVALID_ARG_TYPE(
      'chunk', ['string', 'Buffer', 'Uint8Array'], chunk);
  }
}


Readable.prototype.isPaused = function() {
  return !(this._readableState[kBitfield] & flowingMask);
};

// Backwards compatibility.
Readable.prototype.setEncoding = function(enc) {
  if (!StringDecoder)
    StringDecoder = require('string_decoder').StringDecoder;
  const decoder = new StringDecoder(enc);
  this._readableState.decoder = decoder;
  // If setEncoding(null), decoder.encoding equals utf8
  this._readableState.encoding = this._readableState.decoder.encoding;

  const buffer = this._readableState.buffer;
  // Iterate over current buffer to convert already stored Buffers:
  let content = '';
  for (const data of buffer) {
    content += decoder.write(data);
  }
  buffer.clear();
  if (content !== '')
    buffer.push(content);
  this._readableState.length = content.length;
  return this;
};

// Don't raise the hwm > 8MB
const MAX_HWM = 0x800000;
function computeNewHighWaterMark(n) {
  if (n >= MAX_HWM) {
    n = MAX_HWM;
  } else {
    // Get the next highest power of 2 to prevent increasing hwm excessively in
    // tiny amounts
    n--;
    n |= n >>> 1;
    n |= n >>> 2;
    n |= n >>> 4;
    n |= n >>> 8;
    n |= n >>> 16;
    n++;
  }
  return n;
}

// This function is designed to be inlinable, so please take care when making
// changes to the function body.
function howMuchToRead(n, state) {
  if (n <= 0 || (state.length === 0 && (state[kBitfield] & endedMask)))
    return 0;
  if (state[kBitfield] & objectModeMask)
    return 1;
  if (Number.isNaN(n)) {
    // Only flow one buffer at a time
    if ((state[kBitfield] & flowingTrueMask) && state.length)
      return state.buffer.first().length;
    else
      return state.length;
  }
  // If we're asking for more than the current hwm, then raise the hwm.
  if (n > state.highWaterMark)
    state.highWaterMark = computeNewHighWaterMark(n);
  if (n <= state.length)
    return n;
  // Don't have enough
  if (!(state[kBitfield] & endedMask)) {
    // state.needReadable = true;
    state[kBitfield] = state[kBitfield] | needReadableMask;
    return 0;
  }
  return state.length;
}

// You can override either this method, or the async _read(n) below.
Readable.prototype.read = function(n) {
  debug('read', n);
  // Same as parseInt(undefined, 10), however V8 7.3 performance regressed
  // in this scenario, so we are doing it manually.
  if (n === undefined) {
    n = NaN;
  } else if (!Number.isInteger(n)) {
    n = parseInt(n, 10);
  }
  const state = this._readableState;

  const nOrig = n;

  if (n !== 0) {
    // state.emittedReadable = false;
    state[kBitfield] = state[kBitfield] & ~emittedReadableMask;
  }

  // If we're doing read(0) to trigger a readable event, but we
  // already have a bunch of data in the buffer, then just trigger
  // the 'readable' event and move on.
  if (n === 0 &&
      (state[kBitfield] & needReadableMask) &&
      ((state.highWaterMark !== 0 ?
        state.length >= state.highWaterMark :
        state.length > 0) ||
       (state[kBitfield] & endedMask))) {
    debug('read: emitReadable', state.length, state.ended);
    if (state.length === 0 && (state[kBitfield] & endedMask))
      endReadable(this);
    else
      emitReadable(this);
    return null;
  }

  n = howMuchToRead(n, state);

  // If we've ended, and we're now clear, then finish it up.
  if (n === 0 && (state[kBitfield] & endedMask)) {
    if (state.length === 0)
      endReadable(this);
    return null;
  }

  // All the actual chunk generation logic needs to be
  // *below* the call to _read.  The reason is that in certain
  // synthetic stream cases, such as passthrough streams, _read
  // may be a completely synchronous operation which may change
  // the state of the read buffer, providing enough data when
  // before there was *not* enough.
  //
  // So, the steps are:
  // 1. Figure out what the state of things will be after we do
  // a read from the buffer.
  //
  // 2. If that resulting state will trigger a _read, then call _read.
  // Note that this may be asynchronous, or synchronous.  Yes, it is
  // deeply ugly to write APIs this way, but that still doesn't mean
  // that the Readable class should behave improperly, as streams are
  // designed to be sync/async agnostic.
  // Take note if the _read call is sync or async (ie, if the read call
  // has returned yet), so that we know whether or not it's safe to emit
  // 'readable' etc.
  //
  // 3. Actually pull the requested chunks out of the buffer and return.

  // if we need a readable event, then we need to do some reading.
  let doRead = state[kBitfield] & needReadableMask;
  debug('need readable', doRead);

  // If we currently have less than the highWaterMark, then also read some
  if (state.length === 0 || state.length - n < state.highWaterMark) {
    doRead = true;
    debug('length less than watermark', doRead);
  }

  // However, if we've ended, then there's no point, and if we're already
  // reading, then it's unnecessary.
  if (state[kBitfield] & (endedMask | readingMask)) {
    doRead = false;
    debug('reading or ended', doRead);
  } else if (doRead) {
    debug('do read');
    // state.reading = true;
    // state.sync = true;
    // If the length is currently zero, then we *need* a readable event.
    // if (state.length === 0)
    //   state.needReadable = true;
    state[kBitfield] = state[kBitfield] |
      readingMask | syncMask |
      (state.length === 0 ? needReadableMask : 0);
    // Call internal read method
    this._read(state.highWaterMark);

    // state.sync = false;
    state[kBitfield] = state[kBitfield] & ~syncMask;

    // If _read pushed data synchronously, then `reading` will be false,
    // and we need to re-evaluate how much data we can return to the user.
    if (!(state[kBitfield] & readingMask)) {
      n = howMuchToRead(nOrig, state);
    }
  }

  var ret;
  if (n > 0)
    ret = fromList(n, state);
  else
    ret = null;

  if (ret === null) {
    // state.needReadable = state.length <= state.highWaterMark;
    state[kBitfield] = (state[kBitfield] & ~needReadableMask) |
      (state.length <= state.highWaterMark ? needReadableMask : 0);
    n = 0;
  } else {
    state.length -= n;
    // state.awaitDrain = 0;
    state[kBitfield] = state[kBitfield] & ~awaitDrainMask;
  }

  if (state.length === 0) {
    // If we have nothing in the buffer, then we want to know
    // as soon as we *do* get something into the buffer.
    if (!(state[kBitfield] & endedMask))
      state[kBitfield] = state[kBitfield] | needReadableMask;
    // If we tried to read() past the EOF, then emit end on the next tick.
    else if (nOrig !== n) {
      endReadable(this);
    }
  }

  if (ret !== null)
    this.emit('data', ret);

  return ret;
};

function onEofChunk(stream, state) {
  debug('onEofChunk');

  if (state[kBitfield] & endedMask) return;
  if (state.decoder) {
    var chunk = state.decoder.end();
    if (chunk && chunk.length) {
      state.buffer.push(chunk);
      state.length += (state[kBitfield] & objectModeMask) ? 1 : chunk.length;
    }
  }

  // state.ended = true;
  state[kBitfield] = state[kBitfield] | endedMask;

  if (state[kBitfield] & syncMask) {
    // If we are sync, wait until next tick to emit the data.
    // Otherwise we risk emitting data in the flow()
    // the readable code triggers during a read() call
    emitReadable(stream);
  } else {
    // Emit 'readable' now to make sure it gets picked up.
    // state.needReadable = false;
    // state.emittedReadable = true;
    state[kBitfield] = (state[kBitfield] & ~needReadableMask)
      | emittedReadableMask;
    // We have to emit readable now that we are EOF. Modules
    // in the ecosystem (e.g. dicer) rely on this event being sync.
    emitReadable_(stream);
  }
}

// Don't emit readable right away in sync mode, because this can trigger
// another read() call => stack overflow.  This way, it might trigger
// a nextTick recursion warning, but that's not so bad.
function emitReadable(stream) {
  const state = stream._readableState;
  debug('emitReadable', state.needReadable, state.emittedReadable);
  // state.needReadable = false;
  state[kBitfield] = state[kBitfield] & ~needReadableMask;
  if (!(state[kBitfield] & emittedReadableMask)) {
    debug('emitReadable', state.flowing);
    // state.emittedReadable = true;
    state[kBitfield] = state[kBitfield] | emittedReadableMask;
    process.nextTick(emitReadable_, stream);
  }
}

function emitReadable_(stream) {
  const state = stream._readableState;

  const destroyed = state[kBitfield] & destroyedMask;
  const ended = state[kBitfield] & endedMask;
  const flowing = state[kBitfield] & flowingTrueMask;

  debug('emitReadable_', state.destroyed, state.length, state.ended);
  if (!destroyed && (ended || state.length)) {
    stream.emit('readable');
    // state.emittedReadable = false;
    state[kBitfield] = state[kBitfield] & ~emittedReadableMask;
  }

  // The stream needs another readable event if
  // 1. It is not flowing, as the flow mechanism will take
  //    care of it.
  // 2. It is not ended.
  // 3. It is below the highWaterMark, so we can schedule
  //    another readable later.
  const needReadable = !flowing && !ended &&
    state.length <= state.highWaterMark;
  // state.emittedReadable = needReadable;
  state[kBitfield] = state[kBitfield] | (needReadable ? needReadableMask : 0);
  flow(stream);
}


// At this point, the user has presumably seen the 'readable' event,
// and called read() to consume some data.  that may have triggered
// in turn another _read(n) call, in which case reading = true if
// it's in progress.
// However, if we're not ended, or reading, and the length < hwm,
// then go ahead and try to read some more preemptively.
function maybeReadMore(stream, state) {
  if (!(state[kBitfield] & readingMoreMask)) {
    // state.readingMore = readingMore;
    state[kBitfield] = state[kBitfield] | readingMoreMask;
    process.nextTick(maybeReadMore_, stream, state);
  }
}

function maybeReadMore_(stream, state) {
  // Attempt to read more data if we should.
  //
  // The conditions for reading more data are (one of):
  // - Not enough data buffered (state.length < state.highWaterMark). The loop
  //   is responsible for filling the buffer with enough data if such data
  //   is available. If highWaterMark is 0 and we are not in the flowing mode
  //   we should _not_ attempt to buffer any extra data. We'll get more data
  //   when the stream consumer calls read() instead.
  // - No data in the buffer, and the stream is in flowing mode. In this mode
  //   the loop below is responsible for ensuring read() is called. Failing to
  //   call read here would abort the flow and there's no other mechanism for
  //   continuing the flow if the stream consumer has just subscribed to the
  //   'data' event.
  //
  // In addition to the above conditions to keep reading data, the following
  // conditions prevent the data from being read:
  // - The stream has ended (state.ended).
  // - There is already a pending 'read' operation (state.reading). This is a
  //   case where the the stream has called the implementation defined _read()
  //   method, but they are processing the call asynchronously and have _not_
  //   called push() with new data. In this case we skip performing more
  //   read()s. The execution ends in this method again after the _read() ends
  //   up calling push() with more data.
  while (!(state[kBitfield] & (readingMask | endedMask)) &&
         (state.length < state.highWaterMark ||
          ((state[kBitfield] & flowingTrueMask) && state.length === 0))) {
    const len = state.length;
    debug('maybeReadMore read 0');
    stream.read(0);
    if (len === state.length)
      // Didn't get any data, stop spinning.
      break;
  }
  // state.readingMore = false;
  state[kBitfield] = state[kBitfield] & ~readingMoreMask;
}

// Abstract method.  to be overridden in specific implementation classes.
// call cb(er, data) where data is <= n in length.
// for virtual (non-string, non-buffer) streams, "length" is somewhat
// arbitrary, and perhaps not very meaningful.
Readable.prototype._read = function(n) {
  errorOrDestroy(this, new ERR_METHOD_NOT_IMPLEMENTED('_read()'));
};

Readable.prototype.pipe = function(dest, pipeOpts) {
  const src = this;
  const state = this._readableState;

  state.pipes.push(dest);
  debug('pipe count=%d opts=%j', state.pipes.length, pipeOpts);

  const doEnd = (!pipeOpts || pipeOpts.end !== false) &&
              dest !== process.stdout &&
              dest !== process.stderr;

  const endFn = doEnd ? onend : unpipe;
  if (state[kBitfield] & endEmittedMask)
    process.nextTick(endFn);
  else
    src.once('end', endFn);

  dest.on('unpipe', onunpipe);
  function onunpipe(readable, unpipeInfo) {
    debug('onunpipe');
    if (readable === src) {
      if (unpipeInfo && unpipeInfo.hasUnpiped === false) {
        unpipeInfo.hasUnpiped = true;
        cleanup();
      }
    }
  }

  function onend() {
    debug('onend');
    dest.end();
  }

  // When the dest drains, it reduces the awaitDrain counter
  // on the source.  This would be more elegant with a .once()
  // handler in flow(), but adding and removing repeatedly is
  // too slow.
  const ondrain = pipeOnDrain(src);
  dest.on('drain', ondrain);

  var cleanedUp = false;
  function cleanup() {
    debug('cleanup');
    // Cleanup event handlers once the pipe is broken
    dest.removeListener('close', onclose);
    dest.removeListener('finish', onfinish);
    dest.removeListener('drain', ondrain);
    dest.removeListener('error', onerror);
    dest.removeListener('unpipe', onunpipe);
    src.removeListener('end', onend);
    src.removeListener('end', unpipe);
    src.removeListener('data', ondata);

    cleanedUp = true;

    // If the reader is waiting for a drain event from this
    // specific writer, then it would cause it to never start
    // flowing again.
    // So, if this is awaiting a drain, then we just call it now.
    // If we don't know, then assume that we are waiting for one.
    if ((state[kBitfield] & awaitDrainMask) &&
        (!dest._writableState || dest._writableState.needDrain))
      ondrain();
  }

  src.on('data', ondata);
  function ondata(chunk) {
    debug('ondata');
    const ret = dest.write(chunk);
    debug('dest.write', ret);
    if (ret === false) {
      // If the user unpiped during `dest.write()`, it is possible
      // to get stuck in a permanently paused state if that write
      // also returned false.
      // => Check whether `dest` is still a piping destination.
      if (state.pipes.length > 0 && state.pipes.includes(dest) && !cleanedUp) {
        debug('false write response, pause', state.awaitDrain);

        // state.awaitDrain++;
        state[kBitfield]++;
      }
      src.pause();
    }
  }

  // If the dest has an error, then stop piping into it.
  // However, don't suppress the throwing behavior for this.
  function onerror(er) {
    debug('onerror', er);
    unpipe();
    dest.removeListener('error', onerror);
    if (EE.listenerCount(dest, 'error') === 0)
      errorOrDestroy(dest, er);
  }

  // Make sure our error handler is attached before userland ones.
  prependListener(dest, 'error', onerror);

  // Both close and finish should trigger unpipe, but only once.
  function onclose() {
    dest.removeListener('finish', onfinish);
    unpipe();
  }
  dest.once('close', onclose);
  function onfinish() {
    debug('onfinish');
    dest.removeListener('close', onclose);
    unpipe();
  }
  dest.once('finish', onfinish);

  function unpipe() {
    debug('unpipe');
    src.unpipe(dest);
  }

  // Tell the dest that it's being piped to
  dest.emit('pipe', src);

  // Start the flow if it hasn't been started already.
  if (!(state[kBitfield] & flowingTrueMask)) {
    debug('pipe resume');
    src.resume();
  }

  return dest;
};

function pipeOnDrain(src) {
  return function pipeOnDrainFunctionResult() {
    const state = src._readableState;

    debug('pipeOnDrain', state.awaitDrain);
    if (state[kBitfield] & awaitDrainMask) {
      // state.awaitDrain--;
      state[kBitfield]--;
    }

    if (!(state[kBitfield] & awaitDrainMask) && EE.listenerCount(src, 'data')) {
      // state.flowing = true;
      state[kBitfield] = state[kBitfield] | flowingTrueMask;
      flow(src);
    }
  };
}


Readable.prototype.unpipe = function(dest) {
  const state = this._readableState;
  const unpipeInfo = { hasUnpiped: false };

  // If we're not piping anywhere, then do nothing.
  if (state.pipes.length === 0)
    return this;

  if (!dest) {
    // remove all.
    var dests = state.pipes;
    state.pipes = [];
    state[kBitfield] = state[kBitfield] & ~flowingMask;

    for (var i = 0; i < dests.length; i++)
      dests[i].emit('unpipe', this, { hasUnpiped: false });
    return this;
  }

  // Try to find the right one.
  const index = state.pipes.indexOf(dest);
  if (index === -1)
    return this;

  state.pipes.splice(index, 1);
  if (state.pipes.length === 0)
    state[kBitfield] = state[kBitfield] & ~flowingMask;

  dest.emit('unpipe', this, unpipeInfo);

  return this;
};

// Set up data events if they are asked for
// Ensure readable listeners eventually get something
Readable.prototype.on = function(ev, fn) {
  const res = Stream.prototype.on.call(this, ev, fn);
  const state = this._readableState;

  if (ev === 'data') {
    // Update readableListening so that resume() may be a no-op
    // a few lines down. This is needed to support once('readable').
    // state.readableListening = this.listenerCount('readable') > 0;
    state[kBitfield] = (state[kBitfield] & ~readableListeningMask) |
      (this.listenerCount('readable') > 0 ? readableListeningMask : 0);

    // Try start flowing on next tick if stream isn't explicitly paused
    if (state[kBitfield] & flowingMask)
      this.resume();
  } else if (ev === 'readable') {
    if (!(state[kBitfield] & (endEmittedMask | readableListeningMask))) {
      // state.readableListening = state.needReadable = true;
      // state.flowing = false;
      // state.emittedReadable = false;
      state[kBitfield] = (state[kBitfield] &
        ~(emittedReadableMask | flowingMask)) |
        readableListeningMask | needReadableMask;
      debug('on readable', state.length, state.reading);
      if (state.length) {
        emitReadable(this);
      } else if (!(state[kBitfield] & readingMask)) {
        process.nextTick(nReadingNextTick, this);
      }
    }
  }

  return res;
};
Readable.prototype.addListener = Readable.prototype.on;

Readable.prototype.removeListener = function(ev, fn) {
  const res = Stream.prototype.removeListener.call(this, ev, fn);

  if (ev === 'readable') {
    // We need to check if there is someone still listening to
    // readable and reset the state. However this needs to happen
    // after readable has been emitted but before I/O (nextTick) to
    // support once('readable', fn) cycles. This means that calling
    // resume within the same tick will have no
    // effect.
    process.nextTick(updateReadableListening, this);
  }

  return res;
};

Readable.prototype.removeAllListeners = function(ev) {
  const res = Stream.prototype.removeAllListeners.apply(this, arguments);

  if (ev === 'readable' || ev === undefined) {
    // We need to check if there is someone still listening to
    // readable and reset the state. However this needs to happen
    // after readable has been emitted but before I/O (nextTick) to
    // support once('readable', fn) cycles. This means that calling
    // resume within the same tick will have no
    // effect.
    process.nextTick(updateReadableListening, this);
  }

  return res;
};

function updateReadableListening(self) {
  const state = self._readableState;

  // state.readableListening = self.listenerCount('readable') > 0;
  state[kBitfield] = (state[kBitfield] & ~readableListeningMask) |
    (self.listenerCount('readable') > 0 ? readableListeningMask : 0);

  if ((state[kBitfield] & resumeScheduledMask) &&
    !(state[kBitfield] & pausedMask)) {
    // Flowing needs to be set to true now, otherwise
    // the upcoming resume will not flow.
    // state.flowing = true;
    state[kBitfield] = state[kBitfield] | flowingTrueMask;

    // Crude way to check if we should resume
  } else if (self.listenerCount('data') > 0) {
    self.resume();
  }
}

function nReadingNextTick(self) {
  debug('readable nexttick read 0');
  self.read(0);
}

// pause() and resume() are remnants of the legacy readable stream API
// If the user uses them, then switch into old mode.
Readable.prototype.resume = function() {
  const state = this._readableState;

  if (!(state[kBitfield] & flowingTrueMask)) {
    debug('resume');
    // We flow only if there is no one listening
    // for readable, but we still have to call
    // resume()
    const readableListening = state[kBitfield] & readableListeningMask;
    // state.flowing = !readableListening;
    state[kBitfield] = (state[kBitfield] & ~flowingMask) |
      (!readableListening ? flowingTrueMask : 0);
    resume(this, state);
  }
  // state.paused = false;
  state[kBitfield] = state[kBitfield] & ~pausedMask;
  return this;
};

function resume(stream, state) {
  if (!(state[kBitfield] & resumeScheduledMask)) {
    // state.resumeScheduled = true;
    state[kBitfield] = state[kBitfield] | resumeScheduledMask;
    process.nextTick(resume_, stream, state);
  }
}

function resume_(stream, state) {
  debug('resume', state.reading);
  if (!(state[kBitfield] & readingMask)) {
    stream.read(0);
  }

  // state.resumeScheduled = false;
  state[kBitfield] = state[kBitfield] & ~resumeScheduledMask;
  stream.emit('resume');
  flow(stream);
  if ((state[kBitfield] & flowingTrueMask) &&
    !(state[kBitfield] & readingMask))
    stream.read(0);
}

Readable.prototype.pause = function() {
  const state = this._readableState;
  debug('call pause flowing=%j', state.flowing);
  if (state[kBitfield] & flowingMask) {
    debug('pause');
    // state.flowing = false;
    state[kBitfield] = state[kBitfield] & ~flowingMask;
    this.emit('pause');
  }
  // state.paused = true;
  state[kBitfield] = state[kBitfield] | pausedMask;
  return this;
};

function flow(stream) {
  const state = stream._readableState;
  debug('flow', state.flowing);
  while ((state[kBitfield] & flowingTrueMask) && stream.read() !== null);
}

// Wrap an old-style stream as the async data source.
// This is *not* part of the readable stream interface.
// It is an ugly unfortunate mess of history.
Readable.prototype.wrap = function(stream) {
  const state = this._readableState;
  var paused = false;

  stream.on('end', () => {
    debug('wrapped end');
    if (state.decoder && !(state[kBitfield] & endedMask)) {
      var chunk = state.decoder.end();
      if (chunk && chunk.length)
        this.push(chunk);
    }

    this.push(null);
  });

  stream.on('data', (chunk) => {
    debug('wrapped data');
    if (state.decoder)
      chunk = state.decoder.write(chunk);

    const objectMode = state[kBitfield] & objectModeMask;
    // Don't skip over falsy values in objectMode
    if (objectMode && (chunk === null || chunk === undefined))
      return;
    else if (!objectMode && (!chunk || !chunk.length))
      return;

    const ret = this.push(chunk);
    if (!ret) {
      paused = true;
      stream.pause();
    }
  });

  // Proxy all the other methods. Important when wrapping filters and duplexes.
  for (const i in stream) {
    if (this[i] === undefined && typeof stream[i] === 'function') {
      this[i] = function methodWrap(method) {
        return function methodWrapReturnFunction() {
          return stream[method].apply(stream, arguments);
        };
      }(i);
    }
  }

  // Proxy certain important events.
  for (var n = 0; n < kProxyEvents.length; n++) {
    stream.on(kProxyEvents[n], this.emit.bind(this, kProxyEvents[n]));
  }

  // When we try to consume some more bytes, simply unpause the
  // underlying stream.
  this._read = (n) => {
    debug('wrapped _read', n);
    if (paused) {
      paused = false;
      stream.resume();
    }
  };

  return this;
};

Readable.prototype[Symbol.asyncIterator] = function() {
  if (createReadableStreamAsyncIterator === undefined) {
    createReadableStreamAsyncIterator =
      require('internal/streams/async_iterator');
  }
  return createReadableStreamAsyncIterator(this);
};

Object.defineProperty(Readable.prototype, 'readableHighWaterMark', {
  // Making it explicit this property is not enumerable
  // because otherwise some prototype manipulation in
  // userland will fail
  enumerable: false,
  get: function() {
    return this._readableState.highWaterMark;
  }
});

Object.defineProperty(Readable.prototype, 'readableBuffer', {
  // Making it explicit this property is not enumerable
  // because otherwise some prototype manipulation in
  // userland will fail
  enumerable: false,
  get: function() {
    return this._readableState && this._readableState.buffer;
  }
});

Object.defineProperty(Readable.prototype, 'readableFlowing', {
  // Making it explicit this property is not enumerable
  // because otherwise some prototype manipulation in
  // userland will fail
  enumerable: false,
  get: function() {
    return this._readableState.flowing;
  },
  set: function(state) {
    if (this._readableState) {
      this._readableState.flowing = state;
    }
  }
});

// Exposed for testing purposes only.
Readable._fromList = fromList;

Object.defineProperty(Readable.prototype, 'readableLength', {
  // Making it explicit this property is not enumerable
  // because otherwise some prototype manipulation in
  // userland will fail
  enumerable: false,
  get() {
    return this._readableState.length;
  }
});

Object.defineProperty(Readable.prototype, 'readableObjectMode', {
  enumerable: false,
  get() {
    return this._readableState ? this._readableState.objectMode : false;
  }
});

Object.defineProperty(Readable.prototype, 'readableEncoding', {
  enumerable: false,
  get() {
    return this._readableState ? this._readableState.encoding : null;
  }
});

// Pluck off n bytes from an array of buffers.
// Length is the combined lengths of all the buffers in the list.
// This function is designed to be inlinable, so please take care when making
// changes to the function body.
function fromList(n, state) {
  // nothing buffered
  if (state.length === 0)
    return null;

  var ret;
  if (state[kBitfield] & objectModeMask)
    ret = state.buffer.shift();
  else if (!n || n >= state.length) {
    // Read it all, truncate the list
    if (state.decoder)
      ret = state.buffer.join('');
    else if (state.buffer.length === 1)
      ret = state.buffer.first();
    else
      ret = state.buffer.concat(state.length);
    state.buffer.clear();
  } else {
    // read part of list
    ret = state.buffer.consume(n, state.decoder);
  }

  return ret;
}

function endReadable(stream) {
  const state = stream._readableState;

  debug('endReadable', state.endEmitted);
  if (!(state[kBitfield] & endEmittedMask)) {
    // state.ended = true;
    state[kBitfield] = state[kBitfield] | endedMask;
    process.nextTick(endReadableNT, state, stream);
  }
}

function endReadableNT(state, stream) {
  debug('endReadableNT', state.endEmitted, state.length);

  // Check that we didn't get one last unshift.
  if (!(state[kBitfield] & endEmittedMask) && state.length === 0) {
    // state.endEmitted = true;
    state[kBitfield] = state[kBitfield] | endEmittedMask;
    stream.readable = false;
    stream.emit('end');

    if (state[kBitfield] & autoDestroyMask) {
      // In case of duplex streams we need a way to detect
      // if the writable side is ready for autoDestroy as well
      const wState = stream._writableState;
      if (!wState || (wState.autoDestroy && wState.finished)) {
        stream.destroy();
      }
    }
  }
}

Readable.from = function(iterable, opts) {
  let iterator;
  if (iterable && iterable[Symbol.asyncIterator])
    iterator = iterable[Symbol.asyncIterator]();
  else if (iterable && iterable[Symbol.iterator])
    iterator = iterable[Symbol.iterator]();
  else
    throw new ERR_INVALID_ARG_TYPE('iterable', ['Iterable'], iterable);

  const readable = new Readable({
    objectMode: true,
    ...opts
  });
  // Reading boolean to protect against _read
  // being called before last iteration completion.
  let reading = false;
  readable._read = function() {
    if (!reading) {
      reading = true;
      next();
    }
  };
  async function next() {
    try {
      const { value, done } = await iterator.next();
      if (done) {
        readable.push(null);
      } else if (readable.push(await value)) {
        next();
      } else {
        reading = false;
      }
    } catch (err) {
      readable.destroy(err);
    }
  }
  return readable;
};

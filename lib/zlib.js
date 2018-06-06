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

const {
  ERR_BUFFER_TOO_LARGE,
  ERR_INVALID_ARG_TYPE,
  ERR_OUT_OF_RANGE,
  ERR_ZLIB_INITIALIZATION_FAILED
} = require('internal/errors').codes;
const Transform = require('_stream_transform');
const {
  _extend,
  inherits,
  types: {
    isAnyArrayBuffer,
    isArrayBufferView
  }
} = require('util');
const binding = process.binding('zlib');
const assert = require('assert').ok;
const {
  Buffer,
  kMaxLength
} = require('buffer');

const constants = process.binding('constants').zlib;
const {
  Z_NO_FLUSH, Z_BLOCK, Z_PARTIAL_FLUSH, Z_SYNC_FLUSH, Z_FULL_FLUSH, Z_FINISH,
  Z_MIN_CHUNK, Z_MIN_WINDOWBITS, Z_MAX_WINDOWBITS, Z_MIN_LEVEL, Z_MAX_LEVEL,
  Z_MIN_MEMLEVEL, Z_MAX_MEMLEVEL, Z_DEFAULT_CHUNK, Z_DEFAULT_COMPRESSION,
  Z_DEFAULT_STRATEGY, Z_DEFAULT_WINDOWBITS, Z_DEFAULT_MEMLEVEL, Z_FIXED,
  DEFLATE, DEFLATERAW, INFLATE, INFLATERAW, GZIP, GUNZIP, UNZIP
} = constants;

// translation table for return codes.
const codes = {
  Z_OK: constants.Z_OK,
  Z_STREAM_END: constants.Z_STREAM_END,
  Z_NEED_DICT: constants.Z_NEED_DICT,
  Z_ERRNO: constants.Z_ERRNO,
  Z_STREAM_ERROR: constants.Z_STREAM_ERROR,
  Z_DATA_ERROR: constants.Z_DATA_ERROR,
  Z_MEM_ERROR: constants.Z_MEM_ERROR,
  Z_BUF_ERROR: constants.Z_BUF_ERROR,
  Z_VERSION_ERROR: constants.Z_VERSION_ERROR
};

const ckeys = Object.keys(codes);
for (var ck = 0; ck < ckeys.length; ck++) {
  var ckey = ckeys[ck];
  codes[codes[ckey]] = ckey;
}

function zlibBuffer(engine, buffer, callback) {
  // Streams do not support non-Buffer ArrayBufferViews yet. Convert it to a
  // Buffer without copying.
  if (isArrayBufferView(buffer) &&
      Object.getPrototypeOf(buffer) !== Buffer.prototype) {
    buffer = Buffer.from(buffer.buffer, buffer.byteOffset, buffer.byteLength);
  } else if (isAnyArrayBuffer(buffer)) {
    buffer = Buffer.from(buffer);
  }
  engine.buffers = null;
  engine.nread = 0;
  engine.cb = callback;
  engine.on('data', zlibBufferOnData);
  engine.on('error', zlibBufferOnError);
  engine.on('end', zlibBufferOnEnd);
  engine.end(buffer);
}

function zlibBufferOnData(chunk) {
  if (!this.buffers)
    this.buffers = [chunk];
  else
    this.buffers.push(chunk);
  this.nread += chunk.length;
}

function zlibBufferOnError(err) {
  this.removeAllListeners('end');
  this.cb(err);
}

function zlibBufferOnEnd() {
  var buf;
  var err;
  if (this.nread >= kMaxLength) {
    err = new ERR_BUFFER_TOO_LARGE();
  } else if (this.nread === 0) {
    buf = Buffer.alloc(0);
  } else {
    var bufs = this.buffers;
    buf = (bufs.length === 1 ? bufs[0] : Buffer.concat(bufs, this.nread));
  }
  this.close();
  if (err)
    this.cb(err);
  else if (this._info)
    this.cb(null, { buffer: buf, engine: this });
  else
    this.cb(null, buf);
}

function zlibBufferSync(engine, buffer) {
  if (typeof buffer === 'string') {
    buffer = Buffer.from(buffer);
  } else if (!isArrayBufferView(buffer)) {
    if (isAnyArrayBuffer(buffer)) {
      buffer = Buffer.from(buffer);
    } else {
      throw new ERR_INVALID_ARG_TYPE(
        'buffer',
        ['string', 'Buffer', 'TypedArray', 'DataView', 'ArrayBuffer'],
        buffer
      );
    }
  }
  buffer = processChunkSync(engine, buffer, engine._finishFlushFlag);
  if (engine._info)
    return { buffer, engine };
  return buffer;
}

function zlibOnError(message, errno) {
  var self = this.jsref;
  // there is no way to cleanly recover.
  // continuing only obscures problems.
  _close(self);
  self._hadError = true;

  // eslint-disable-next-line no-restricted-syntax
  const error = new Error(message);
  error.errno = errno;
  error.code = codes[errno];
  self.emit('error', error);
}

function flushCallback(level, strategy, callback) {
  if (!this._handle)
    assert(false, 'zlib binding closed');
  this._handle.params(level, strategy);
  if (!this._hadError) {
    this._level = level;
    this._strategy = strategy;
    if (callback) callback();
  }
}

// 1. Returns false for undefined and NaN
// 2. Returns true for finite numbers
// 3. Throws ERR_INVALID_ARG_TYPE for non-numbers
// 4. Throws ERR_OUT_OF_RANGE for infinite numbers
function checkFiniteNumber(number, name) {
  // Common case
  if (number === undefined) {
    return false;
  }

  if (Number.isFinite(number)) {
    return true; // Is a valid number
  }

  if (Number.isNaN(number)) {
    return false;
  }

  // Other non-numbers
  if (typeof number !== 'number') {
    const err = new ERR_INVALID_ARG_TYPE(name, 'number', number);
    Error.captureStackTrace(err, checkFiniteNumber);
    throw err;
  }

  // Infinite numbers
  const err = new ERR_OUT_OF_RANGE(name, 'a finite number', number);
  Error.captureStackTrace(err, checkFiniteNumber);
  throw err;
}

// 1. Returns def for undefined and NaN
// 2. Returns number for finite numbers >= lower and <= upper
// 3. Throws ERR_INVALID_ARG_TYPE for non-numbers
// 4. Throws ERR_OUT_OF_RANGE for infinite numbers or numbers > upper or < lower
function checkRangesOrGetDefault(number, name, lower, upper, def) {
  if (!checkFiniteNumber(number, name, lower, upper)) {
    return def;
  }
  if (number < lower || number > upper) {
    const err = new ERR_OUT_OF_RANGE(name,
                                     `>= ${lower} and <= ${upper}`, number);
    Error.captureStackTrace(err, checkRangesOrGetDefault);
    throw err;
  }
  return number;
}

// the Zlib class they all inherit from
// This thing manages the queue of requests, and returns
// true or false if there is anything in the queue when
// you call the .write() method.
function Zlib(opts, mode) {
  var chunkSize = Z_DEFAULT_CHUNK;
  var flush = Z_NO_FLUSH;
  var finishFlush = Z_FINISH;
  var windowBits = Z_DEFAULT_WINDOWBITS;
  var level = Z_DEFAULT_COMPRESSION;
  var memLevel = Z_DEFAULT_MEMLEVEL;
  var strategy = Z_DEFAULT_STRATEGY;
  var dictionary;

  // The Zlib class is not exported to user land, the mode should only be
  // passed in by us.
  assert(typeof mode === 'number');
  assert(mode >= DEFLATE && mode <= UNZIP);

  if (opts) {
    chunkSize = opts.chunkSize;
    if (!checkFiniteNumber(chunkSize, 'options.chunkSize')) {
      chunkSize = Z_DEFAULT_CHUNK;
    } else if (chunkSize < Z_MIN_CHUNK) {
      throw new ERR_OUT_OF_RANGE('options.chunkSize',
                                 `>= ${Z_MIN_CHUNK}`, chunkSize);
    }

    flush = checkRangesOrGetDefault(
      opts.flush, 'options.flush',
      Z_NO_FLUSH, Z_BLOCK, Z_NO_FLUSH);

    finishFlush = checkRangesOrGetDefault(
      opts.finishFlush, 'options.finishFlush',
      Z_NO_FLUSH, Z_BLOCK, Z_FINISH);

    // windowBits is special. On the compression side, 0 is an invalid value.
    // But on the decompression side, a value of 0 for windowBits tells zlib
    // to use the window size in the zlib header of the compressed stream.
    if ((opts.windowBits == null || opts.windowBits === 0) &&
        (mode === INFLATE ||
         mode === GUNZIP ||
         mode === UNZIP)) {
      windowBits = 0;
    } else {
      windowBits = checkRangesOrGetDefault(
        opts.windowBits, 'options.windowBits',
        Z_MIN_WINDOWBITS, Z_MAX_WINDOWBITS, Z_DEFAULT_WINDOWBITS);
    }

    level = checkRangesOrGetDefault(
      opts.level, 'options.level',
      Z_MIN_LEVEL, Z_MAX_LEVEL, Z_DEFAULT_COMPRESSION);

    memLevel = checkRangesOrGetDefault(
      opts.memLevel, 'options.memLevel',
      Z_MIN_MEMLEVEL, Z_MAX_MEMLEVEL, Z_DEFAULT_MEMLEVEL);

    strategy = checkRangesOrGetDefault(
      opts.strategy, 'options.strategy',
      Z_DEFAULT_STRATEGY, Z_FIXED, Z_DEFAULT_STRATEGY);

    dictionary = opts.dictionary;
    if (dictionary !== undefined && !isArrayBufferView(dictionary)) {
      if (isAnyArrayBuffer(dictionary)) {
        dictionary = Buffer.from(dictionary);
      } else {
        throw new ERR_INVALID_ARG_TYPE(
          'options.dictionary',
          ['Buffer', 'TypedArray', 'DataView', 'ArrayBuffer'],
          dictionary
        );
      }
    }

    if (opts.encoding || opts.objectMode || opts.writableObjectMode) {
      opts = _extend({}, opts);
      opts.encoding = null;
      opts.objectMode = false;
      opts.writableObjectMode = false;
    }
  }
  Transform.call(this, opts);
  this.bytesWritten = 0;
  this._handle = new binding.Zlib(mode);
  this._handle.jsref = this; // Used by processCallback() and zlibOnError()
  this._handle.onerror = zlibOnError;
  this._hadError = false;
  this._writeState = new Uint32Array(2);

  if (!this._handle.init(windowBits,
                         level,
                         memLevel,
                         strategy,
                         this._writeState,
                         processCallback,
                         dictionary)) {
    throw new ERR_ZLIB_INITIALIZATION_FAILED();
  }

  this._outBuffer = Buffer.allocUnsafe(chunkSize);
  this._outOffset = 0;
  this._level = level;
  this._strategy = strategy;
  this._chunkSize = chunkSize;
  this._flushFlag = flush;
  this._scheduledFlushFlag = Z_NO_FLUSH;
  this._origFlushFlag = flush;
  this._finishFlushFlag = finishFlush;
  this._info = opts && opts.info;
  this.once('end', this.close);
}
inherits(Zlib, Transform);

Object.defineProperty(Zlib.prototype, '_closed', {
  configurable: true,
  enumerable: true,
  get() {
    return !this._handle;
  }
});

// `bytesRead` made sense as a name when looking from the zlib engine's
// perspective, but it is inconsistent with all other streams exposed by Node.js
// that have this concept, where it stands for the number of bytes read
// *from* the stream (that is, net.Socket/tls.Socket & file system streams).
Object.defineProperty(Zlib.prototype, 'bytesRead', {
  configurable: true,
  enumerable: true,
  get() {
    return this.bytesWritten;
  },
  set(value) {
    this.bytesWritten = value;
  }
});

Zlib.prototype.params = function params(level, strategy, callback) {
  checkRangesOrGetDefault(level, 'level', Z_MIN_LEVEL, Z_MAX_LEVEL);
  checkRangesOrGetDefault(strategy, 'strategy', Z_DEFAULT_STRATEGY, Z_FIXED);

  if (this._level !== level || this._strategy !== strategy) {
    this.flush(Z_SYNC_FLUSH,
               flushCallback.bind(this, level, strategy, callback));
  } else {
    process.nextTick(callback);
  }
};

Zlib.prototype.reset = function reset() {
  if (!this._handle)
    assert(false, 'zlib binding closed');
  return this._handle.reset();
};

// This is the _flush function called by the transform class,
// internally, when the last chunk has been written.
Zlib.prototype._flush = function _flush(callback) {
  this._transform(Buffer.alloc(0), '', callback);
};

// If a flush is scheduled while another flush is still pending, a way to figure
// out which one is the "stronger" flush is needed.
// Roughly, the following holds:
// Z_NO_FLUSH (< Z_TREES) < Z_BLOCK < Z_PARTIAL_FLUSH <
//     Z_SYNC_FLUSH < Z_FULL_FLUSH < Z_FINISH
const flushiness = [];
let i = 0;
for (const flushFlag of [Z_NO_FLUSH, Z_BLOCK, Z_PARTIAL_FLUSH,
                         Z_SYNC_FLUSH, Z_FULL_FLUSH, Z_FINISH]) {
  flushiness[flushFlag] = i++;
}

function maxFlush(a, b) {
  return flushiness[a] > flushiness[b] ? a : b;
}

Zlib.prototype.flush = function flush(kind, callback) {
  var ws = this._writableState;

  if (typeof kind === 'function' || (kind === undefined && !callback)) {
    callback = kind;
    kind = Z_FULL_FLUSH;
  }

  if (ws.ended) {
    if (callback)
      process.nextTick(callback);
  } else if (ws.ending) {
    if (callback)
      this.once('end', callback);
  } else if (ws.needDrain) {
    const alreadyHadFlushScheduled = this._scheduledFlushFlag !== Z_NO_FLUSH;
    this._scheduledFlushFlag = maxFlush(kind, this._scheduledFlushFlag);

    // If a callback was passed, always register a new `drain` + flush handler,
    // mostly because that's simpler and flush callbacks piling up is a rare
    // thing anyway.
    if (!alreadyHadFlushScheduled || callback) {
      const drainHandler = () => this.flush(this._scheduledFlushFlag, callback);
      this.once('drain', drainHandler);
    }
  } else {
    this._flushFlag = kind;
    this.write(Buffer.alloc(0), '', callback);
    this._scheduledFlushFlag = Z_NO_FLUSH;
  }
};

Zlib.prototype.close = function close(callback) {
  _close(this, callback);
  this.destroy();
};

Zlib.prototype._transform = function _transform(chunk, encoding, cb) {
  // If it's the last chunk, or a final flush, we use the Z_FINISH flush flag
  // (or whatever flag was provided using opts.finishFlush).
  // If it's explicitly flushing at some other time, then we use
  // Z_FULL_FLUSH. Otherwise, use the original opts.flush flag.
  var flushFlag;
  var ws = this._writableState;
  if ((ws.ending || ws.ended) && ws.length === chunk.byteLength) {
    flushFlag = this._finishFlushFlag;
  } else {
    flushFlag = this._flushFlag;
    // once we've flushed the last of the queue, stop flushing and
    // go back to the normal behavior.
    if (chunk.byteLength >= ws.length)
      this._flushFlag = this._origFlushFlag;
  }
  processChunk(this, chunk, flushFlag, cb);
};

Zlib.prototype._processChunk = function _processChunk(chunk, flushFlag, cb) {
  // _processChunk() is left for backwards compatibility
  if (typeof cb === 'function')
    processChunk(this, chunk, flushFlag, cb);
  else
    return processChunkSync(this, chunk, flushFlag);
};

function processChunkSync(self, chunk, flushFlag) {
  var availInBefore = chunk.byteLength;
  var availOutBefore = self._chunkSize - self._outOffset;
  var inOff = 0;
  var availOutAfter;
  var availInAfter;

  var buffers = null;
  var nread = 0;
  var inputRead = 0;
  var state = self._writeState;
  var handle = self._handle;
  var buffer = self._outBuffer;
  var offset = self._outOffset;
  var chunkSize = self._chunkSize;

  var error;
  self.on('error', function(er) {
    error = er;
  });

  while (true) {
    handle.writeSync(flushFlag,
                     chunk, // in
                     inOff, // in_off
                     availInBefore, // in_len
                     buffer, // out
                     offset, // out_off
                     availOutBefore); // out_len
    if (error)
      throw error;

    availOutAfter = state[0];
    availInAfter = state[1];

    var inDelta = (availInBefore - availInAfter);
    inputRead += inDelta;

    var have = availOutBefore - availOutAfter;
    if (have > 0) {
      var out = buffer.slice(offset, offset + have);
      offset += have;
      if (!buffers)
        buffers = [out];
      else
        buffers.push(out);
      nread += out.byteLength;
    } else if (have < 0) {
      assert(false, 'have should not go down');
    }

    // exhausted the output buffer, or used all the input create a new one.
    if (availOutAfter === 0 || offset >= chunkSize) {
      availOutBefore = chunkSize;
      offset = 0;
      buffer = Buffer.allocUnsafe(chunkSize);
    }

    if (availOutAfter === 0) {
      // Not actually done. Need to reprocess.
      // Also, update the availInBefore to the availInAfter value,
      // so that if we have to hit it a third (fourth, etc.) time,
      // it'll have the correct byte counts.
      inOff += inDelta;
      availInBefore = availInAfter;
    } else {
      break;
    }
  }

  self.bytesWritten = inputRead;

  if (nread >= kMaxLength) {
    _close(self);
    throw new ERR_BUFFER_TOO_LARGE();
  }

  _close(self);

  if (nread === 0)
    return Buffer.alloc(0);

  return (buffers.length === 1 ? buffers[0] : Buffer.concat(buffers, nread));
}

function processChunk(self, chunk, flushFlag, cb) {
  var handle = self._handle;
  if (!handle)
    assert(false, 'zlib binding closed');

  handle.buffer = chunk;
  handle.cb = cb;
  handle.availOutBefore = self._chunkSize - self._outOffset;
  handle.availInBefore = chunk.byteLength;
  handle.inOff = 0;
  handle.flushFlag = flushFlag;

  handle.write(flushFlag,
               chunk, // in
               0, // in_off
               handle.availInBefore, // in_len
               self._outBuffer, // out
               self._outOffset, // out_off
               handle.availOutBefore); // out_len
}

function processCallback() {
  // This callback's context (`this`) is the `_handle` (ZCtx) object. It is
  // important to null out the values once they are no longer needed since
  // `_handle` can stay in memory long after the buffer is needed.
  var handle = this;
  var self = this.jsref;
  var state = self._writeState;

  if (self._hadError) {
    this.buffer = null;
    return;
  }

  if (self.destroyed) {
    this.buffer = null;
    return;
  }

  var availOutAfter = state[0];
  var availInAfter = state[1];

  const inDelta = handle.availInBefore - availInAfter;
  self.bytesWritten += inDelta;

  var have = handle.availOutBefore - availOutAfter;
  if (have > 0) {
    var out = self._outBuffer.slice(self._outOffset, self._outOffset + have);
    self._outOffset += have;
    self.push(out);
  } else if (have < 0) {
    assert(false, 'have should not go down');
  }

  // exhausted the output buffer, or used all the input create a new one.
  if (availOutAfter === 0 || self._outOffset >= self._chunkSize) {
    handle.availOutBefore = self._chunkSize;
    self._outOffset = 0;
    self._outBuffer = Buffer.allocUnsafe(self._chunkSize);
  }

  if (availOutAfter === 0) {
    // Not actually done. Need to reprocess.
    // Also, update the availInBefore to the availInAfter value,
    // so that if we have to hit it a third (fourth, etc.) time,
    // it'll have the correct byte counts.
    handle.inOff += inDelta;
    handle.availInBefore = availInAfter;

    this.write(handle.flushFlag,
               this.buffer, // in
               handle.inOff, // in_off
               handle.availInBefore, // in_len
               self._outBuffer, // out
               self._outOffset, // out_off
               self._chunkSize); // out_len
    return;
  }

  // finished with the chunk.
  this.buffer = null;
  this.cb();
}

function _close(engine, callback) {
  if (callback)
    process.nextTick(callback);

  // Caller may invoke .close after a zlib error (which will null _handle).
  if (!engine._handle)
    return;

  engine._handle.close();
  engine._handle = null;
}

// generic zlib
// minimal 2-byte header
function Deflate(opts) {
  if (!(this instanceof Deflate))
    return new Deflate(opts);
  Zlib.call(this, opts, DEFLATE);
}
inherits(Deflate, Zlib);

function Inflate(opts) {
  if (!(this instanceof Inflate))
    return new Inflate(opts);
  Zlib.call(this, opts, INFLATE);
}
inherits(Inflate, Zlib);

function Gzip(opts) {
  if (!(this instanceof Gzip))
    return new Gzip(opts);
  Zlib.call(this, opts, GZIP);
}
inherits(Gzip, Zlib);

function Gunzip(opts) {
  if (!(this instanceof Gunzip))
    return new Gunzip(opts);
  Zlib.call(this, opts, GUNZIP);
}
inherits(Gunzip, Zlib);

function DeflateRaw(opts) {
  if (opts && opts.windowBits === 8) opts.windowBits = 9;
  if (!(this instanceof DeflateRaw))
    return new DeflateRaw(opts);
  Zlib.call(this, opts, DEFLATERAW);
}
inherits(DeflateRaw, Zlib);

function InflateRaw(opts) {
  if (!(this instanceof InflateRaw))
    return new InflateRaw(opts);
  Zlib.call(this, opts, INFLATERAW);
}
inherits(InflateRaw, Zlib);

function Unzip(opts) {
  if (!(this instanceof Unzip))
    return new Unzip(opts);
  Zlib.call(this, opts, UNZIP);
}
inherits(Unzip, Zlib);

function createConvenienceMethod(ctor, sync) {
  if (sync) {
    return function(buffer, opts) {
      return zlibBufferSync(new ctor(opts), buffer);
    };
  } else {
    return function(buffer, opts, callback) {
      if (typeof opts === 'function') {
        callback = opts;
        opts = {};
      }
      return zlibBuffer(new ctor(opts), buffer, callback);
    };
  }
}

function createProperty(ctor) {
  return {
    configurable: true,
    enumerable: true,
    value: function(options) {
      return new ctor(options);
    }
  };
}

module.exports = {
  Deflate,
  Inflate,
  Gzip,
  Gunzip,
  DeflateRaw,
  InflateRaw,
  Unzip,

  // Convenience methods.
  // compress/decompress a string or buffer in one step.
  deflate: createConvenienceMethod(Deflate, false),
  deflateSync: createConvenienceMethod(Deflate, true),
  gzip: createConvenienceMethod(Gzip, false),
  gzipSync: createConvenienceMethod(Gzip, true),
  deflateRaw: createConvenienceMethod(DeflateRaw, false),
  deflateRawSync: createConvenienceMethod(DeflateRaw, true),
  unzip: createConvenienceMethod(Unzip, false),
  unzipSync: createConvenienceMethod(Unzip, true),
  inflate: createConvenienceMethod(Inflate, false),
  inflateSync: createConvenienceMethod(Inflate, true),
  gunzip: createConvenienceMethod(Gunzip, false),
  gunzipSync: createConvenienceMethod(Gunzip, true),
  inflateRaw: createConvenienceMethod(InflateRaw, false),
  inflateRawSync: createConvenienceMethod(InflateRaw, true)
};

Object.defineProperties(module.exports, {
  createDeflate: createProperty(Deflate),
  createInflate: createProperty(Inflate),
  createDeflateRaw: createProperty(DeflateRaw),
  createInflateRaw: createProperty(InflateRaw),
  createGzip: createProperty(Gzip),
  createGunzip: createProperty(Gunzip),
  createUnzip: createProperty(Unzip),
  constants: {
    configurable: false,
    enumerable: true,
    value: constants
  },
  codes: {
    enumerable: true,
    writable: false,
    value: Object.freeze(codes)
  }
});

// These should be considered deprecated
// expose all the zlib constants
const bkeys = Object.keys(constants);
for (var bk = 0; bk < bkeys.length; bk++) {
  var bkey = bkeys[bk];
  Object.defineProperty(module.exports, bkey, {
    enumerable: true, value: constants[bkey], writable: false
  });
}

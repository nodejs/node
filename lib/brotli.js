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

process.emitWarning('The brotli API is experimental', 'ExperimentalWarning');

const {
  ERR_BUFFER_TOO_LARGE,
  ERR_INVALID_ARG_TYPE,
  ERR_OUT_OF_RANGE,
  ERR_BROTLI_INITIALIZATION_FAILED
} = require('internal/errors').codes;
const Transform = require('_stream_transform');
const {
  _extend,
  types: {
    isAnyArrayBuffer,
    isArrayBufferView
  }
} = require('util');
const binding = process.binding('brotli');
const assert = require('assert').ok;
const {
  Buffer,
  kMaxLength
} = require('buffer');

const constants = process.binding('constants').brotli;
const {
  COMPRESS, DECOMPRESS,
  BROTLI_PARAM_SIZE_HINT,
  BROTLI_MIN_WINDOW_BITS, BROTLI_MAX_WINDOW_BITS, BROTLI_DEFAULT_WINDOW,
  BROTLI_MIN_QUALITY, BROTLI_MAX_QUALITY, BROTLI_DEFAULT_QUALITY,
  BROTLI_MODE_GENERIC, BROTLI_MODE_FONT, BROTLI_DEFAULT_MODE,
  BROTLI_OPERATION_PROCESS, BROTLI_OPERATION_FLUSH, BROTLI_OPERATION_FINISH,
  BROTLI_DEFAULT_CHUNK, BROTLI_MIN_CHUNK,
  BROTLI_MIN_INPUT_BLOCK_BITS, BROTLI_MAX_INPUT_BLOCK_BITS,
  BROTLI_MAX_NPOSTFIX,
  BROTLI_LARGE_MAX_WINDOW_BITS,
} = constants;

const codes = {
  [COMPRESS]: {
    BROTLI_TRUE: constants.BROTLI_TRUE,
    BROTLI_FALSE: constants.BROTLI_FALSE,
  },

  [DECOMPRESS]: {
    BROTLI_DECODER_NO_ERROR: constants.BROTLI_DECODER_NO_ERROR,
    BROTLI_DECODER_SUCCESS: constants.BROTLI_DECODER_SUCCESS,
    BROTLI_DECODER_NEEDS_MORE_INPUT: constants.BROTLI_DECODER_NEEDS_MORE_INPUT,
    BROTLI_DECODER_NEEDS_MORE_OUTPUT:
      constants.BROTLI_DECODER_NEEDS_MORE_OUTPUT,
    BROTLI_DECODER_ERROR_FORMAT_EXUBERANT_NIBBLE:
      constants.BROTLI_DECODER_ERROR_FORMAT_EXUBERANT_NIBBLE,
    BROTLI_DECODER_ERROR_FORMAT_RESERVED:
      constants.BROTLI_DECODER_ERROR_FORMAT_RESERVED,
    BROTLI_DECODER_ERROR_FORMAT_EXUBERANT_META_NIBBLE:
      constants.BROTLI_DECODER_ERROR_FORMAT_EXUBERANT_META_NIBBLE,
    BROTLI_DECODER_ERROR_FORMAT_SIMPLE_HUFFMAN_ALPHABET:
      constants.BROTLI_DECODER_ERROR_FORMAT_SIMPLE_HUFFMAN_ALPHABET,
    BROTLI_DECODER_ERROR_FORMAT_SIMPLE_HUFFMAN_SAME:
      constants.BROTLI_DECODER_ERROR_FORMAT_SIMPLE_HUFFMAN_SAME,
    BROTLI_DECODER_ERROR_FORMAT_CL_SPACE:
      constants.BROTLI_DECODER_ERROR_FORMAT_CL_SPACE,
    BROTLI_DECODER_ERROR_FORMAT_HUFFMAN_SPACE:
      constants.BROTLI_DECODER_ERROR_FORMAT_HUFFMAN_SPACE,
    BROTLI_DECODER_ERROR_FORMAT_CONTEXT_MAP_REPEAT:
      constants.BROTLI_DECODER_ERROR_FORMAT_CONTEXT_MAP_REPEAT,
    BROTLI_DECODER_ERROR_FORMAT_BLOCK_LENGTH_1:
      constants.BROTLI_DECODER_ERROR_FORMAT_BLOCK_LENGTH_1,
    BROTLI_DECODER_ERROR_FORMAT_BLOCK_LENGTH_2:
      constants.BROTLI_DECODER_ERROR_FORMAT_BLOCK_LENGTH_2,
    BROTLI_DECODER_ERROR_FORMAT_TRANSFORM:
      constants.BROTLI_DECODER_ERROR_FORMAT_TRANSFORM,
    BROTLI_DECODER_ERROR_FORMAT_DICTIONARY:
      constants.BROTLI_DECODER_ERROR_FORMAT_DICTIONARY,
    BROTLI_DECODER_ERROR_FORMAT_WINDOW_BITS:
      constants.BROTLI_DECODER_ERROR_FORMAT_WINDOW_BITS,
    BROTLI_DECODER_ERROR_FORMAT_PADDING_1:
      constants.BROTLI_DECODER_ERROR_FORMAT_PADDING_1,
    BROTLI_DECODER_ERROR_FORMAT_PADDING_2:
      constants.BROTLI_DECODER_ERROR_FORMAT_PADDING_2,
    BROTLI_DECODER_ERROR_FORMAT_DISTANCE:
      constants.BROTLI_DECODER_ERROR_FORMAT_DISTANCE,
    BROTLI_DECODER_ERROR_DICTIONARY_NOT_SET:
      constants.BROTLI_DECODER_ERROR_DICTIONARY_NOT_SET,
    BROTLI_DECODER_ERROR_INVALID_ARGUMENTS:
      constants.BROTLI_DECODER_ERROR_INVALID_ARGUMENTS,
    BROTLI_DECODER_ERROR_ALLOC_CONTEXT_MODES:
      constants.BROTLI_DECODER_ERROR_ALLOC_CONTEXT_MODES,
    BROTLI_DECODER_ERROR_ALLOC_TREE_GROUPS:
      constants.BROTLI_DECODER_ERROR_ALLOC_TREE_GROUPS,
    BROTLI_DECODER_ERROR_ALLOC_CONTEXT_MAP:
      constants.BROTLI_DECODER_ERROR_ALLOC_CONTEXT_MAP,
    BROTLI_DECODER_ERROR_ALLOC_RING_BUFFER_1:
      constants.BROTLI_DECODER_ERROR_ALLOC_RING_BUFFER_1,
    BROTLI_DECODER_ERROR_ALLOC_RING_BUFFER_2:
      constants.BROTLI_DECODER_ERROR_ALLOC_RING_BUFFER_2,
    BROTLI_DECODER_ERROR_ALLOC_BLOCK_TYPE_TREES:
      constants.BROTLI_DECODER_ERROR_ALLOC_BLOCK_TYPE_TREES,
    BROTLI_DECODER_ERROR_UNREACHABLE:
      constants.BROTLI_DECODER_ERROR_UNREACHABLE,
  },
};

const keys = Object.keys(codes);
for (var k = 0; k < keys.length; k++) {
  const ckeys = Object.keys(codes[keys[k]]);
  for (var ck = 0; ck < ckeys.length; ck++) {
    var ckey = ckeys[ck];
    codes[keys[k]][codes[keys[k]][ckey]] = ckey;
  }
}

function brotliBuffer(engine, buffer, callback) {
  // Streams do not support non-Buffer ArrayBufferViews yet. Convert it to a
  // Buffer without copying.
  if (isArrayBufferView(buffer) &&
      Object.getPrototypeOf(buffer) !== Buffer.prototype) {
    buffer = Buffer.from(buffer.buffer, buffer.byteOffset, buffer.byteLength);
  } else if (isAnyArrayBuffer(buffer)) {
    buffer = Buffer.from(buffer);
  }
  if (engine._mode === COMPRESS)
    engine.setParameter(BROTLI_PARAM_SIZE_HINT, buffer.length);
  engine.buffers = null;
  engine.nread = 0;
  engine.cb = callback;
  engine.on('data', brotliBufferOnData);
  engine.on('error', brotliBufferOnError);
  engine.on('end', brotliBufferOnEnd);
  engine.end(buffer);
}

function brotliBufferOnData(chunk) {
  if (!this.buffers)
    this.buffers = [chunk];
  else
    this.buffers.push(chunk);
  this.nread += chunk.length;
}

function brotliBufferOnError(err) {
  this.removeAllListeners('end');
  this.cb(err);
}

function brotliBufferOnEnd() {
  let buf;
  let err;
  if (this.nread >= kMaxLength) {
    err = new ERR_BUFFER_TOO_LARGE();
  } else if (this.nread === 0) {
    buf = Buffer.alloc(0);
  } else {
    const bufs = this.buffers;
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

function brotliBufferSync(engine, buffer) {
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
  if (engine._mode === COMPRESS)
    engine.setParameter(BROTLI_PARAM_SIZE_HINT, buffer.length);
  buffer = processChunkSync(engine, buffer, BROTLI_OPERATION_FINISH);
  if (engine._info)
    return { buffer, engine };
  return buffer;
}

function brotliOnError(message, errno) {
  const self = this.jsref;
  self._hadError = true;

  // eslint-disable-next-line no-restricted-syntax
  const error = new Error(message);
  error.errno = errno;
  error.code = codes[self._mode][errno];

  // there is no way to cleanly recover.
  // continuing only obscures problems.
  _close(self);

  self.emit('error', error);
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

// the Brotli class they all inherit from
// This thing manages the queue of requests, and returns
// true or false if there is anything in the queue when
// you call the .write() method.
class Brotli extends Transform {
  constructor(opts, mode) {
    let chunkSize = BROTLI_DEFAULT_CHUNK;

    // Encoder options
    let encoderMode = BROTLI_DEFAULT_MODE;
    let quality = BROTLI_DEFAULT_QUALITY;
    let lgwin = BROTLI_DEFAULT_WINDOW;
    let lgblock = 0;
    let disableLiteralContextModeling = 0;
    let sizeHint = 0;
    let npostfix = 0;
    let ndirect = 0;

    // Decoder options
    let disableRingBufferReallocation = 0;

    // Shared options
    let largeWindow = 0;

    // The Brotli class is not exported to user land, the mode should only be
    // passed in by us.
    assert(typeof mode === 'number');
    assert(mode >= COMPRESS && mode <= DECOMPRESS);

    if (opts) {
      chunkSize = checkRangesOrGetDefault(
        opts.chunkSize, 'options.chunkSize',
        BROTLI_MIN_CHUNK, Infinity,
        BROTLI_DEFAULT_CHUNK);

      // Shared options
      largeWindow = checkRangesOrGetDefault(
        Number(opts.largeWindow), 'options.largeWindow',
        0, 1, largeWindow);

      // Encoder options
      encoderMode = checkRangesOrGetDefault(
        opts.mode, 'options.mode',
        BROTLI_MODE_GENERIC, BROTLI_MODE_FONT, encoderMode);

      quality = checkRangesOrGetDefault(
        opts.quality, 'options.quality',
        BROTLI_MIN_QUALITY, BROTLI_MAX_QUALITY, quality);

      lgwin = checkRangesOrGetDefault(
        opts.lgwin, 'options.lgwin',
        BROTLI_MIN_WINDOW_BITS,
        largeWindow ? BROTLI_LARGE_MAX_WINDOW_BITS : BROTLI_MAX_WINDOW_BITS,
        lgwin);

      lgblock = checkRangesOrGetDefault(
        opts.lgblock, 'options.lgblock',
        BROTLI_MIN_INPUT_BLOCK_BITS, BROTLI_MAX_INPUT_BLOCK_BITS, lgblock);

      disableLiteralContextModeling = checkRangesOrGetDefault(
        Number(opts.disableLiteralContextModeling),
        'options.disableLiteralContextModeling',
        0, 1, disableLiteralContextModeling);

      sizeHint = checkRangesOrGetDefault(
        opts.sizeHint, 'options.sizeHint',
        0, (2 ** 32) - 1, sizeHint);

      npostfix = checkRangesOrGetDefault(
        opts.npostfix, 'options.npostfix',
        0, BROTLI_MAX_NPOSTFIX, npostfix);

      ndirect = checkRangesOrGetDefault(
        opts.ndirect, 'options.ndirect',
        0, 15 << npostfix, ndirect);

      // Decoder options
      disableRingBufferReallocation = checkRangesOrGetDefault(
        Number(opts.disableRingBufferReallocation),
        'options.disableRingBufferReallocation',
        0, 1, disableRingBufferReallocation);

      if (opts.encoding || opts.objectMode || opts.writableObjectMode) {
        opts = _extend({}, opts);
        opts.encoding = null;
        opts.objectMode = false;
        opts.writableObjectMode = false;
      }
    }

    super(opts);

    this.bytesWritten = 0;
    this._handle = new binding.Brotli(mode);
    this._handle.jsref = this; // Used by processCallback() and brotliOnError()
    this._handle.onerror = brotliOnError;
    this._hadError = false;
    this._writeState = new Uint32Array(2);

    if (!this._handle.init(encoderMode,
                           quality,
                           lgwin,
                           lgblock,
                           disableLiteralContextModeling,
                           sizeHint,
                           npostfix,
                           ndirect,
                           disableRingBufferReallocation,
                           largeWindow,
                           this._writeState,
                           processCallback)) {
      throw new ERR_BROTLI_INITIALIZATION_FAILED();
    }

    this._mode = mode;
    this._outBuffer = Buffer.allocUnsafe(chunkSize);
    this._outOffset = 0;
    this._chunkSize = chunkSize;
    this._flushFlag = BROTLI_OPERATION_PROCESS;
    this._scheduledFlushFlag = BROTLI_OPERATION_PROCESS;
    this._info = opts && opts.info;
    this.once('end', this.close);
  }

  get _closed() {
    return !this._handle;
  }

  reset() {
    if (!this._handle)
      assert(false, 'brotli binding closed');
    return this._handle.reset();
  }

  // This is the _flush function called by the transform class,
  // internally, when the last chunk has been written.
  _flush(callback) {
    this._transform(Buffer.alloc(0), '', callback);
  }

  flush(callback) {
    const ws = this._writableState;

    if (ws.ended) {
      if (callback)
        process.nextTick(callback);
    } else if (ws.ending) {
      if (callback)
        this.once('end', callback);
    } else if (ws.needDrain) {
      const alreadyHadFlushScheduled =
        this._scheduledFlushFlag !== BROTLI_OPERATION_PROCESS;
      this._scheduledFlushFlag = BROTLI_OPERATION_FLUSH;

      // If a callback was passed, always register a new `drain` + flush
      // handler, mostly because that's simpler and flush callbacks piling
      // up is a rare thing anyway.
      if (!alreadyHadFlushScheduled || callback) {
        const drainHandler = () =>
          this.flush(callback);
        this.once('drain', drainHandler);
      }
    } else {
      this._flushFlag = BROTLI_OPERATION_FLUSH;
      this.write(Buffer.alloc(0), callback);
      this._scheduledFlushFlag = BROTLI_OPERATION_PROCESS;
    }
  }

  close(callback) {
    _close(this, callback);
    this.destroy();
  }

  _transform(chunk, encoding, cb) {
    // If it's the last chunk, or a final flush, we use the
    // BROTLI_OPERATION_FINISH flush flag If it's explicitly
    // flushing at some other time, then we use BROTLI_OPERATION_FLUSH.
    // Otherwise, use the original opts.flush flag.
    let flushFlag;
    const ws = this._writableState;
    if ((ws.ending || ws.ended) && ws.length === chunk.byteLength) {
      flushFlag = BROTLI_OPERATION_FINISH;
    } else {
      flushFlag = this._flushFlag;
      // once we've flushed the last of the queue, stop flushing and
      // go back to the normal behavior.
      if (chunk.byteLength >= ws.length)
        this._flushFlag = BROTLI_OPERATION_PROCESS;
    }
    processChunk(this, chunk, flushFlag, cb);
  }

  setParameter(param, value) {
    this._handle.params(param, value);
  }
}

function processChunkSync(self, chunk, flushFlag) {
  let availInBefore = chunk.byteLength;
  let availOutBefore = self._chunkSize - self._outOffset;
  let inOff = 0;
  let availOutAfter;
  let availInAfter;

  let buffers = null;
  let nread = 0;
  let inputRead = 0;
  const state = self._writeState;
  const handle = self._handle;
  let buffer = self._outBuffer;
  let offset = self._outOffset;
  const chunkSize = self._chunkSize;

  let error;
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

    const inDelta = (availInBefore - availInAfter);
    inputRead += inDelta;

    const have = availOutBefore - availOutAfter;
    if (have > 0) {
      const out = buffer.slice(offset, offset + have);
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
  const handle = self._handle;
  if (!handle)
    assert(false, 'brotli binding closed');

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
  const handle = this;
  const self = this.jsref;
  const state = self._writeState;

  if (self._hadError) {
    this.buffer = null;
    return;
  }

  if (self.destroyed) {
    this.buffer = null;
    return;
  }

  const availOutAfter = state[0];
  const availInAfter = state[1];

  const inDelta = handle.availInBefore - availInAfter;
  self.bytesWritten += inDelta;

  const have = handle.availOutBefore - availOutAfter;
  if (have > 0) {
    const out = self._outBuffer.slice(self._outOffset, self._outOffset + have);
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

  // Caller may invoke .close after a brotli error (which will null _handle).
  if (!engine._handle)
    return;

  engine._handle.close();
  engine._handle = null;
}

class Compress extends Brotli {
  constructor(opts) {
    super(opts, COMPRESS);
  }
}

Object.defineProperties(Compress, {
  codes: {
    enumerable: true,
    writable: false,
    value: Object.freeze(codes[COMPRESS])
  }
});

class Decompress extends Brotli {
  constructor(opts) {
    super(opts, DECOMPRESS);
  }
}

Object.defineProperties(Decompress, {
  codes: {
    enumerable: true,
    writable: false,
    value: Object.freeze(codes[DECOMPRESS])
  }
});

function createConvenienceMethod(ctor, sync) {
  if (sync) {
    return function(buffer, opts) {
      return brotliBufferSync(new ctor(opts), buffer);
    };
  } else {
    return function(buffer, opts, callback) {
      if (typeof opts === 'function') {
        callback = opts;
        opts = {};
      }
      if (typeof callback === 'function') {
        return brotliBuffer(new ctor(opts), buffer, callback);
      } else {
        return new Promise((resolve, reject) => {
          brotliBuffer(new ctor(opts), buffer, (err, outBuf) => {
            if (err) return reject(err);
            resolve(outBuf);
          });
        });
      }
    };
  }
}

module.exports = {
  Compress,
  Decompress,

  // Convenience methods.
  // compress/decompress a string or buffer in one step.
  compress: createConvenienceMethod(Compress, false),
  compressSync: createConvenienceMethod(Compress, true),
  decompress: createConvenienceMethod(Decompress, false),
  decompressSync: createConvenienceMethod(Decompress, true),
};

Object.defineProperties(module.exports, {
  constants: {
    configurable: false,
    enumerable: true,
    value: constants
  },
});

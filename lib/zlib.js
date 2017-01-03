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

const Buffer = require('buffer').Buffer;
const internalUtil = require('internal/util');
const Transform = require('_stream_transform');
const binding = process.binding('zlib');
const assert = require('assert').ok;
const kMaxLength = require('buffer').kMaxLength;
const kRangeErrorMessage = 'Cannot create final Buffer. It would be larger ' +
                           `than 0x${kMaxLength.toString(16)} bytes`;

const constants = process.binding('constants').zlib;
const createClassWrapper = internalUtil.createClassWrapper;

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

function isInvalidFlushFlag(flag) {
  return typeof flag !== 'number' ||
         flag < constants.Z_NO_FLUSH ||
         flag > constants.Z_BLOCK;

  // Covers: constants.Z_NO_FLUSH (0),
  //         constants.Z_PARTIAL_FLUSH (1),
  //         constants.Z_SYNC_FLUSH (2),
  //         constants.Z_FULL_FLUSH (3),
  //         constants.Z_FINISH (4), and
  //         constants.Z_BLOCK (5)
}

function isInvalidStrategy(strategy) {
  return typeof strategy !== 'number' ||
         strategy < constants.Z_DEFAULT_STRATEGY ||
         strategy > constants.Z_FIXED;

  // Covers: constants.Z_FILTERED, (1)
  //         constants.Z_HUFFMAN_ONLY (2),
  //         constants.Z_RLE (3),
  //         constants.Z_FIXED (4), and
  //         constants.Z_DEFAULT_STRATEGY (0)
}

function zlibBuffer(engine, buffer, callback) {
  var buffers = [];
  var nread = 0;

  engine.on('error', onError);
  engine.on('end', onEnd);

  engine.end(buffer);
  flow();

  function flow() {
    var chunk;
    while (null !== (chunk = engine.read())) {
      buffers.push(chunk);
      nread += chunk.length;
    }
    engine.once('readable', flow);
  }

  function onError(err) {
    engine.removeListener('end', onEnd);
    engine.removeListener('readable', flow);
    callback(err);
  }

  function onEnd() {
    var buf;
    var err = null;

    if (nread >= kMaxLength) {
      err = new RangeError(kRangeErrorMessage);
    } else {
      buf = Buffer.concat(buffers, nread);
    }

    buffers = [];
    engine.close();
    callback(err, buf);
  }
}

function zlibBufferSync(engine, buffer) {
  if (typeof buffer === 'string')
    buffer = Buffer.from(buffer);
  if (!(buffer instanceof Buffer))
    throw new TypeError('Not a string or buffer');

  var flushFlag = engine._finishFlushFlag;

  return engine._processChunk(buffer, flushFlag);
}

function zlibOnError(message, errno) {
  // there is no way to cleanly recover.
  // continuing only obscures problems.
  _close(this);
  this._hadError = true;

  var error = new Error(message);
  error.errno = errno;
  error.code = codes[errno];
  this.emit('error', error);
}

function flushCallback(level, strategy, callback) {
  assert(this._handle, 'zlib binding closed');
  this._handle.params(level, strategy);
  if (!this._hadError) {
    this._level = level;
    this._strategy = strategy;
    if (callback) callback();
  }
}

// the Zlib class they all inherit from
// This thing manages the queue of requests, and returns
// true or false if there is anything in the queue when
// you call the .write() method.
class Zlib extends Transform {
  constructor(opts, mode) {
    opts = opts || {};
    super(opts);

    this._opts = opts;
    this._chunkSize = opts.chunkSize || constants.Z_DEFAULT_CHUNK;

    if (opts.flush && isInvalidFlushFlag(opts.flush)) {
      throw new RangeError('Invalid flush flag: ' + opts.flush);
    }
    if (opts.finishFlush && isInvalidFlushFlag(opts.finishFlush)) {
      throw new RangeError('Invalid flush flag: ' + opts.finishFlush);
    }

    this._flushFlag = opts.flush || constants.Z_NO_FLUSH;
    this._finishFlushFlag = opts.finishFlush !== undefined ?
      opts.finishFlush : constants.Z_FINISH;

    if (opts.chunkSize) {
      if (opts.chunkSize < constants.Z_MIN_CHUNK) {
        throw new RangeError('Invalid chunk size: ' + opts.chunkSize);
      }
    }

    if (opts.windowBits) {
      if (opts.windowBits < constants.Z_MIN_WINDOWBITS ||
          opts.windowBits > constants.Z_MAX_WINDOWBITS) {
        throw new RangeError('Invalid windowBits: ' + opts.windowBits);
      }
    }

    if (opts.level) {
      if (opts.level < constants.Z_MIN_LEVEL ||
          opts.level > constants.Z_MAX_LEVEL) {
        throw new RangeError('Invalid compression level: ' + opts.level);
      }
    }

    if (opts.memLevel) {
      if (opts.memLevel < constants.Z_MIN_MEMLEVEL ||
          opts.memLevel > constants.Z_MAX_MEMLEVEL) {
        throw new RangeError('Invalid memLevel: ' + opts.memLevel);
      }
    }

    if (opts.strategy && isInvalidStrategy(opts.strategy))
      throw new TypeError('Invalid strategy: ' + opts.strategy);

    if (opts.dictionary) {
      if (!(opts.dictionary instanceof Buffer)) {
        throw new TypeError(
          'Invalid dictionary: it should be a Buffer instance');
      }
    }

    this._handle = new binding.Zlib(mode);
    this._handle.onerror = zlibOnError.bind(this);
    this._hadError = false;

    var level = constants.Z_DEFAULT_COMPRESSION;
    if (typeof opts.level === 'number') level = opts.level;

    var strategy = constants.Z_DEFAULT_STRATEGY;
    if (typeof opts.strategy === 'number') strategy = opts.strategy;

    this._handle.init(opts.windowBits || constants.Z_DEFAULT_WINDOWBITS,
                      level,
                      opts.memLevel || constants.Z_DEFAULT_MEMLEVEL,
                      strategy,
                      opts.dictionary);

    this._buffer = Buffer.allocUnsafe(this._chunkSize);
    this._offset = 0;
    this._level = level;
    this._strategy = strategy;

    this.once('end', this.close);
  }

  get _closed() {
    return !this._handle;
  }

  params(level, strategy, callback) {
    if (level < constants.Z_MIN_LEVEL ||
        level > constants.Z_MAX_LEVEL) {
      throw new RangeError('Invalid compression level: ' + level);
    }
    if (isInvalidStrategy(strategy))
      throw new TypeError('Invalid strategy: ' + strategy);

    if (this._level !== level || this._strategy !== strategy) {
      this.flush(constants.Z_SYNC_FLUSH,
                 flushCallback.bind(this, level, strategy, callback));
    } else {
      process.nextTick(callback);
    }
  }

  reset() {
    assert(this._handle, 'zlib binding closed');
    return this._handle.reset();
  }

  // This is the _flush function called by the transform class,
  // internally, when the last chunk has been written.
  _flush(callback) {
    this._transform(Buffer.alloc(0), '', callback);
  }

  flush(kind, callback) {
    var ws = this._writableState;

    if (typeof kind === 'function' || (kind === undefined && !callback)) {
      callback = kind;
      kind = constants.Z_FULL_FLUSH;
    }

    if (ws.ended) {
      if (callback)
        process.nextTick(callback);
    } else if (ws.ending) {
      if (callback)
        this.once('end', callback);
    } else if (ws.needDrain) {
      if (callback) {
        const drainHandler = () => this.flush(kind, callback);
        this.once('drain', drainHandler);
      }
    } else {
      this._flushFlag = kind;
      this.write(Buffer.alloc(0), '', callback);
    }
  }

  close(callback) {
    _close(this, callback);
    process.nextTick(emitCloseNT, this);
  }

  _transform(chunk, encoding, cb) {
    var flushFlag;
    var ws = this._writableState;
    var ending = ws.ending || ws.ended;
    var last = ending && (!chunk || ws.length === chunk.length);

    if (chunk !== null && !(chunk instanceof Buffer))
      return cb(new TypeError('invalid input'));

    if (!this._handle)
      return cb(new Error('zlib binding closed'));

    // If it's the last chunk, or a final flush, we use the Z_FINISH flush flag
    // (or whatever flag was provided using opts.finishFlush).
    // If it's explicitly flushing at some other time, then we use
    // Z_FULL_FLUSH. Otherwise, use Z_NO_FLUSH for maximum compression
    // goodness.
    if (last)
      flushFlag = this._finishFlushFlag;
    else {
      flushFlag = this._flushFlag;
      // once we've flushed the last of the queue, stop flushing and
      // go back to the normal behavior.
      if (chunk.length >= ws.length) {
        this._flushFlag = this._opts.flush || constants.Z_NO_FLUSH;
      }
    }

    this._processChunk(chunk, flushFlag, cb);
  }

  _processChunk(chunk, flushFlag, cb) {
    var availInBefore = chunk && chunk.length;
    var availOutBefore = this._chunkSize - this._offset;
    var inOff = 0;

    var self = this;

    var async = typeof cb === 'function';

    if (!async) {
      var buffers = [];
      var nread = 0;

      var error;
      this.on('error', function(er) {
        error = er;
      });

      assert(this._handle, 'zlib binding closed');
      do {
        var res = this._handle.writeSync(flushFlag,
                                         chunk, // in
                                         inOff, // in_off
                                         availInBefore, // in_len
                                         this._buffer, // out
                                         this._offset, //out_off
                                         availOutBefore); // out_len
      } while (!this._hadError && callback(res[0], res[1]));

      if (this._hadError) {
        throw error;
      }

      if (nread >= kMaxLength) {
        _close(this);
        throw new RangeError(kRangeErrorMessage);
      }

      var buf = Buffer.concat(buffers, nread);
      _close(this);

      return buf;
    }

    assert(this._handle, 'zlib binding closed');
    var req = this._handle.write(flushFlag,
                                 chunk, // in
                                 inOff, // in_off
                                 availInBefore, // in_len
                                 this._buffer, // out
                                 this._offset, //out_off
                                 availOutBefore); // out_len

    req.buffer = chunk;
    req.callback = callback;

    function callback(availInAfter, availOutAfter) {
      // When the callback is used in an async write, the callback's
      // context is the `req` object that was created. The req object
      // is === this._handle, and that's why it's important to null
      // out the values after they are done being used. `this._handle`
      // can stay in memory longer than the callback and buffer are needed.
      if (this) {
        this.buffer = null;
        this.callback = null;
      }

      if (self._hadError)
        return;

      var have = availOutBefore - availOutAfter;
      assert(have >= 0, 'have should not go down');

      if (have > 0) {
        var out = self._buffer.slice(self._offset, self._offset + have);
        self._offset += have;
        // serve some output to the consumer.
        if (async) {
          self.push(out);
        } else {
          buffers.push(out);
          nread += out.length;
        }
      }

      // exhausted the output buffer, or used all the input create a new one.
      if (availOutAfter === 0 || self._offset >= self._chunkSize) {
        availOutBefore = self._chunkSize;
        self._offset = 0;
        self._buffer = Buffer.allocUnsafe(self._chunkSize);
      }

      if (availOutAfter === 0) {
        // Not actually done.  Need to reprocess.
        // Also, update the availInBefore to the availInAfter value,
        // so that if we have to hit it a third (fourth, etc.) time,
        // it'll have the correct byte counts.
        inOff += (availInBefore - availInAfter);
        availInBefore = availInAfter;

        if (!async)
          return true;

        var newReq = self._handle.write(flushFlag,
                                        chunk,
                                        inOff,
                                        availInBefore,
                                        self._buffer,
                                        self._offset,
                                        self._chunkSize);
        newReq.callback = callback; // this same function
        newReq.buffer = chunk;
        return;
      }

      if (!async)
        return false;

      // finished with the chunk.
      cb();
    }
  }
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

function emitCloseNT(self) {
  self.emit('close');
}

// generic zlib
// minimal 2-byte header
class Deflate extends Zlib {
  constructor(opts) {
    super(opts, constants.DEFLATE);
  }
}

class Inflate extends Zlib {
  constructor(opts) {
    super(opts, constants.INFLATE);
  }
}

class Gzip extends Zlib {
  constructor(opts) {
    super(opts, constants.GZIP);
  }
}

class Gunzip extends Zlib {
  constructor(opts) {
    super(opts, constants.GUNZIP);
  }
}

class DeflateRaw extends Zlib {
  constructor(opts) {
    super(opts, constants.DEFLATERAW);
  }
}

class InflateRaw extends Zlib {
  constructor(opts) {
    super(opts, constants.INFLATERAW);
  }
}

class Unzip extends Zlib {
  constructor(opts) {
    super(opts, constants.UNZIP);
  }
}

function createConvenienceMethod(type, sync) {
  if (sync) {
    return function(buffer, opts) {
      return zlibBufferSync(new type(opts), buffer);
    };
  } else {
    return function(buffer, opts, callback) {
      if (typeof opts === 'function') {
        callback = opts;
        opts = {};
      }
      return zlibBuffer(new type(opts), buffer, callback);
    };
  }
}

function createProperty(type) {
  return {
    configurable: true,
    enumerable: true,
    value: type
  };
}

module.exports = {
  Deflate: createClassWrapper(Deflate),
  Inflate: createClassWrapper(Inflate),
  Gzip: createClassWrapper(Gzip),
  Gunzip: createClassWrapper(Gunzip),
  DeflateRaw: createClassWrapper(DeflateRaw),
  InflateRaw: createClassWrapper(InflateRaw),
  Unzip: createClassWrapper(Unzip),

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
  createDeflate: createProperty(module.exports.Deflate),
  createInflate: createProperty(module.exports.Inflate),
  createDeflateRaw: createProperty(module.exports.DeflateRaw),
  createInflateRaw: createProperty(module.exports.InflateRaw),
  createGzip: createProperty(module.exports.Gzip),
  createGunzip: createProperty(module.exports.Gunzip),
  createUnzip: createProperty(module.exports.Unzip),
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

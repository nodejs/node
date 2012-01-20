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

var binding = process.binding('zlib');
var util = require('util');
var stream = require('stream');
var assert = require('assert').ok;

// zlib doesn't provide these, so kludge them in following the same
// const naming scheme zlib uses.
binding.Z_MIN_WINDOWBITS = 8;
binding.Z_MAX_WINDOWBITS = 15;
binding.Z_DEFAULT_WINDOWBITS = 15;

// fewer than 64 bytes per chunk is stupid.
// technically it could work with as few as 8, but even 64 bytes
// is absurdly low.  Usually a MB or more is best.
binding.Z_MIN_CHUNK = 64;
binding.Z_MAX_CHUNK = Infinity;
binding.Z_DEFAULT_CHUNK = (16 * 1024);

binding.Z_MIN_MEMLEVEL = 1;
binding.Z_MAX_MEMLEVEL = 9;
binding.Z_DEFAULT_MEMLEVEL = 8;

binding.Z_MIN_LEVEL = -1;
binding.Z_MAX_LEVEL = 9;
binding.Z_DEFAULT_LEVEL = binding.Z_DEFAULT_COMPRESSION;

// expose all the zlib constants
Object.keys(binding).forEach(function(k) {
  if (k.match(/^Z/)) exports[k] = binding[k];
});


exports.Deflate = Deflate;
exports.Inflate = Inflate;
exports.Gzip = Gzip;
exports.Gunzip = Gunzip;
exports.DeflateRaw = DeflateRaw;
exports.InflateRaw = InflateRaw;
exports.Unzip = Unzip;

exports.createDeflate = function(o) {
  return new Deflate(o);
};

exports.createInflate = function(o) {
  return new Inflate(o);
};

exports.createDeflateRaw = function(o) {
  return new DeflateRaw(o);
};

exports.createInflateRaw = function(o) {
  return new InflateRaw(o);
};

exports.createGzip = function(o) {
  return new Gzip(o);
};

exports.createGunzip = function(o) {
  return new Gunzip(o);
};

exports.createUnzip = function(o) {
  return new Unzip(o);
};


// Convenience methods.
// compress/decompress a string or buffer in one step.
exports.deflate = function(buffer, callback) {
  zlibBuffer(new Deflate(), buffer, callback);
};

exports.gzip = function(buffer, callback) {
  zlibBuffer(new Gzip(), buffer, callback);
};

exports.deflateRaw = function(buffer, callback) {
  zlibBuffer(new DeflateRaw(), buffer, callback);
};

exports.unzip = function(buffer, callback) {
  zlibBuffer(new Unzip(), buffer, callback);
};

exports.inflate = function(buffer, callback) {
  zlibBuffer(new Inflate(), buffer, callback);
};

exports.gunzip = function(buffer, callback) {
  zlibBuffer(new Gunzip(), buffer, callback);
};

exports.inflateRaw = function(buffer, callback) {
  zlibBuffer(new InflateRaw(), buffer, callback);
};

function zlibBuffer(engine, buffer, callback) {
  var buffers = [];
  var nread = 0;

  function onError(err) {
    engine.removeListener('end', onEnd);
    engine.removeListener('error', onError);
    callback(err);
  }

  function onData(chunk) {
    buffers.push(chunk);
    nread += chunk.length;
  }

  function onEnd() {
    var buffer;
    switch(buffers.length) {
      case 0:
        buffer = new Buffer(0);
        break;
      case 1:
        buffer = buffers[0];
        break;
      default:
        buffer = new Buffer(nread);
        var n = 0;
        buffers.forEach(function(b) {
          var l = b.length;
          b.copy(buffer, n, 0, l);
          n += l;
        });
        break;
    }
    callback(null, buffer);
  }

  engine.on('error', onError);
  engine.on('data', onData);
  engine.on('end', onEnd);

  engine.write(buffer);
  engine.end();
}



// generic zlib
// minimal 2-byte header
function Deflate(opts) {
  if (!(this instanceof Deflate)) return new Deflate(opts);
  Zlib.call(this, opts, binding.Deflate);
}

function Inflate(opts) {
  if (!(this instanceof Inflate)) return new Inflate(opts);
  Zlib.call(this, opts, binding.Inflate);
}



// gzip - bigger header, same deflate compression
function Gzip(opts) {
  if (!(this instanceof Gzip)) return new Gzip(opts);
  Zlib.call(this, opts, binding.Gzip);
}

function Gunzip(opts) {
  if (!(this instanceof Gunzip)) return new Gunzip(opts);
  Zlib.call(this, opts, binding.Gunzip);
}



// raw - no header
function DeflateRaw(opts) {
  if (!(this instanceof DeflateRaw)) return new DeflateRaw(opts);
  Zlib.call(this, opts, binding.DeflateRaw);
}

function InflateRaw(opts) {
  if (!(this instanceof InflateRaw)) return new InflateRaw(opts);
  Zlib.call(this, opts, binding.InflateRaw);
}


// auto-detect header.
function Unzip(opts) {
  if (!(this instanceof Unzip)) return new Unzip(opts);
  Zlib.call(this, opts, binding.Unzip);
}


// the Zlib class they all inherit from
// This thing manages the queue of requests, and returns
// true or false if there is anything in the queue when
// you call the .write() method.

function Zlib(opts, Binding) {
  this._opts = opts = opts || {};
  this._queue = [];
  this._processing = false;
  this._ended = false;
  this.readable = true;
  this.writable = true;
  this._flush = binding.Z_NO_FLUSH;

  if (opts.chunkSize) {
    if (opts.chunkSize < exports.Z_MIN_CHUNK ||
        opts.chunkSize > exports.Z_MAX_CHUNK) {
      throw new Error('Invalid chunk size: ' + opts.chunkSize);
    }
  }

  if (opts.windowBits) {
    if (opts.windowBits < exports.Z_MIN_WINDOWBITS ||
        opts.windowBits > exports.Z_MAX_WINDOWBITS) {
      throw new Error('Invalid windowBits: ' + opts.windowBits);
    }
  }

  if (opts.level) {
    if (opts.level < exports.Z_MIN_LEVEL ||
        opts.level > exports.Z_MAX_LEVEL) {
      throw new Error('Invalid compression level: ' + opts.level);
    }
  }

  if (opts.memLevel) {
    if (opts.memLevel < exports.Z_MIN_MEMLEVEL ||
        opts.memLevel > exports.Z_MAX_MEMLEVEL) {
      throw new Error('Invalid memLevel: ' + opts.memLevel);
    }
  }

  if (opts.strategy) {
    if (opts.strategy != exports.Z_FILTERED &&
        opts.strategy != exports.Z_HUFFMAN_ONLY &&
        opts.strategy != exports.Z_RLE &&
        opts.strategy != exports.Z_FIXED &&
        opts.strategy != exports.Z_DEFAULT_STRATEGY) {
      throw new Error('Invalid strategy: ' + opts.strategy);
    }
  }

  this._binding = new Binding();
  this._binding.init(opts.windowBits || exports.Z_DEFAULT_WINDOWBITS,
                     opts.level || exports.Z_DEFAULT_COMPRESSION,
                     opts.memLevel || exports.Z_DEFAULT_MEMLEVEL,
                     opts.strategy || exports.Z_DEFAULT_STRATEGY);

  this._chunkSize = opts.chunkSize || exports.Z_DEFAULT_CHUNK;
  this._buffer = new Buffer(this._chunkSize);
  this._offset = 0;
  var self = this;
}

util.inherits(Zlib, stream.Stream);

Zlib.prototype.write = function write(chunk, cb) {
  if (this._ended) {
    return this.emit('error', new Error('Cannot write after end'));
  }

  if (arguments.length === 1 && typeof chunk === 'function') {
    cb = chunk;
    chunk = null;
  }

  if (!chunk) {
    chunk = null;
  } else if (typeof chunk === 'string') {
    chunk = new Buffer(chunk);
  } else if (!Buffer.isBuffer(chunk)) {
    return this.emit('error', new Error('Invalid argument'));
  }


  var empty = this._queue.length === 0;

  this._queue.push([chunk, cb]);
  this._process();
  if (!empty) {
    this._needDrain = true;
  }
  return empty;
};

Zlib.prototype.flush = function flush(cb) {
  this._flush = binding.Z_SYNC_FLUSH;
  return this.write(cb);
};

Zlib.prototype.end = function end(chunk, cb) {
  var self = this;
  this._ending = true;
  var ret = this.write(chunk, function() {
    self.emit('end');
    if (cb) cb();
  });
  this._ended = true;
  return ret;
};

Zlib.prototype._process = function() {
  if (this._processing || this._paused) return;

  if (this._queue.length === 0) {
    if (this._needDrain) {
      this._needDrain = false;
      this.emit('drain');
    }
    // nothing to do, waiting for more data at this point.
    return;
  }

  var req = this._queue.shift();
  var cb = req.pop();
  var chunk = req.pop();

  if (this._ending && this._queue.length === 0) {
    this._flush = binding.Z_FINISH;
  }

  var self = this;
  var availInBefore = chunk && chunk.length;
  var availOutBefore = this._chunkSize - this._offset;

  var inOff = 0;
  var req = this._binding.write(this._flush,
                                chunk, // in
                                inOff, // in_off
                                availInBefore, // in_len
                                this._buffer, // out
                                this._offset, //out_off
                                availOutBefore); // out_len

  req.buffer = chunk;
  req.callback = callback;
  this._processing = req;

  function callback(availInAfter, availOutAfter, buffer) {
    var have = availOutBefore - availOutAfter;

    assert(have >= 0, 'have should not go down');

    if (have > 0) {
      var out = self._buffer.slice(self._offset, self._offset + have);
      self._offset += have;
      self.emit('data', out);
    }

    // XXX Maybe have a 'min buffer' size so we don't dip into the
    // thread pool with only 1 byte available or something?
    if (availOutAfter === 0 || self._offset >= self._chunkSize) {
      availOutBefore = self._chunkSize;
      self._offset = 0;
      self._buffer = new Buffer(self._chunkSize);
    }

    if (availOutAfter === 0) {
      // Not actually done.  Need to reprocess.
      // Also, update the availInBefore to the availInAfter value,
      // so that if we have to hit it a third (fourth, etc.) time,
      // it'll have the correct byte counts.
      inOff += (availInBefore - availInAfter);
      availInBefore = availInAfter;

      var newReq = self._binding.write(self._flush,
                                       chunk,
                                       inOff,
                                       availInBefore,
                                       self._buffer,
                                       self._offset,
                                       self._chunkSize);
      newReq.callback = callback; // this same function
      newReq.buffer = chunk;
      self._processing = newReq;
      return;
    }

    // finished with the chunk.
    self._processing = false;
    if (cb) cb();
    self._process();
  }
};

Zlib.prototype.pause = function() {
  this._paused = true;
  this.emit('pause');
};

Zlib.prototype.resume = function() {
  this._paused = false;
  this._process();
};

util.inherits(Deflate, Zlib);
util.inherits(Inflate, Zlib);
util.inherits(Gzip, Zlib);
util.inherits(Gunzip, Zlib);
util.inherits(DeflateRaw, Zlib);
util.inherits(InflateRaw, Zlib);
util.inherits(Unzip, Zlib);

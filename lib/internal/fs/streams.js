'use strict';

const {
  Array,
  MathMin,
  ObjectDefineProperty,
  ObjectSetPrototypeOf,
  Symbol,
} = primordials;

const {
  ERR_INVALID_ARG_TYPE,
  ERR_OUT_OF_RANGE,
  ERR_STREAM_DESTROYED
} = require('internal/errors').codes;
const { deprecate } = require('internal/util');
const { validateInteger } = require('internal/validators');
const fs = require('fs');
const { Buffer } = require('buffer');
const {
  copyObject,
  getOptions,
} = require('internal/fs/utils');
const { Readable, Writable, finished } = require('stream');
const { toPathIfFileURL } = require('internal/url');
const kIoDone = Symbol('kIoDone');
const kIsPerformingIO = Symbol('kIsPerformingIO');

const kFs = Symbol('kFs');

function ReadStream(path, options) {
  if (!(this instanceof ReadStream))
    return new ReadStream(path, options);

  // A little bit bigger buffer and water marks by default
  options = copyObject(getOptions(options, {}));
  if (options.highWaterMark === undefined)
    options.highWaterMark = 64 * 1024;

  if (options.autoDestroy === undefined) {
    options.autoDestroy = false;
  }

  this[kFs] = options.fs || fs;

  if (typeof this[kFs].open !== 'function') {
    throw new ERR_INVALID_ARG_TYPE('options.fs.open', 'function',
                                   this[kFs].open);
  }

  if (typeof this[kFs].read !== 'function') {
    throw new ERR_INVALID_ARG_TYPE('options.fs.read', 'function',
                                   this[kFs].read);
  }

  if (typeof this[kFs].close !== 'function') {
    throw new ERR_INVALID_ARG_TYPE('options.fs.close', 'function',
                                   this[kFs].close);
  }

  Readable.call(this, options);

  // Path will be ignored when fd is specified, so it can be falsy
  this.path = toPathIfFileURL(path);
  this.fd = options.fd === undefined ? null : options.fd;
  this.flags = options.flags === undefined ? 'r' : options.flags;
  this.mode = options.mode === undefined ? 0o666 : options.mode;

  this.start = options.start;
  this.end = options.end;
  this.autoClose = options.autoClose === undefined ? true : options.autoClose;
  this.pos = undefined;
  this.bytesRead = 0;
  this.closed = false;
  this[kIsPerformingIO] = false;

  if (this.start !== undefined) {
    validateInteger(this.start, 'start', 0);

    this.pos = this.start;
  }

  if (this.end === undefined) {
    this.end = Infinity;
  } else if (this.end !== Infinity) {
    validateInteger(this.end, 'end', 0);

    if (this.start !== undefined && this.start > this.end) {
      throw new ERR_OUT_OF_RANGE(
        'start',
        `<= "end" (here: ${this.end})`,
        this.start
      );
    }
  }

  if (typeof this.fd !== 'number')
    _openReadFs(this);

  this.on('end', function() {
    if (this.autoClose) {
      this.destroy();
    }
  });
}
ObjectSetPrototypeOf(ReadStream.prototype, Readable.prototype);
ObjectSetPrototypeOf(ReadStream, Readable);

const openReadFs = deprecate(function() {
  _openReadFs(this);
}, 'ReadStream.prototype.open() is deprecated', 'DEP0135');
ReadStream.prototype.open = openReadFs;

function _openReadFs(stream) {
  // Backwards compat for overriden open.
  if (stream.open !== openReadFs) {
    stream.open();
    return;
  }

  stream[kFs].open(stream.path, stream.flags, stream.mode, (er, fd) => {
    if (er) {
      if (stream.autoClose) {
        stream.destroy();
      }
      stream.emit('error', er);
      return;
    }

    stream.fd = fd;
    stream.emit('open', fd);
    stream.emit('ready');
    // Start the flow of data.
    stream.read();
  });
}

ReadStream.prototype._read = function(n) {
  if (typeof this.fd !== 'number') {
    return this.once('open', function() {
      this._read(n);
    });
  }

  if (this.destroyed) return;

  n = this.pos !== undefined ?
    MathMin(this.end - this.pos + 1, n) :
    MathMin(this.end - this.bytesRead + 1, n);

  if (n <= 0) {
    this.push(null);
    return;
  }

  const buf = Buffer.allocUnsafeSlow(n);

  this[kIsPerformingIO] = true;
  this[kFs]
    .read(this.fd, buf, 0, n, this.pos, (er, bytesRead, buf) => {
      this[kIsPerformingIO] = false;

      // Tell ._destroy() that it's safe to close the fd now.
      if (this.destroyed) {
        this.emit(kIoDone, er);
        return;
      }

      if (er) {
        if (this.autoClose) {
          this.destroy();
        }
        this.emit('error', er);
      } else if (bytesRead > 0) {
        if (this.pos !== undefined) {
          this.pos += bytesRead;
        }

        this.bytesRead += bytesRead;

        if (bytesRead !== buf.length) {
          // Slow path. Shrink to fit.
          // Copy instead of slice so that we don't retain
          // large backing buffer for small reads.
          const dst = Buffer.allocUnsafeSlow(bytesRead);
          buf.copy(dst, 0, 0, bytesRead);
          buf = dst;
        }

        this.push(buf);
      } else {
        this.push(null);
      }
    });
};

ReadStream.prototype._destroy = function(err, cb) {
  if (typeof this.fd !== 'number') {
    this.once('open', closeFsStream.bind(null, this, cb, err));
    return;
  }

  if (this[kIsPerformingIO]) {
    this.once(kIoDone, (er) => closeFsStream(this, cb, err || er));
    return;
  }

  closeFsStream(this, cb, err);
};

function closeFsStream(stream, cb, err) {
  stream[kFs].close(stream.fd, (er) => {
    stream.closed = true;
    cb(er || err);
  });

  stream.fd = null;
}

ReadStream.prototype.close = function(cb) {
  if (typeof cb === 'function') finished(this, cb);
  this.destroy();
};

ObjectDefineProperty(ReadStream.prototype, 'pending', {
  get() { return this.fd === null; },
  configurable: true
});

function WriteStream(path, options) {
  if (!(this instanceof WriteStream))
    return new WriteStream(path, options);

  options = copyObject(getOptions(options, {}));

  // Only buffers are supported.
  options.decodeStrings = true;

  if (options.autoDestroy === undefined) {
    options.autoDestroy = options.autoClose === undefined ?
      true : (options.autoClose || false);
  }

  this[kFs] = options.fs || fs;
  if (typeof this[kFs].open !== 'function') {
    throw new ERR_INVALID_ARG_TYPE('options.fs.open', 'function',
                                   this[kFs].open);
  }

  if (!this[kFs].write && !this[kFs].writev) {
    throw new ERR_INVALID_ARG_TYPE('options.fs.write', 'function',
                                   this[kFs].write);
  }

  if (this[kFs].write && typeof this[kFs].write !== 'function') {
    throw new ERR_INVALID_ARG_TYPE('options.fs.write', 'function',
                                   this[kFs].write);
  }

  if (this[kFs].writev && typeof this[kFs].writev !== 'function') {
    throw new ERR_INVALID_ARG_TYPE('options.fs.writev', 'function',
                                   this[kFs].writev);
  }

  if (typeof this[kFs].close !== 'function') {
    throw new ERR_INVALID_ARG_TYPE('options.fs.close', 'function',
                                   this[kFs].close);
  }

  // It's enough to override either, in which case only one will be used.
  if (!this[kFs].write) {
    this._write = null;
  }
  if (!this[kFs].writev) {
    this._writev = null;
  }

  Writable.call(this, options);

  // Path will be ignored when fd is specified, so it can be falsy
  this.path = toPathIfFileURL(path);
  this.fd = options.fd === undefined ? null : options.fd;
  this.flags = options.flags === undefined ? 'w' : options.flags;
  this.mode = options.mode === undefined ? 0o666 : options.mode;

  this.start = options.start;
  this.autoClose = options.autoDestroy;
  this.pos = undefined;
  this.bytesWritten = 0;
  this.closed = false;
  this[kIsPerformingIO] = false;

  if (this.start !== undefined) {
    validateInteger(this.start, 'start', 0);

    this.pos = this.start;
  }

  if (options.encoding)
    this.setDefaultEncoding(options.encoding);

  if (typeof this.fd !== 'number')
    _openWriteFs(this);
}
ObjectSetPrototypeOf(WriteStream.prototype, Writable.prototype);
ObjectSetPrototypeOf(WriteStream, Writable);

WriteStream.prototype._final = function(callback) {
  if (typeof this.fd !== 'number') {
    return this.once('open', function() {
      this._final(callback);
    });
  }

  callback();
};

const openWriteFs = deprecate(function() {
  _openWriteFs(this);
}, 'WriteStream.prototype.open() is deprecated', 'DEP0135');
WriteStream.prototype.open = openWriteFs;

function _openWriteFs(stream) {
  // Backwards compat for overriden open.
  if (stream.open !== openWriteFs) {
    stream.open();
    return;
  }

  stream[kFs].open(stream.path, stream.flags, stream.mode, (er, fd) => {
    if (er) {
      if (stream.autoClose) {
        stream.destroy();
      }
      stream.emit('error', er);
      return;
    }

    stream.fd = fd;
    stream.emit('open', fd);
    stream.emit('ready');
  });
}


WriteStream.prototype._write = function(data, encoding, cb) {
  if (typeof this.fd !== 'number') {
    return this.once('open', function() {
      this._write(data, encoding, cb);
    });
  }

  if (this.destroyed) return cb(new ERR_STREAM_DESTROYED('write'));

  this[kIsPerformingIO] = true;
  this[kFs].write(this.fd, data, 0, data.length, this.pos, (er, bytes) => {
    this[kIsPerformingIO] = false;
    // Tell ._destroy() that it's safe to close the fd now.
    if (this.destroyed) {
      cb(er);
      return this.emit(kIoDone, er);
    }

    if (er) {
      return cb(er);
    }
    this.bytesWritten += bytes;
    cb();
  });

  if (this.pos !== undefined)
    this.pos += data.length;
};


WriteStream.prototype._writev = function(data, cb) {
  if (typeof this.fd !== 'number') {
    return this.once('open', function() {
      this._writev(data, cb);
    });
  }

  if (this.destroyed) return cb(new ERR_STREAM_DESTROYED('write'));

  const len = data.length;
  const chunks = new Array(len);
  let size = 0;

  for (let i = 0; i < len; i++) {
    const chunk = data[i].chunk;

    chunks[i] = chunk;
    size += chunk.length;
  }

  this[kIsPerformingIO] = true;
  this[kFs].writev(this.fd, chunks, this.pos, (er, bytes) => {
    this[kIsPerformingIO] = false;
    // Tell ._destroy() that it's safe to close the fd now.
    if (this.destroyed) {
      cb(er);
      return this.emit(kIoDone, er);
    }

    if (er) {
      if (this.autoClose) {
        this.destroy(er);
      }
      return cb(er);
    }
    this.bytesWritten += bytes;
    cb();
  });

  if (this.pos !== undefined)
    this.pos += size;
};


WriteStream.prototype._destroy = ReadStream.prototype._destroy;
WriteStream.prototype.close = function(cb) {
  if (cb) {
    if (this.closed) {
      process.nextTick(cb);
      return;
    }
    this.on('close', cb);
  }

  // If we are not autoClosing, we should call
  // destroy on 'finish'.
  if (!this.autoClose) {
    this.on('finish', this.destroy);
  }

  // We use end() instead of destroy() because of
  // https://github.com/nodejs/node/issues/2006
  this.end();
};

// There is no shutdown() for files.
WriteStream.prototype.destroySoon = WriteStream.prototype.end;

ObjectDefineProperty(WriteStream.prototype, 'pending', {
  get() { return this.fd === null; },
  configurable: true
});

module.exports = {
  ReadStream,
  WriteStream
};

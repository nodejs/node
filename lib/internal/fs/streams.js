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
  ERR_OUT_OF_RANGE
} = require('internal/errors').codes;
const { deprecate } = require('internal/util');
const { validateInteger } = require('internal/validators');
const { errorOrDestroy } = require('internal/streams/destroy');
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

function _construct(callback) {
  const stream = this;
  if (typeof stream.fd === 'number') {
    callback();
    return;
  }

  if (stream.open !== openWriteFs && stream.open !== openReadFs) {
    // Backwards compat for monkey patching open().
    const orgEmit = stream.emit;
    stream.emit = function(...args) {
      if (args[0] === 'open') {
        this.emit = orgEmit;
        callback();
        orgEmit.apply(this, args);
      } else if (args[0] === 'error') {
        this.emit = orgEmit;
        callback(args[1]);
      } else {
        orgEmit.apply(this, args);
      }
    };
    stream.open();
  } else {
    stream[kFs].open(stream.path, stream.flags, stream.mode, (er, fd) => {
      if (er) {
        callback(er);
      } else {
        stream.fd = fd;
        callback();
        stream.emit('open', stream.fd);
        stream.emit('ready');
      }
    });
  }
}

function close(stream, err, cb) {
  if (!stream.fd) {
    // TODO(ronag)
    // stream.closed = true;
    cb(err);
  } else {
    stream[kFs].close(stream.fd, (er) => {
      stream.closed = true;
      cb(er || err);
    });
    stream.fd = null;
  }
}

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

  options.autoDestroy = options.autoClose === undefined ?
    true : options.autoClose;

  // Path will be ignored when fd is specified, so it can be falsy
  this.path = toPathIfFileURL(path);
  this.fd = options.fd === undefined ? null : options.fd;
  this.flags = options.flags === undefined ? 'r' : options.flags;
  this.mode = options.mode === undefined ? 0o666 : options.mode;

  this.start = options.start;
  this.end = options.end;
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

  Readable.call(this, options);
}
ObjectSetPrototypeOf(ReadStream.prototype, Readable.prototype);
ObjectSetPrototypeOf(ReadStream, Readable);

ObjectDefineProperty(ReadStream.prototype, 'autoClose', {
  get() {
    return this._readableState.autoDestroy;
  },
  set(val) {
    this._readableState.autoDestroy = val;
  }
});

const openReadFs = deprecate(function() {
  // Noop.
}, 'ReadStream.prototype.open() is deprecated', 'DEP0135');
ReadStream.prototype.open = openReadFs;

ReadStream.prototype._construct = _construct;

ReadStream.prototype._read = function(n) {
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
        errorOrDestroy(this, er);
      } else if (bytesRead > 0) {
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

  if (this.pos !== undefined) {
    this.pos += n;
  }
};

ReadStream.prototype._destroy = function(err, cb) {
  // Usually for async IO it is safe to close a file descriptor
  // even when there are pending operations. However, due to platform
  // differences file IO is implemented using synchronous operations
  // running in a thread pool. Therefore, file descriptors are not safe
  // to close while used in a pending read or write operation. Wait for
  // any pending IO (kIsPerformingIO) to complete (kIoDone).
  if (this[kIsPerformingIO]) {
    this.once(kIoDone, (er) => close(this, err || er, cb));
  } else {
    close(this, err, cb);
  }
};

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

  options.autoDestroy = options.autoClose === undefined ?
    true : options.autoClose;

  // Path will be ignored when fd is specified, so it can be falsy
  this.path = toPathIfFileURL(path);
  this.fd = options.fd === undefined ? null : options.fd;
  this.flags = options.flags === undefined ? 'w' : options.flags;
  this.mode = options.mode === undefined ? 0o666 : options.mode;

  this.start = options.start;
  this.pos = undefined;
  this.bytesWritten = 0;
  this.closed = false;
  this[kIsPerformingIO] = false;

  if (this.start !== undefined) {
    validateInteger(this.start, 'start', 0);

    this.pos = this.start;
  }

  Writable.call(this, options);

  if (options.encoding)
    this.setDefaultEncoding(options.encoding);
}
ObjectSetPrototypeOf(WriteStream.prototype, Writable.prototype);
ObjectSetPrototypeOf(WriteStream, Writable);

ObjectDefineProperty(WriteStream.prototype, 'autoClose', {
  get() {
    return this._writableState.autoDestroy;
  },
  set(val) {
    this._writableState.autoDestroy = val;
  }
});

const openWriteFs = deprecate(function() {
  // Noop.
}, 'WriteStream.prototype.open() is deprecated', 'DEP0135');
WriteStream.prototype.open = openWriteFs;

WriteStream.prototype._construct = _construct;

WriteStream.prototype._write = function(data, encoding, cb) {
  this[kIsPerformingIO] = true;
  this[kFs].write(this.fd, data, 0, data.length, this.pos, (er, bytes) => {
    this[kIsPerformingIO] = false;
    if (this.destroyed) {
      // Tell ._destroy() that it's safe to close the fd now.
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
    if (this.destroyed) {
      // Tell ._destroy() that it's safe to close the fd now.
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
    this.pos += size;
};

WriteStream.prototype._destroy = function(err, cb) {
  // Usually for async IO it is safe to close a file descriptor
  // even when there are pending operations. However, due to platform
  // differences file IO is implemented using synchronous operations
  // running in a thread pool. Therefore, file descriptors are not safe
  // to close while used in a pending read or write operation. Wait for
  // any pending IO (kIsPerformingIO) to complete (kIoDone).
  if (this[kIsPerformingIO]) {
    this.once(kIoDone, (er) => close(this, err || er, cb));
  } else {
    close(this, err, cb);
  }
};

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

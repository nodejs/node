'use strict';

// This file is derived from the original SonicBoom module
// MIT License
// Copyright (c) 2017 Matteo Collina

const {
  ArrayPrototypePush,
  AtomicsWait,
  Int32Array,
  MathMax,
  SymbolDispose,
  globalThis: {
    Number,
    SharedArrayBuffer,
  },
} = primordials;

const {
  Buffer,
} = require('buffer');

const fs = require('fs');
const EventEmitter = require('events');
const path = require('path');
const {
  clearInterval,
  setInterval,
  setTimeout,
} = require('timers');
const {
  codes: {
    ERR_INVALID_ARG_TYPE,
    ERR_INVALID_ARG_VALUE,
    ERR_INVALID_STATE,
    ERR_OPERATION_FAILED,
  },
} = require('internal/errors');

const {
  validateBoolean,
  validateFunction,
  validateObject,
  validateOneOf,
  validateUint32,
} = require('internal/validators');

const assert = require('assert');

const BUSY_WRITE_TIMEOUT = 100;
const kEmptyBuffer = Buffer.allocUnsafe(0);

const kNil = new Int32Array(new SharedArrayBuffer(4));

function sleep(ms) {
  // Also filters out NaN, non-number types, including empty strings, but allows bigints
  const valid = ms > 0 && ms < Infinity;
  if (valid === false) {
    if (typeof ms !== 'number' && typeof ms !== 'bigint') {
      throw new ERR_INVALID_ARG_TYPE('ms', ['number', 'bigint'], ms);
    }
    throw new ERR_INVALID_ARG_VALUE.RangeError('ms', ms,
                                               'must be a number greater than 0 and less than Infinity');
  }

  AtomicsWait(kNil, 0, 0, Number(ms));
}

// 16 KB. Don't write more than docker buffer size.
// https://github.com/moby/moby/blob/513ec73831269947d38a644c278ce3cac36783b2/daemon/logger/copier.go#L13
const kMaxWrite = 16 * 1024;
const kContentModeBuffer = 'buffer';
const kContentModeUtf8 = 'utf8';
const kNullPrototype = { __proto__: null };

// FastUtf8Stream is a port of the original SonicBoom module
// (https://github.com/pinojs/sonic-boom) that provides a fast and efficient
// way to write UTF-8 encoded data to a file or stream.
class FastUtf8Stream extends EventEmitter {
  #len = 0;
  #fd = -1;
  #bufs = [];
  #lens = [];
  #writing = false;
  #ending = false;
  #reopening = false;
  #asyncDrainScheduled = false;
  #flushPending = false;
  #hwm = 16387; // 16 KB
  #file = null;
  #destroyed = false;
  #minLength = 0;
  #maxLength = 0;
  #maxWrite = kMaxWrite;
  #opening = false;
  #periodicFlush = 0;
  #periodicFlushTimer = undefined;
  #sync = false;
  #fsync = false;
  #append = true;
  #mode = 0o666;
  #retryEAGAIN = () => true;
  #mkdir = false;
  #writingBuf = '';
  #write;
  #flush;
  #flushSync;
  #actualWrite;
  #fsWriteSync;
  #fsWrite;

  constructor(options = kNullPrototype) {
    validateObject(options, 'options');
    let {
      fd,
    } = options;
    const {
      dest,
      minLength,
      maxLength,
      maxWrite,
      periodicFlush,
      sync,
      append = true,
      mkdir,
      retryEAGAIN,
      fsync,
      contentMode = kContentModeUtf8,
      mode,
    } = options;

    super();

    fd ??= dest;

    this.#hwm = MathMax(minLength || 0, this.#hwm);
    this.#minLength = minLength || 0;
    this.#maxLength = maxLength || 0;
    this.#maxWrite = maxWrite || kMaxWrite;
    this.#periodicFlush = periodicFlush || 0;
    this.#sync = sync || false;
    this.#fsync = fsync || false;
    this.#append = append || false;
    this.#mode = mode;
    this.#retryEAGAIN = retryEAGAIN || (() => true);
    this.#mkdir = mkdir || false;

    validateUint32(this.#hwm, 'options.hwm');
    validateUint32(this.#minLength, 'options.minLength');
    validateUint32(this.#maxLength, 'options.maxLength');
    validateUint32(this.#maxWrite, 'options.maxWrite');
    validateUint32(this.#periodicFlush, 'options.periodicFlush');
    validateBoolean(this.#sync, 'options.sync');
    validateBoolean(this.#fsync, 'options.fsync');
    validateBoolean(this.#append, 'options.append');
    validateBoolean(this.#mkdir, 'options.mkdir');
    validateFunction(this.#retryEAGAIN, 'options.retryEAGAIN');
    validateOneOf(contentMode, 'options.contentMode', [kContentModeBuffer, kContentModeUtf8]);

    if (contentMode === kContentModeBuffer) {
      this.#writingBuf = kEmptyBuffer;
      this.#write = (...args) => this.#writeBuffer(...args);
      this.#flush = (...args) => this.#flushBuffer(...args);
      this.#flushSync = (...args) => this.#flushBufferSync(...args);
      this.#actualWrite = (...args) => this.#actualWriteBuffer(...args);
      this.#fsWriteSync = () => fs.writeSync(this.#fd, this.#writingBuf);
      this.#fsWrite = () => fs.write(this.#fd, this.#writingBuf, (...args) => this.#release(...args));
    } else {
      this.#writingBuf = '';
      this.#write = (...args) => this.#writeUtf8(...args);
      this.#flush = (...args) => this.#flushUtf8(...args);
      this.#flushSync = (...args) => this.#flushSyncUtf8(...args);
      this.#actualWrite = (...args) => this.#actualWriteUtf8(...args);
      this.#fsWriteSync = () => fs.writeSync(this.#fd, this.#writingBuf, 'utf8');
      this.#fsWrite = () => fs.write(this.#fd, this.#writingBuf, 'utf8', (...args) => this.#release(...args));
    }

    if (typeof fd === 'number') {
      this.#fd = fd;
      process.nextTick(() => this.emit('ready'));
    } else if (typeof fd === 'string') {
      this.#openFile(fd);
    } else {
      throw new ERR_INVALID_ARG_TYPE('fd', ['number', 'string'], fd);
    }
    if (this.#minLength >= this.#maxWrite) {
      throw new ERR_INVALID_ARG_VALUE.RangeError('minLength', this.#minLength,
                                                 `should be smaller than maxWrite (${this.#maxWrite})`);
    }

    this.on('newListener', (name) => {
      if (name === 'drain') {
        this._asyncDrainScheduled = false;
      }
    });

    if (this.#periodicFlush !== 0) {
      this.#periodicFlushTimer = setInterval(() => this.flush(null), this.#periodicFlush);
      this.#periodicFlushTimer.unref();
    }
  }

  write(data) {
    return this.#write(data);
  }

  flush(cb = function() {}) { return this.#flush(cb); }

  flushSync() { return this.#flushSync(); }

  reopen(file) {
    if (this.#destroyed) {
      throw new ERR_INVALID_STATE('FastUtf8Stream is destroyed');
    }

    if (this.#opening) {
      this.once('ready', () => this.reopen(file));
      return;
    }

    if (this.#ending) {
      return;
    }

    if (!this.#file) {
      throw new ERR_OPERATION_FAILED(
        'Unable to reopen a file descriptor, you must pass a file to SonicBoom');
    }

    if (file) {
      this.#file = file;
    }
    this.#reopening = true;

    if (this.#writing) {
      return;
    }

    const fd = this.#fd;
    this.once('ready', () => {
      if (fd !== this.#fd) {
        fs.close(fd, (err) => {
          if (err) {
            return this.emit('error', err);
          }
        });
      }
    });

    this.#openFile(this.#file);
  }

  end() {
    if (this.#destroyed) {
      throw new ERR_INVALID_STATE('FastUtf8Stream is destroyed');
    }

    if (this.#opening) {
      this.once('ready', () => {
        this.end();
      });
      return;
    }

    if (this.#ending) {
      return;
    }

    this.#ending = true;

    if (this.#writing) {
      return;
    }

    if (this.#len > 0 && this.#fd >= 0) {
      this.#actualWrite();
    } else {
      this.#actualClose();
    }
  }

  destroy() {
    if (this.#destroyed) {
      return;
    }
    this.#actualClose();
  }

  get mode() { return this.#mode; }
  get file() { return this.#file; }
  get fd() { return this.#fd; }
  get minLength() { return this.#minLength; }
  get maxLength() { return this.#maxLength; }
  get writing() { return this.#writing; }
  get sync() { return this.#sync; }
  get fsync() { return this.#fsync; }
  get append() { return this.#append; }
  get periodicFlush() { return this.#periodicFlush; }
  get contentMode() {
    return this.#writingBuf instanceof Buffer ? kContentModeBuffer : kContentModeUtf8;
  }
  get mkdir() { return this.#mkdir; }

  [SymbolDispose]() { this.destroy(); }

  #release(err, n) {
    if (err) {
      if ((err.code === 'EAGAIN' || err.code === 'EBUSY') &&
          this.#retryEAGAIN(err, this.#writingBuf.length, this.#len - this.#writingBuf.length)) {
        if (this.#sync) {
          // This error code should not happen in sync mode, because it is
          // not using the underlining operating system asynchronous functions.
          // However it happens, and so we handle it.
          // Ref: https://github.com/pinojs/pino/issues/783
          try {
            sleep(BUSY_WRITE_TIMEOUT);
            this.#release(undefined, 0);
          } catch (err) {
            this.#release(err);
          }
        } else {
          // Let's give the destination some time to process the chunk.
          setTimeout(() => this.#fsWrite(), BUSY_WRITE_TIMEOUT);
        }
      } else {
        this.#writing = false;

        this.emit('error', err);
      }
      return;
    }

    this.emit('write', n);
    const releasedBufObj = releaseWritingBuf(this.#writingBuf, this.#len, n);
    this.#len = releasedBufObj.len;
    this.#writingBuf = releasedBufObj.writingBuf;

    if (this.#writingBuf.length) {
      if (!this.#sync) {
        this.#fsWrite();
        return;
      }

      try {
        do {
          const n = this.#fsWriteSync();
          const releasedBufObj = releaseWritingBuf(this.#writingBuf, this.#len, n);
          this.#len = releasedBufObj.len;
          this.#writingBuf = releasedBufObj.writingBuf;
        } while (this.#writingBuf.length);
      } catch (err) {
        this.#release(err);
        return;
      }
    }

    if (this.#fsync) {
      fs.fsyncSync(this.#fd);
    }

    const len = this.#len;
    if (this.#reopening) {
      this.#writing = false;
      this.#reopening = false;
      this.reopen();
    } else if (len > this.#minLength) {
      this.#actualWrite();
    } else if (this.#ending) {
      if (len > 0) {
        this.#actualWrite();
      } else {
        this.#writing = false;
        this.#actualClose();
      }
    } else {
      this.#writing = false;
      if (this.#sync) {
        if (!this.#asyncDrainScheduled) {
          this.#asyncDrainScheduled = true;
          process.nextTick(() => this.#emitDrain());
        }
      } else {
        this.emit('drain');
      }
    }
  }

  #openFile(file) {
    this.#opening = true;
    this.#writing = true;
    this.#asyncDrainScheduled = false;

    // NOTE: 'error' and 'ready' events emitted below only relevant when sonic.sync===false
    // for sync mode, there is no way to add a listener that will receive these

    const fileOpened = (err, fd) => {
      if (err) {
        this.#reopening = false;
        this.#writing = false;
        this.#opening = false;

        if (this.#sync) {
          process.nextTick(() => {
            if (this.listenerCount('error') > 0) {
              this.emit('error', err);
            }
          });
        } else {
          this.emit('error', err);
        }
        return;
      }

      const reopening = this.#reopening;

      this.#fd = fd;
      this.#file = file;
      this.#reopening = false;
      this.#opening = false;
      this.#writing = false;

      if (this.#sync) {
        process.nextTick(() => this.emit('ready'));
      } else {
        this.emit('ready');
      }

      if (this.#destroyed) {
        return;
      }

      // start
      if ((!this.#writing && this.#len > this.#minLength) || this.#flushPending) {
        this.#actualWrite();
      } else if (reopening) {
        process.nextTick(() => this.emit('drain'));
      }
    };

    const flags = this.#append ? 'a' : 'w';
    const mode = this.#mode;

    if (this.#sync) {
      try {
        if (this.#mkdir) fs.mkdirSync(path.dirname(file), { recursive: true });
        const fd = fs.openSync(file, flags, mode);
        fileOpened(null, fd);
      } catch (err) {
        fileOpened(err);
        throw err;
      }
    } else if (this.#mkdir) {
      fs.mkdir(path.dirname(file), { recursive: true }, (err) => {
        if (err) return fileOpened(err);
        fs.open(file, flags, mode, fileOpened);
      });
    } else {
      fs.open(file, flags, mode, fileOpened);
    }
  }

  #emitDrain() {
    const hasListeners = this.listenerCount('drain') > 0;
    if (!hasListeners) return;
    this.#asyncDrainScheduled = false;
    this.emit('drain');
  }

  #actualClose() {
    if (this.#fd === -1) {
      this.once('ready', () => this.#actualClose());
      return;
    }

    if (this.#periodicFlushTimer !== undefined) {
      clearInterval(this.#periodicFlushTimer);
    }

    this.#destroyed = true;
    this.#bufs = [];
    this.#lens = [];

    assert(typeof this.#fd === 'number', `sonic.fd must be a number, got ${typeof this.#fd}`);

    const done = (err) => {
      if (err) {
        this.emit('error', err);
        return;
      }

      if (this.#ending && !this.#writing) {
        this.emit('finish');
      }
      this.emit('close');
    };

    const closeWrapped = () => {
      // We skip errors in fsync
      if (this.#fd !== 1 && this.#fd !== 2) {
        fs.close(this.#fd, done);
      } else {
        done();
      }
    };

    try {
      fs.fsync(this.#fd, closeWrapped);
    } catch {
      // Intentionally empty.
    }
  }

  #actualWriteBuffer() {
    this.#writing = true;
    this.#writingBuf = this.#writingBuf.length ? this.#writingBuf : mergeBuf(this.#bufs.shift(), this.#lens.shift());

    if (this.#sync) {
      try {
        const written = fs.writeSync(this.#fd, this.#writingBuf);
        this.#release(null, written);
      } catch (err) {
        this.#release(err);
      }
    } else {
      // fs.write will need to copy string to buffer anyway so
      // we do it here to avoid the overhead of calculating the buffer size
      // in releaseWritingBuf.
      this.#writingBuf = Buffer.from(this.#writingBuf);
      fs.write(this.#fd, this.#writingBuf, (...args) => this.#release(...args));
    }
  }

  #actualWriteUtf8() {
    this.#writing = true;
    this.#writingBuf ||= this.#bufs.shift() || '';

    if (this.#sync) {
      try {
        const written = fs.writeSync(this.#fd, this.#writingBuf, 'utf8');
        this.#release(null, written);
      } catch (err) {
        this.#release(err);
      }
    } else {
      fs.write(this.#fd, this.#writingBuf, 'utf8', (...args) => this.#release(...args));
    }
  }

  #flushBufferSync() {
    if (this.#destroyed) {
      throw new ERR_INVALID_STATE('FastUtf8Stream is destroyed');
    }

    if (this.#fd < 0) {
      throw new ERR_INVALID_STATE('Invalid file descriptor');
    }

    if (!this.#writing && this.#writingBuf.length > 0) {
      this.#bufs.unshift([this.#writingBuf]);
      this.#writingBuf = kEmptyBuffer;
    }

    let buf = kEmptyBuffer;
    while (this.#bufs.length || buf.length) {
      if (buf.length <= 0) {
        buf = mergeBuf(this.#bufs[0], this.#lens[0]);
      }
      try {
        const n = fs.writeSync(this.#fd, buf);
        buf = buf.subarray(n);
        this.#len = MathMax(this.#len - n, 0);
        if (buf.length <= 0) {
          this.#bufs.shift();
          this.#lens.shift();
        }
      } catch (err) {
        const shouldRetry = err.code === 'EAGAIN' || err.code === 'EBUSY';
        if (shouldRetry && !this.#retryEAGAIN(err, buf.length, this.#len - buf.length)) {
          throw err;
        }

        sleep(BUSY_WRITE_TIMEOUT);
      }
    }
  }

  #flushSyncUtf8() {
    if (this.#destroyed) {
      throw new ERR_INVALID_STATE('FastUtf8Stream is destroyed');
    }

    if (this.#fd < 0) {
      throw new ERR_INVALID_STATE('Invalid file descriptor');
    }

    if (!this.#writing && this.#writingBuf.length > 0) {
      this.#bufs.unshift(this.#writingBuf);
      this.#writingBuf = '';
    }

    let buf = '';
    while (this.#bufs.length || buf) {
      if (buf.length <= 0) {
        buf = this.#bufs[0];
      }
      try {
        const n = fs.writeSync(this.#fd, buf, 'utf8');
        const releasedBufObj = releaseWritingBuf(buf, this.#len, n);
        buf = releasedBufObj.writingBuf;
        this.#len = releasedBufObj.len;
        if (buf.length <= 0) {
          this.#bufs.shift();
        }
      } catch (err) {
        const shouldRetry = err.code === 'EAGAIN' || err.code === 'EBUSY';
        if (shouldRetry && !this.#retryEAGAIN(err, buf.length, this.#len - buf.length)) {
          throw err;
        }

        sleep(BUSY_WRITE_TIMEOUT);
      }
    }

    try {
      fs.fsyncSync(this.#fd);
    } catch {
      // Skip the error. The fd might not support fsync.
    }
  }

  #callFlushCallbackOnDrain(cb) {
    this.#flushPending = true;
    const onDrain = () => {
      // Only if _fsync is false to avoid double fsync
      if (!this.#fsync) {
        try {
          fs.fsync(this.#fd, (err) => {
            this.#flushPending = false;
            cb(err);
          });
        } catch (err) {
          cb(err);
        }
      } else {
        this.#flushPending = false;
        cb();
      }
      this.off('error', onError);
    };
    const onError = (err) => {
      this.#flushPending = false;
      cb(err);
      this.off('drain', onDrain);
    };

    this.once('drain', onDrain);
    this.once('error', onError);
  }

  #flushBuffer(cb) {
    validateFunction(cb, 'cb');

    if (this.#destroyed) {
      const error = new ERR_INVALID_STATE('FastUtf8Stream is destroyed');
      if (cb) {
        cb(error);
        return;
      }

      throw error;
    }

    if (this.#minLength <= 0) {
      cb?.();
      return;
    }

    if (cb) {
      this.#callFlushCallbackOnDrain(cb);
    }

    if (this.#writing) {
      return;
    }

    if (this.#bufs.length === 0) {
      ArrayPrototypePush(this.#bufs, []);
      ArrayPrototypePush(this.#lens, 0);
    }

    this.#actualWrite();
  }

  #flushUtf8(cb) {
    validateFunction(cb, 'cb');

    if (this.#destroyed) {
      const error = new ERR_INVALID_STATE('FastUtf8Stream is destroyed');
      if (cb) {
        cb(error);
        return;
      }

      throw error;
    }

    if (this.#minLength <= 0) {
      cb?.();
      return;
    }

    if (cb) {
      this.#callFlushCallbackOnDrain(cb);
    }

    if (this.#writing) {
      return;
    }

    if (this.#bufs.length === 0) {
      ArrayPrototypePush(this.#bufs, '');
    }

    this.#actualWrite();
  }

  #writeBuffer(data) {
    if (this.#destroyed) {
      throw new ERR_INVALID_STATE('FastUtf8Stream is destroyed');
    }

    const len = this.#len + data.length;
    const bufs = this.#bufs;
    const lens = this.#lens;

    if (this.#maxLength && len > this.#maxLength) {
      this.emit('drop', data);
      return this.#len < this.#hwm;
    }

    if (
      bufs.length === 0 ||
      lens[lens.length - 1] + data.length > this.#maxWrite
    ) {
      ArrayPrototypePush(bufs, []);
      ArrayPrototypePush(lens, data.length);
    } else {
      ArrayPrototypePush(bufs[bufs.length - 1], data);
      lens[lens.length - 1] += data.length;
    }

    this.#len = len;

    if (!this.#writing && this.#len >= this.#minLength) {
      this.#actualWrite();
    }

    return this.#len < this.#hwm;
  }

  #writeUtf8(data) {
    if (this.#destroyed) {
      throw new ERR_INVALID_STATE('FastUtf8Stream is destroyed');
    }

    const len = this.#len + data.length;
    const bufs = this.#bufs;

    if (this.#maxLength && len > this.#maxLength) {
      this.emit('drop', data);
      return this.#len < this.#hwm;
    }

    if (
      bufs.length === 0 ||
      bufs[bufs.length - 1].length + data.length > this.#maxWrite
    ) {
      ArrayPrototypePush(bufs, '' + data);
    } else {
      bufs[bufs.length - 1] += data;
    }

    this.#len = len;

    if (!this.#writing && this.#len >= this.#minLength) {
      this.#actualWrite();
    }

    return this.#len < this.#hwm;
  }
}

/**
 * Release the writingBuf after fs.write n bytes data
 * @param {string | Buffer} writingBuf - currently writing buffer, usually be instance._writingBuf.
 * @param {number} len - currently buffer length, usually be instance._len.
 * @param {number} n - number of bytes fs already written
 * @returns {{writingBuf: string | Buffer, len: number}} released writingBuf and length
 */
function releaseWritingBuf(writingBuf, len, n) {
  // if Buffer.byteLength is equal to n, that means writingBuf contains no multi-byte character
  if (typeof writingBuf === 'string' && Buffer.byteLength(writingBuf) !== n) {
    // Since the fs.write callback parameter `n` means how many bytes the passed of string
    // We calculate the original string length for avoiding the multi-byte character issue
    n = Buffer.from(writingBuf).subarray(0, n).toString().length;
  }
  len = MathMax(len - n, 0);
  writingBuf = writingBuf.slice(n);
  return { writingBuf, len };
}

function mergeBuf(bufs, len) {
  if (bufs.length === 0) {
    return kEmptyBuffer;
  }

  if (bufs.length === 1) {
    return bufs[0];
  }

  return Buffer.concat(bufs, len);
}

module.exports = FastUtf8Stream;

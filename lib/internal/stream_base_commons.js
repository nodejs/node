'use strict';

const {
  Array,
  Symbol,
} = primordials;

const { Buffer } = require('buffer');
const { FastBuffer } = require('internal/buffer');
const {
  kReadBytesOrError,
  kArrayBufferOffset,
  kBytesWritten,
  kLastWriteErr,
  streamBaseState,
} = internalBinding('stream_wrap');
const { UV_EOF } = internalBinding('uv');
const {
  ErrnoException,
} = require('internal/errors');
const { owner_symbol } = require('internal/async_hooks').symbols;
const {
  kTimeout,
  reuseOrCreateUnrefTimeout,
  getTimerDuration,
} = require('internal/timers');
const { isUint8Array } = require('internal/util/types');
const { clearTimeout } = require('timers');
const { validateFunction } = require('internal/validators');

const kMaybeDestroy = Symbol('kMaybeDestroy');
const kUpdateTimer = Symbol('kUpdateTimer');
const kAfterAsyncWrite = Symbol('kAfterAsyncWrite');
const kHandle = Symbol('kHandle');
const kBoundSession = Symbol('kBoundSession');
const kSession = Symbol('kSession');

let debug = require('internal/util/debuglog').debuglog('stream', (fn) => {
  debug = fn;
});
const kBuffer = Symbol('kBuffer');
const kBufferGen = Symbol('kBufferGen');
const kBufferCb = Symbol('kBufferCb');

function onWriteComplete(status) {
  debug('onWriteComplete', status, this.error);

  const stream = this.handle[owner_symbol];

  if (status < 0) {
    const error = new ErrnoException(status, 'write', this.error);
    if (typeof this.callback === 'function') {
      return this.callback(error);
    }

    return stream.destroy(error);
  }

  if (stream.destroyed) {
    if (typeof this.callback === 'function')
      this.callback(null);
    return;
  }

  stream[kUpdateTimer]();
  stream[kAfterAsyncWrite](this);

  if (typeof this.callback === 'function')
    this.callback(null);
}

function writevGeneric(self, data, cb) {
  const handle = self[kHandle];
  const allBuffers = data.allBuffers;
  let chunks;
  if (allBuffers) {
    chunks = data;
    for (let i = 0; i < data.length; i++)
      data[i] = data[i].chunk;
  } else {
    chunks = new Array(data.length << 1);
    for (let i = 0; i < data.length; i++) {
      const entry = data[i];
      chunks[i * 2] = entry.chunk;
      chunks[i * 2 + 1] = entry.encoding;
    }
  }
  const ret = handle.writev(null, chunks, allBuffers);

  return afterWriteDispatched(handle, ret, chunks, cb);
}

function writeGeneric(self, data, encoding, cb) {
  const handle = self[kHandle];
  let ret;
  let buffer = null;

  switch (encoding) {
    case 'buffer':
      buffer = data;
      ret = handle.writeBuffer(null, data);
      break;
    case 'latin1':
    case 'binary':
      ret = handle.writeLatin1String(null, data);
      break;
    case 'utf8':
    case 'utf-8':
      ret = handle.writeUtf8String(null, data);
      break;
    case 'ascii':
      ret = handle.writeAsciiString(null, data);
      break;
    case 'ucs2':
    case 'ucs-2':
    case 'utf16le':
    case 'utf-16le':
      ret = handle.writeUcs2String(null, data);
      break;
    default:
      buffer = Buffer.from(data, encoding);
      ret = handle.writeBuffer(null, buffer);
      break;
  }

  return afterWriteDispatched(handle, ret, buffer, cb);
}

// `ret` is either a numeric error code (when the write - or its failure -
// completed synchronously and no write request was created) or the WriteWrap
// object of a dispatched write.
function afterWriteDispatched(handle, ret, buffer, cb) {
  const bytes = streamBaseState[kBytesWritten];

  if (typeof ret === 'number') {
    // The write (or its failure) completed synchronously.
    if (ret !== 0)
      cb(new ErrnoException(ret, 'write'));
    else if (typeof cb === 'function')
      cb();
    return { async: false, bytes };
  }

  const req = ret;
  const err = streamBaseState[kLastWriteErr];
  if (err !== 0) {
    cb(new ErrnoException(err, 'write', req.error));
    return { async: false, bytes };
  }

  req.handle = handle;
  req.oncomplete = onWriteComplete;
  req.callback = cb;
  req.async = true;
  req.bytes = bytes;
  // Retain the data (or chunks) being written until the write completes.
  req.buffer = buffer;

  // If the write completed synchronously inside the write call, before
  // `oncomplete` could be attached, the completion status has been recorded;
  // deliver it now.
  const status = req.writeStatus;
  if (status !== null) {
    req.writeStatus = null;
    req.oncomplete(status);
  }

  return req;
}

function onStreamRead(arrayBuffer) {
  const nread = streamBaseState[kReadBytesOrError];

  const handle = this;
  const stream = this[owner_symbol];

  stream[kUpdateTimer]();

  if (nread > 0 && !stream.destroyed) {
    let ret;
    let result;
    const userBuf = stream[kBuffer];
    if (userBuf) {
      result = (stream[kBufferCb](nread, userBuf) !== false);
      const bufGen = stream[kBufferGen];
      if (bufGen !== null) {
        const nextBuf = bufGen();
        if (isUint8Array(nextBuf))
          stream[kBuffer] = ret = nextBuf;
      }
    } else {
      const offset = streamBaseState[kArrayBufferOffset];
      const buf = new FastBuffer(arrayBuffer, offset, nread);
      result = stream.push(buf);
    }
    if (!result) {
      handle.reading = false;
      if (!stream.destroyed) {
        const err = handle.readStop();
        if (err)
          stream.destroy(new ErrnoException(err, 'read'));
      }
    }

    return ret;
  }

  if (nread === 0) {
    return;
  }

  // After seeing EOF, most streams will be closed permanently,
  // and will not deliver any more read events after this point.
  // (equivalently, it should have called readStop on itself already).
  // Some streams may be reset and explicitly started again with a call
  // to readStart, such as TTY.

  if (nread !== UV_EOF) {
    // CallJSOnreadMethod expects the return value to be a buffer.
    // Ref: https://github.com/nodejs/node/pull/34375
    stream.destroy(new ErrnoException(nread, 'read'));
    return;
  }

  // Defer this until we actually emit end
  if (stream._readableState.endEmitted) {
    if (stream[kMaybeDestroy])
      stream[kMaybeDestroy]();
  } else {
    if (stream[kMaybeDestroy])
      stream.on('end', stream[kMaybeDestroy]);

    // Push a null to signal the end of data.
    // Do it before `maybeDestroy` for correct order of events:
    // `end` -> `close`
    stream.push(null);
    stream.read(0);
  }
}

function onStreamTimeout(stream) {
  stream._onTimeout();
}

function setStreamTimeout(msecs, callback) {
  if (this.destroyed)
    return this;

  this.timeout = msecs;

  // Type checking identical to timers.enroll()
  msecs = getTimerDuration(msecs, 'msecs');

  // Attempt to clear an existing timer in both cases -
  //  even if it will be rescheduled we don't want to leak an existing timer.
  // The cleared Timeout stays referenced in this[kTimeout] so that it can be
  // reused the next time a timeout is set, e.g. on keep-alive connections.
  clearTimeout(this[kTimeout]);

  if (msecs === 0) {
    if (callback !== undefined) {
      validateFunction(callback, 'callback');
      this.removeListener('timeout', callback);
    }
  } else {
    this[kTimeout] =
      reuseOrCreateUnrefTimeout(this[kTimeout], onStreamTimeout, msecs, this);
    if (this[kSession]) this[kSession][kUpdateTimer]();
    if (this[kBoundSession]) this[kBoundSession][kUpdateTimer]();

    if (callback !== undefined) {
      validateFunction(callback, 'callback');
      this.once('timeout', callback);
    }
  }
  return this;
}

module.exports = {
  writevGeneric,
  writeGeneric,
  onStreamRead,
  kAfterAsyncWrite,
  kMaybeDestroy,
  kUpdateTimer,
  kHandle,
  kSession,
  setStreamTimeout,
  kBuffer,
  kBufferCb,
  kBufferGen,
};

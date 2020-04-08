'use strict';

const {
  Array,
  Symbol,
} = primordials;

const { Buffer } = require('buffer');
const { FastBuffer } = require('internal/buffer');
const {
  WriteWrap,
  kReadBytesOrError,
  kArrayBufferOffset,
  kBytesWritten,
  kLastWriteWasAsync,
  streamBaseState
} = internalBinding('stream_wrap');
const { UV_EOF } = internalBinding('uv');
const {
  codes: {
    ERR_INVALID_CALLBACK
  },
  errnoException
} = require('internal/errors');
const { owner_symbol } = require('internal/async_hooks').symbols;
const {
  kTimeout,
  setUnrefTimeout,
  getTimerDuration
} = require('internal/timers');
const { isUint8Array } = require('internal/util/types');
const { clearTimeout } = require('timers');

const kMaybeDestroy = Symbol('kMaybeDestroy');
const kUpdateTimer = Symbol('kUpdateTimer');
const kAfterAsyncWrite = Symbol('kAfterAsyncWrite');
const kHandle = Symbol('kHandle');
const kSession = Symbol('kSession');

const debug = require('internal/util/debuglog').debuglog('stream');
const kBuffer = Symbol('kBuffer');
const kBufferGen = Symbol('kBufferGen');
const kBufferCb = Symbol('kBufferCb');

function handleWriteReq(req, data, encoding) {
  const { handle } = req;

  switch (encoding) {
    case 'buffer':
    {
      const ret = handle.writeBuffer(req, data);
      if (streamBaseState[kLastWriteWasAsync])
        req.buffer = data;
      return ret;
    }
    case 'latin1':
    case 'binary':
      return handle.writeLatin1String(req, data);
    case 'utf8':
    case 'utf-8':
      return handle.writeUtf8String(req, data);
    case 'ascii':
      return handle.writeAsciiString(req, data);
    case 'ucs2':
    case 'ucs-2':
    case 'utf16le':
    case 'utf-16le':
      return handle.writeUcs2String(req, data);
    default:
    {
      const buffer = Buffer.from(data, encoding);
      const ret = handle.writeBuffer(req, buffer);
      if (streamBaseState[kLastWriteWasAsync])
        req.buffer = buffer;
      return ret;
    }
  }
}

function onWriteComplete(status) {
  debug('onWriteComplete', status, this.error);

  const stream = this.handle[owner_symbol];

  if (stream.destroyed) {
    if (typeof this.callback === 'function')
      this.callback(null);
    return;
  }

  if (status < 0) {
    const ex = errnoException(status, 'write', this.error);
    stream.destroy(ex, this.callback);
    return;
  }

  stream[kUpdateTimer]();
  stream[kAfterAsyncWrite](this);

  if (typeof this.callback === 'function')
    this.callback(null);
}

function createWriteWrap(handle) {
  const req = new WriteWrap();

  req.handle = handle;
  req.oncomplete = onWriteComplete;
  req.async = false;
  req.bytes = 0;
  req.buffer = null;

  return req;
}

function writevGeneric(self, data, cb) {
  const req = createWriteWrap(self[kHandle]);
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
  const err = req.handle.writev(req, chunks, allBuffers);

  // Retain chunks
  if (err === 0) req._chunks = chunks;

  afterWriteDispatched(self, req, err, cb);
  return req;
}

function writeGeneric(self, data, encoding, cb) {
  const req = createWriteWrap(self[kHandle]);
  const err = handleWriteReq(req, data, encoding);

  afterWriteDispatched(self, req, err, cb);
  return req;
}

function afterWriteDispatched(self, req, err, cb) {
  req.bytes = streamBaseState[kBytesWritten];
  req.async = !!streamBaseState[kLastWriteWasAsync];

  if (err !== 0)
    return self.destroy(errnoException(err, 'write', req.error), cb);

  if (!req.async) {
    cb();
  } else {
    req.callback = cb;
  }
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
          stream.destroy(errnoException(err, 'read'));
      }
    }

    return ret;
  }

  if (nread === 0) {
    return;
  }

  if (nread !== UV_EOF) {
    return stream.destroy(errnoException(nread, 'read'));
  }

  // Defer this until we actually emit end
  if (stream._readableState.endEmitted) {
    if (stream[kMaybeDestroy])
      stream[kMaybeDestroy]();
  } else {
    if (stream[kMaybeDestroy])
      stream.on('end', stream[kMaybeDestroy]);

    // TODO(ronag): Without this `readStop`, `onStreamRead`
    // will be called once more (i.e. after Readable.ended)
    // on Windows causing a ECONNRESET, failing the
    // test-https-truncate test.
    if (handle.readStop) {
      const err = handle.readStop();
      if (err)
        return stream.destroy(errnoException(err, 'read'));
    }

    // Push a null to signal the end of data.
    // Do it before `maybeDestroy` for correct order of events:
    // `end` -> `close`
    stream.push(null);
    stream.read(0);
  }
}

function setStreamTimeout(msecs, callback) {
  if (this.destroyed)
    return this;

  this.timeout = msecs;

  // Type checking identical to timers.enroll()
  msecs = getTimerDuration(msecs, 'msecs');

  // Attempt to clear an existing timer in both cases -
  //  even if it will be rescheduled we don't want to leak an existing timer.
  clearTimeout(this[kTimeout]);

  if (msecs === 0) {
    if (callback !== undefined) {
      if (typeof callback !== 'function')
        throw new ERR_INVALID_CALLBACK(callback);
      this.removeListener('timeout', callback);
    }
  } else {
    this[kTimeout] = setUnrefTimeout(this._onTimeout.bind(this), msecs);
    if (this[kSession]) this[kSession][kUpdateTimer]();

    if (callback !== undefined) {
      if (typeof callback !== 'function')
        throw new ERR_INVALID_CALLBACK(callback);
      this.once('timeout', callback);
    }
  }
  return this;
}

module.exports = {
  createWriteWrap,
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
  kBufferGen
};

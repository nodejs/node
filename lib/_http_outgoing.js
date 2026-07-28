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
  Array,
  ArrayIsArray,
  MathFloor,
  ObjectDefineProperty,
  ObjectHasOwn,
  ObjectKeys,
  ObjectSetPrototypeOf,
  ObjectValues,
  SafeMap,
  SafeSet,
  Symbol,
} = primordials;

const { getDefaultHighWaterMark } = require('internal/streams/state');
const assert = require('internal/assert');
const EE = require('events');
const Stream = require('stream');
const { kOutHeaders, utcDate, kNeedDrain } = require('internal/http');
const { Buffer } = require('buffer');
const {
  _checkIsHttpToken: checkIsHttpToken,
  _checkInvalidHeaderChar: checkInvalidHeaderChar,
  chunkExpression: RE_TE_CHUNKED,
  isLenient,
} = require('_http_common');
const {
  defaultTriggerAsyncIdScope,
  symbols: { async_id_symbol },
} = require('internal/async_hooks');
const {
  codes: {
    ERR_HTTP_BODY_NOT_ALLOWED,
    ERR_HTTP_CONTENT_LENGTH_MISMATCH,
    ERR_HTTP_HEADERS_SENT,
    ERR_HTTP_INVALID_HEADER_VALUE,
    ERR_HTTP_TRAILER_INVALID,
    ERR_INVALID_ARG_TYPE,
    ERR_INVALID_ARG_VALUE,
    ERR_INVALID_CHAR,
    ERR_INVALID_HTTP_TOKEN,
    ERR_METHOD_NOT_IMPLEMENTED,
    ERR_STREAM_ALREADY_FINISHED,
    ERR_STREAM_CANNOT_PIPE,
    ERR_STREAM_DESTROYED,
    ERR_STREAM_NULL_VALUES,
    ERR_STREAM_WRITE_AFTER_END,
  },
  hideStackFrames,
} = require('internal/errors');
const { validateString } = require('internal/validators');
const { assignFunctionName } = require('internal/util');
const { isUint8Array } = require('internal/util/types');
const { writeGeneric } = require('internal/stream_base_commons');

let debug = require('internal/util/debuglog').debuglog('http', (fn) => {
  debug = fn;
});

const kCorked = Symbol('corked');
const kLenientValidation = Symbol('kLenientValidation');
const kSocket = Symbol('kSocket');
const kChunkedBuffer = Symbol('kChunkedBuffer');
const kChunkedLength = Symbol('kChunkedLength');
const kUniqueHeaders = Symbol('kUniqueHeaders');
const kBytesWritten = Symbol('kBytesWritten');
const kErrored = Symbol('errored');
const kHighWaterMark = Symbol('kHighWaterMark');
const kRejectNonStandardBodyWrites = Symbol('kRejectNonStandardBodyWrites');
const kServerFinishHook = Symbol('kServerFinishHook');

const nop = () => {};

const RE_CONN_CLOSE = /(?:^|\W)close(?:$|\W)/i;

// Bodies larger than this are never folded into a combined headers+body
// buffer/string: the copy is slower than a separate write()
// (benchmark/http/end-vs-write-end.js).
const kMaxSingleShotBody = 16 * 1024;

// Cached "Date: ...\r\n" line - utcDate() already caches the date string for
// up to 1s; avoid re-concatenating the "Date: " prefix on every response.
let cachedDateHeader = '';
let cachedDateValue = '';
function dateHeaderLine() {
  const d = utcDate();
  if (d !== cachedDateValue) {
    cachedDateValue = d;
    cachedDateHeader = 'Date: ' + d + '\r\n';
  }
  return cachedDateHeader;
}

// Cached Connection/Keep-Alive suffix for the common keep-alive shape.
// Keyed by timeoutSec and maxReq (usually constant for a server).
const keepAliveSuffixCache = new SafeMap();
function keepAliveSuffix(timeoutSec, maxReq) {
  const key = timeoutSec * 0x100000000 + (maxReq | 0);
  let s = keepAliveSuffixCache.get(key);
  if (s !== undefined) return s;
  if (maxReq > 0)
    s = `Connection: keep-alive\r\nKeep-Alive: timeout=${timeoutSec}, max=${maxReq}\r\n`;
  else
    s = `Connection: keep-alive\r\nKeep-Alive: timeout=${timeoutSec}\r\n`;
  // Bound the cache; servers almost never change these at runtime.
  if (keepAliveSuffixCache.size < 16)
    keepAliveSuffixCache.set(key, s);
  return s;
}


// isCookieField performs a case-insensitive comparison of a provided string
// against the word "cookie." As of V8 6.6 this is faster than handrolling or
// using a case-insensitive RegExp.
function isCookieField(s) {
  return s.length === 6 && s.toLowerCase() === 'cookie';
}

function isContentDispositionField(s) {
  return s.length === 19 && s.toLowerCase() === 'content-disposition';
}

function OutgoingMessage(options) {
  Stream.call(this);

  // Queue that holds all currently pending data, until the response will be
  // assigned to the socket (until it will its turn in the HTTP pipeline).
  this.outputData = [];

  // `outputSize` is an approximate measure of how much data is queued on this
  // response. `_onPendingData` will be invoked to update similar global
  // per-connection counter. That counter will be used to pause/unpause the
  // TCP socket and HTTP Parser and thus handle the backpressure.
  this.outputSize = 0;

  this.writable = true;
  this.destroyed = false;

  this._last = false;
  this.chunkedEncoding = false;
  this.shouldKeepAlive = true;
  this.maxRequestsOnConnectionReached = false;
  this._defaultKeepAlive = true;
  this.useChunkedEncodingByDefault = true;
  this.sendDate = false;
  this._removedConnection = false;
  this._removedContLen = false;
  this._removedTE = false;

  this.strictContentLength = false;
  this[kBytesWritten] = 0;
  this._contentLength = null;
  this._hasBody = true;
  this._trailer = '';
  this[kNeedDrain] = false;

  this.finished = false;
  this._headerSent = false;
  this[kCorked] = 0;
  this[kChunkedBuffer] = [];
  this[kChunkedLength] = 0;
  this._closed = false;
  this[kLenientValidation] = undefined;

  this[kSocket] = null;
  this._header = null;
  this[kOutHeaders] = null;

  this._keepAliveTimeout = 0;

  this._onPendingData = nop;

  this[kErrored] = null;
  this[kHighWaterMark] = options?.highWaterMark ?? getDefaultHighWaterMark();
  this[kRejectNonStandardBodyWrites] = options?.rejectNonStandardBodyWrites ?? false;
}
ObjectSetPrototypeOf(OutgoingMessage.prototype, Stream.prototype);
ObjectSetPrototypeOf(OutgoingMessage, Stream);

// Check if lenient header validation should be used.
// For ClientRequest: checks this.httpValidation or this.insecureHTTPParser
// For ServerResponse: checks the server's httpValidation or insecureHTTPParser
// Falls back to global --insecure-http-parser flag.
// The answer is invariant for the lifetime of the message (the options are
// fixed at construction time), but this runs on every setHeader/appendHeader
// call and once per stored header block, so the multi-step lookup chain is
// resolved once per message and cached.
OutgoingMessage.prototype._isLenientHeaderValidation = function() {
  const cached = this[kLenientValidation];
  if (cached !== undefined)
    return cached;
  return this[kLenientValidation] = lenientHeaderValidation(this);
};

function lenientHeaderValidation(msg) {
  // New httpValidation option takes priority (ClientRequest case)
  if (msg.httpValidation !== undefined) {
    return msg.httpValidation !== 'strict';
  }
  // ServerResponse: check server's httpValidation option
  const serverHttpValidation = msg.req?.socket?.server?.httpValidation;
  if (serverHttpValidation !== undefined) {
    return serverHttpValidation !== 'strict';
  }
  // Legacy insecureHTTPParser - ClientRequest has it directly
  if (typeof msg.insecureHTTPParser === 'boolean') {
    return msg.insecureHTTPParser;
  }
  // ServerResponse can access via req.socket.server
  const serverOption = msg.req?.socket?.server?.insecureHTTPParser;
  if (typeof serverOption === 'boolean') {
    return serverOption;
  }
  // Fall back to global option
  return isLenient();
}

ObjectDefineProperty(OutgoingMessage.prototype, 'errored', {
  __proto__: null,
  get() {
    return this[kErrored];
  },
});

ObjectDefineProperty(OutgoingMessage.prototype, 'closed', {
  __proto__: null,
  get() {
    return this._closed;
  },
});

ObjectDefineProperty(OutgoingMessage.prototype, 'writableFinished', {
  __proto__: null,
  get() {
    return (
      this.finished &&
      this.outputSize === 0 &&
      (!this[kSocket] || this[kSocket].writableLength === 0)
    );
  },
});

ObjectDefineProperty(OutgoingMessage.prototype, 'writableObjectMode', {
  __proto__: null,
  get() {
    return false;
  },
});

ObjectDefineProperty(OutgoingMessage.prototype, 'writableLength', {
  __proto__: null,
  get() {
    return this.outputSize + this[kChunkedLength] + (this[kSocket] ? this[kSocket].writableLength : 0);
  },
});

ObjectDefineProperty(OutgoingMessage.prototype, 'writableHighWaterMark', {
  __proto__: null,
  get() {
    return this[kSocket] ? this[kSocket].writableHighWaterMark : this[kHighWaterMark];
  },
});

ObjectDefineProperty(OutgoingMessage.prototype, 'writableCorked', {
  __proto__: null,
  get() {
    return this[kCorked];
  },
});

ObjectDefineProperty(OutgoingMessage.prototype, 'connection', {
  __proto__: null,
  get: function() {
    return this[kSocket];
  },
  set: function(val) {
    this.socket = val;
  },
});

ObjectDefineProperty(OutgoingMessage.prototype, 'socket', {
  __proto__: null,
  get: function() {
    return this[kSocket];
  },
  set: function(val) {
    for (let n = 0; n < this[kCorked]; n++) {
      val?.cork();
      this[kSocket]?.uncork();
    }
    this[kSocket] = val;
  },
});

OutgoingMessage.prototype._renderHeaders = function _renderHeaders() {
  if (this._header) {
    throw new ERR_HTTP_HEADERS_SENT('render');
  }

  const headersMap = this[kOutHeaders];
  const headers = {};

  if (headersMap !== null) {
    const keys = ObjectKeys(headersMap);
    // Retain for(;;) loop for performance reasons
    // Refs: https://github.com/nodejs/node/pull/30958
    for (let i = 0, l = keys.length; i < l; i++) {
      const key = keys[i];
      headers[headersMap[key][0]] = headersMap[key][1];
    }
  }
  return headers;
};

OutgoingMessage.prototype.cork = function cork() {
  this[kCorked]++;
  if (this[kSocket]) {
    this[kSocket].cork();
  }
};

OutgoingMessage.prototype.uncork = function uncork() {
  this[kCorked]--;
  if (this[kSocket]) {
    this[kSocket].uncork();
  }

  if (this[kCorked] || this[kChunkedBuffer].length === 0) {
    return;
  }

  const len = this[kChunkedLength];
  const buf = this[kChunkedBuffer];

  assert(this.chunkedEncoding);

  let callbacks;
  this._send(len.toString(16), 'latin1', null);
  this._send(crlf_buf, null, null);
  for (let n = 0; n < buf.length; n += 3) {
    this._send(buf[n + 0], buf[n + 1], null);
    if (buf[n + 2]) {
      callbacks ??= [];
      callbacks.push(buf[n + 2]);
    }
  }
  // The runner is a shared top-level function so flushing a corked
  // chunked buffer does not allocate a fresh closure environment per
  // flush.
  this._send(crlf_buf, null, callbacks?.length ?
    runChunkCallbacks.bind(undefined, callbacks) :
    null);

  this[kChunkedBuffer].length = 0;
  this[kChunkedLength] = 0;

  // If we had a pending drain and flushed all data, emit the drain event.
  if (this[kNeedDrain] && this.writableLength === 0) {
    this[kNeedDrain] = false;
    this.emit('drain');
  }
};

OutgoingMessage.prototype.setTimeout = function setTimeout(msecs, callback) {

  if (callback) {
    this.on('timeout', callback);
  }

  if (!this[kSocket]) {
    this.once('socket', function socketSetTimeoutOnConnect(socket) {
      socket.setTimeout(msecs);
    });
  } else {
    this[kSocket].setTimeout(msecs);
  }
  return this;
};


// It's possible that the socket will be destroyed, and removed from
// any messages, before ever calling this.  In that case, just skip
// it, since something else is destroying this connection anyway.
OutgoingMessage.prototype.destroy = function destroy(error) {
  if (this.destroyed) {
    return this;
  }
  this.destroyed = true;

  this[kErrored] = error;

  if (this[kSocket]) {
    this[kSocket].destroy(error);
  } else {
    process.nextTick(emitDestroyNT, this);
  }

  return this;
};

function emitDestroyNT(self) {
  if (!self._closed) {
    self._closed = true;
    self.emit('close');
  }
}


// Queue an unsent header block ahead of the body write. Used for Buffer
// headers (native builder) and for string headers that cannot share the
// body's encoding (high-byte latin1 / non-utf8 body encodings).
function queueHeaderWrite(msg, header, encoding) {
  msg.outputData.unshift({
    data: header,
    encoding,
    callback: null,
  });
  const len = header.length;
  msg.outputSize += len;
  msg._onPendingData(len);
}

// This abstract either writing directly to the socket or buffering it.
OutgoingMessage.prototype._send = function _send(data, encoding, callback, byteLength) {
  // This is a shameful hack to get the headers and first body chunk onto
  // the same packet. Future versions of Node are going to take care of
  // this at a lower level and in a more general way.
  let header;
  if (!this._headerSent && (header = this._header) !== null) {
    // `this._header` can be null if OutgoingMessage is used without a proper Socket
    // See: /test/parallel/test-http-outgoing-message-inheritance.js
    //
    // Native builder stores _header as a Buffer (server path). Legacy and
    // ClientRequest keep a latin1 string. Never Buffer.concat: queue the
    // header as its own write when it cannot share the body encoding.
    if (isUint8Array(header)) {
      // Native Buffer header. Prefer a single write when the body encoding
      // can share the stream; otherwise queue the header separately.
      if (typeof data === 'string' && data.length === 0) {
        data = header;
        encoding = 'buffer';
      } else if (typeof data === 'string' && data.length > 0 &&
                 (encoding === 'latin1' || encoding === 'binary')) {
        data = header.toString('latin1') + data;
        encoding = 'latin1';
      } else if (typeof data === 'string' && data.length > 0 &&
                 (encoding === 'utf8' || encoding === 'utf-8' || !encoding)) {
        let ascii = true;
        for (let i = 0; i < header.byteLength; i++) {
          if (header[i] > 127) {
            ascii = false;
            break;
          }
        }
        if (ascii) {
          // latin1 decode of ASCII is identity; one utf8 write with body.
          data = header.toString('latin1') + data;
        } else {
          queueHeaderWrite(this, header, 'buffer');
        }
      } else if (isUint8Array(data) && data.byteLength > 0) {
        queueHeaderWrite(this, header, 'buffer');
      } else {
        queueHeaderWrite(this, header, 'buffer');
      }
    } else if (typeof data === 'string' && data.length > 0 &&
        (encoding === 'latin1' || encoding === 'binary')) {
      // Small bodies: one combined write. Large bodies: avoid the O(n) string
      // copy (end-vs-write-end.js) with a corked dual write when the socket
      // is ready; otherwise queue the header and fall through.
      if (data.length > kMaxSingleShotBody) {
        const conn = this[kSocket];
        if (conn && conn._httpMessage === this && conn.writable) {
          this._headerSent = true;
          if (this.outputData.length) this._flushOutput(conn);
          if (!conn.writableCorked) conn.cork();
          conn.write(header, 'latin1');
          const ret = conn.write(data, 'latin1', callback);
          conn._writableState.corked = 1;
          conn.uncork();
          return ret;
        }
        queueHeaderWrite(this, header, 'latin1');
      } else {
        data = header + data;
        encoding = 'latin1';
      }
    } else if (typeof data === 'string' && data.length > 0 &&
               (encoding === 'utf8' || encoding === 'utf-8' || !encoding)) {
      // ASCII headers can safely share one UTF-8 write with a small body.
      // Large bodies: corked dual write avoids the combined-string copy.
      if (data.length > kMaxSingleShotBody) {
        const conn = this[kSocket];
        if (conn && conn._httpMessage === this && conn.writable) {
          this._headerSent = true;
          if (this.outputData.length) this._flushOutput(conn);
          if (!conn.writableCorked) conn.cork();
          conn.write(header, 'latin1');
          const ret = conn.write(data, encoding || 'utf8', callback);
          conn._writableState.corked = 1;
          conn.uncork();
          return ret;
        }
        queueHeaderWrite(this, header, 'latin1');
      } else {
        let ascii = true;
        for (let i = 0; i < header.length; i++) {
          if (header.charCodeAt(i) > 127) {
            ascii = false;
            break;
          }
        }
        if (ascii) {
          data = header + data;
        } else {
          queueHeaderWrite(this, header, 'latin1');
        }
      }
    } else if (typeof data === 'string' && data.length === 0) {
      // Header-only flush (end without body, Expect: 100-continue, etc.).
      data = header;
      encoding = 'latin1';
    } else {
      // Buffers / hex / utf16le / base64: separate header write.
      queueHeaderWrite(this, header, 'latin1');
    }
    this._headerSent = true;
  }
  return this._writeRaw(data, encoding, callback, byteLength);
};

OutgoingMessage.prototype._writeRaw = _writeRaw;
function _writeRaw(data, encoding, callback, size) {
  const conn = this[kSocket];
  if (conn?.destroyed) {
    // The socket was destroyed. If we're still trying to write to it,
    // then we haven't gotten the 'close' event yet.
    return false;
  }

  if (typeof encoding === 'function') {
    callback = encoding;
    encoding = null;
  }

  if (conn && conn._httpMessage === this && conn.writable) {
    // There might be pending data in the this.output buffer.
    if (this.outputData.length) {
      this._flushOutput(conn);
    }
    // Directly write to socket.
    return conn.write(data, encoding, callback);
  }
  // Buffer, as long as we're not destroyed.
  this.outputData.push({ data, encoding, callback });
  this.outputSize += data.length;
  this._onPendingData(data.length);
  return this.outputSize < this[kHighWaterMark];
}


OutgoingMessage.prototype._storeHeader = _storeHeader;
function _storeHeader(firstLine, headers) {
  // Keep the legacy JS header builder for writeHead() / _implicitHeader().
  // The C++ builder is used by tryFastEnd() for the single-shot
  // res.end(body) path. Using C++ for every writeHead() was a net
  // regression on small-header (simple.js) and many-header (headers.js)
  // workloads: flatten + V8/C++ round-trips cost more than string concat.
  legacyStoreHeader(this, firstLine, headers);
}

// Fast writeHead({ k: string, ... }) path. Returns false when any value is
// not a plain string (arrays, Buffers) so the caller can fall back.
const headerBlockCache = new SafeMap();
const kHeaderBlockCacheMax = 64;

// Per-headerBuf map of body -> complete wire-format response Buffer.
// Attached to cached header Buffers so date-rollover naturally drops entries
// when the header block is rebuilt. Concurrent writes of the same Buffer to
// different sockets are safe (read-only payload).
const kFullBodyMap = Symbol('kFullBodyMap');
const kChunkedFullBodyMap = Symbol('kChunkedFullBodyMap');
const kFullBodyMapMax = 8;


function tryFastStoreHeaderObject(self, firstLine, headers) {
  const lenient = self._isLenientHeaderValidation();

  // Build a cache key. Hot path: two string headers (simple.js) uses a
  // short key without a full flags dump when defaults are intact.
  let k0, v0, k1, v1;
  let pairCount = 0;
  for (const key in headers) {
    if (!ObjectHasOwn(headers, key)) continue;
    const value = headers[key];
    if (typeof value !== 'string') return false;
    if (pairCount === 0) {
      k0 = key;
      v0 = value;
    } else if (pairCount === 1) {
      k1 = key;
      v1 = value;
    } else {
      // Fall through to general key below.
      pairCount = -1;
      break;
    }
    pairCount++;
  }
  let cacheKey;
  if (pairCount === 2 &&
      firstLine === 'HTTP/1.1 200 OK\r\n' &&
      self.sendDate && self.shouldKeepAlive && self._defaultKeepAlive &&
      self._hasBody && self.useChunkedEncodingByDefault &&
      !self.maxRequestsOnConnectionReached &&
      !self._removedConnection && !self._removedContLen && !self._removedTE &&
      !self.agent) {
    // simple.js-shaped response: two headers, stock keep-alive server.
    // (maxRequestsOnConnectionReached is always false on this branch.)
    cacheKey = k0 + '\0' + v0 + '\0' + k1 + '\0' + v1 +
      '\0' + (self._keepAliveTimeout | 0);
  } else if (pairCount === 1 &&
             firstLine === 'HTTP/1.1 200 OK\r\n' &&
             !self.maxRequestsOnConnectionReached &&
             self._hasBody) {
    cacheKey = k0 + '\0' + v0 + '\0' + (self._keepAliveTimeout | 0) +
      '\0' + (self.shouldKeepAlive ? '1' : '0') +
      '\0' + (self._defaultKeepAlive ? '1' : '0') +
      '\0' + (self.useChunkedEncodingByDefault ? '1' : '0');
  } else {
    // General key.
    if (pairCount === -1) {
      // Restart enumeration for the general path.
      cacheKey = firstLine;
      for (const key in headers) {
        if (!ObjectHasOwn(headers, key)) continue;
        const value = headers[key];
        if (typeof value !== 'string') return false;
        cacheKey += '\n' + key + '\n' + value;
      }
    } else {
      cacheKey = firstLine;
      if (pairCount >= 1) cacheKey += '\n' + k0 + '\n' + v0;
      if (pairCount >= 2) cacheKey += '\n' + k1 + '\n' + v1;
    }
    cacheKey += '\n' + (self.sendDate ? '1' : '0') +
      (self.shouldKeepAlive ? '1' : '0') +
      (self.maxRequestsOnConnectionReached ? '1' : '0') +
      (self._defaultKeepAlive ? '1' : '0') +
      (self._hasBody ? '1' : '0') +
      (self.useChunkedEncodingByDefault ? '1' : '0') +
      (self._removedConnection ? '1' : '0') +
      (self._removedContLen ? '1' : '0') +
      (self._removedTE ? '1' : '0') +
      (self.agent ? '1' : '0') +
      '\n' + (self._keepAliveTimeout | 0) +
      '\n' + (self._maxRequestsPerSocket | 0) +
      '\n' + (self.statusCode | 0);
  }

  const date = self.sendDate ? utcDate() : '';
  const cached = headerBlockCache.get(cacheKey);
  if (cached !== undefined && cached.date === date) {
    // Replay side effects from the cached build. Prefer Buffer header so
    // the flush path can write without re-encoding the header block.
    self._header = cached.headerBuf || cached.header;
    self._headerSent = false;
    self.chunkedEncoding = cached.chunkedEncoding;
    self._last = cached._last;
    self.shouldKeepAlive = cached.shouldKeepAlive;
    self._contentLength = cached._contentLength;
    self._removedConnection = cached._removedConnection;
    self._removedContLen = cached._removedContLen;
    self._removedTE = cached._removedTE;
    self._defaultKeepAlive = cached._defaultKeepAlive;
    if (cached.expect) self._send('');
    return true;
  }

  let header = firstLine;
  let connection = false;
  let contLen = false;
  let te = false;
  let dateSeen = false;
  let expect = false;
  let trailer = false;

  for (const key in headers) {
    if (!ObjectHasOwn(headers, key)) continue;
    const value = headers[key];
    // Typeof already checked above when building cacheKey, but re-check for
    // safety if the object was mutated (shouldn't happen).
    if (typeof value !== 'string') return false;

    validateHeaderName(key);
    validateHeaderValue(key, value, lenient);
    header += key + ': ' + value + '\r\n';

    const len = key.length;
    if (len !== 4 && len !== 6 && len !== 7 && len !== 10 &&
        len !== 14 && len !== 17) {
      continue;
    }
    const lower = key.toLowerCase();
    switch (lower) {
      case 'connection':
        connection = true;
        self._removedConnection = false;
        if (RE_CONN_CLOSE.test(value))
          self._last = true;
        else
          self.shouldKeepAlive = true;
        break;
      case 'transfer-encoding':
        te = true;
        self._removedTE = false;
        if (RE_TE_CHUNKED.test(value))
          self.chunkedEncoding = true;
        break;
      case 'content-length':
        contLen = true;
        self._contentLength = +value;
        self._removedContLen = false;
        break;
      case 'date':
        dateSeen = true;
        break;
      case 'expect':
        expect = true;
        break;
      case 'trailer':
        trailer = true;
        break;
      case 'keep-alive':
        self._defaultKeepAlive = false;
        break;
      default:
        break;
    }
  }

  if (self.sendDate && !dateSeen) {
    header += dateHeaderLine();
  }

  if (self.chunkedEncoding && (self.statusCode === 204 ||
                               self.statusCode === 304)) {
    debug(self.statusCode + ' response should not use chunked encoding,' +
          ' closing connection.');
    self.chunkedEncoding = false;
    self.shouldKeepAlive = false;
  }

  if (self._removedConnection) {
    self._last = !self.shouldKeepAlive;
  } else if (!connection) {
    const shouldSendKeepAlive = self.shouldKeepAlive &&
        (contLen || self.useChunkedEncodingByDefault || self.agent);
    if (shouldSendKeepAlive && self.maxRequestsOnConnectionReached) {
      header += 'Connection: close\r\n';
    } else if (shouldSendKeepAlive) {
      const keepAliveTimeout = self._keepAliveTimeout;
      if (keepAliveTimeout && self._defaultKeepAlive) {
        header += keepAliveSuffix(
          MathFloor(keepAliveTimeout / 1000),
          self._maxRequestsPerSocket | 0,
        );
      } else {
        header += 'Connection: keep-alive\r\n';
      }
    } else {
      self._last = true;
      header += 'Connection: close\r\n';
    }
  }

  let contentLength;
  if (!contLen && !te) {
    if (!self._hasBody) {
      self.chunkedEncoding = false;
    } else if (!self.useChunkedEncodingByDefault) {
      self._last = true;
    } else if (!trailer &&
               !self._removedContLen &&
               typeof (contentLength = self._contentLength) === 'number') {
      header += 'Content-Length: ' + contentLength + '\r\n';
    } else if (!self._removedTE) {
      header += 'Transfer-Encoding: chunked\r\n';
      self.chunkedEncoding = true;
    } else {
      debug('Both Content-Length and Transfer-Encoding are removed');
      self._last = true;
    }
  }

  if (self.chunkedEncoding !== true && trailer) {
    throw new ERR_HTTP_TRAILER_INVALID();
  }

  header += '\r\n';
  self._header = header;
  self._headerSent = false;

  // Cache for subsequent identical writeHead() calls this second.
  if (headerBlockCache.size >= kHeaderBlockCacheMax) {
    // Drop an arbitrary old entry (Map iterates insertion order).
    const first = headerBlockCache.keys().next().value;
    headerBlockCache.delete(first);
  }
  // Pre-encode the header block once; keep-alive requests reuse the Buffer
  // so end() only encodes the body (or copies a Buffer body).
  const headerBuf = Buffer.from(header, 'latin1');
  headerBlockCache.set(cacheKey, {
    date,
    header,
    headerBuf,
    chunkedEncoding: self.chunkedEncoding,
    _last: self._last,
    shouldKeepAlive: self.shouldKeepAlive,
    _contentLength: self._contentLength,
    _removedConnection: self._removedConnection,
    _removedContLen: self._removedContLen,
    _removedTE: self._removedTE,
    _defaultKeepAlive: self._defaultKeepAlive,
    expect,
  });
  // Prefer Buffer form on the message for the flush path.
  self._header = headerBuf;

  if (expect) self._send('');
  return true;
}


function legacyStoreHeader(self, firstLine, headers) {
  // firstLine in the case of request is: 'GET /index.html HTTP/1.1\r\n'
  // in the case of response it is: 'HTTP/1.1 200 OK\r\n'

  // Hot path: writeHead(status, { name: string, ... }) with only string
  // values (benchmark/http/simple.js). Avoids the state object and the
  // processHeader/storeHeader call chain.
  if (headers &&
      headers !== self[kOutHeaders] &&
      !ArrayIsArray(headers) &&
      tryFastStoreHeaderObject(self, firstLine, headers)) {
    return;
  }

  const state = {
    connection: false,
    contLen: false,
    te: false,
    date: false,
    expect: false,
    trailer: false,
    header: firstLine,
  };
  const lenient = self._isLenientHeaderValidation();

  if (headers) {
    if (headers === self[kOutHeaders]) {
      for (const key in headers) {
        const entry = headers[key];
        processHeader(self, state, entry[0], entry[1], false, lenient);
      }
    } else if (ArrayIsArray(headers)) {
      // The length is hoisted into a local because the engine cannot fold
      // the reload across the processHeader calls; the array is never
      // mutated while this loop runs.
      const headersLength = headers.length;
      if (headersLength && ArrayIsArray(headers[0])) {
        for (let i = 0; i < headersLength; i++) {
          const entry = headers[i];
          processHeader(self, state, entry[0], entry[1], true, lenient);
        }
      } else {
        if (headersLength % 2 !== 0) {
          throw new ERR_INVALID_ARG_VALUE('headers', headers);
        }

        for (let n = 0; n < headersLength; n += 2) {
          processHeader(self, state, headers[n + 0], headers[n + 1], true, lenient);
        }
      }
    } else {
      for (const key in headers) {
        if (ObjectHasOwn(headers, key)) {
          processHeader(self, state, key, headers[key], true, lenient);
        }
      }
    }
  }

  let { header } = state;

  // Date header
  if (self.sendDate && !state.date) {
    header += dateHeaderLine();
  }

  // Force the connection to close when the response is a 204 No Content or
  // a 304 Not Modified and the user has set a "Transfer-Encoding: chunked"
  // header.
  //
  // RFC 2616 mandates that 204 and 304 responses MUST NOT have a body but
  // node.js used to send out a zero chunk anyway to accommodate clients
  // that don't have special handling for those responses.
  //
  // It was pointed out that this might confuse reverse proxies to the point
  // of creating security liabilities, so suppress the zero chunk and force
  // the connection to close.
  if (self.chunkedEncoding && (self.statusCode === 204 ||
                               self.statusCode === 304)) {
    debug(self.statusCode + ' response should not use chunked encoding,' +
          ' closing connection.');
    self.chunkedEncoding = false;
    self.shouldKeepAlive = false;
  }

  // keep-alive logic
  if (self._removedConnection) {
    // shouldKeepAlive is generally true for HTTP/1.1. In that common case,
    // even if the connection header isn't sent, we still persist by default.
    self._last = !self.shouldKeepAlive;
  } else if (!state.connection) {
    const shouldSendKeepAlive = self.shouldKeepAlive &&
        (state.contLen || self.useChunkedEncodingByDefault || self.agent);
    if (shouldSendKeepAlive && self.maxRequestsOnConnectionReached) {
      header += 'Connection: close\r\n';
    } else if (shouldSendKeepAlive) {
      header += 'Connection: keep-alive\r\n';
      const keepAliveTimeout = self._keepAliveTimeout;
      if (keepAliveTimeout && self._defaultKeepAlive) {
        const timeoutSeconds = MathFloor(keepAliveTimeout / 1000);
        const maxRequestsPerSocket = self._maxRequestsPerSocket;
        let max = '';
        if (~~maxRequestsPerSocket > 0) {
          max = `, max=${maxRequestsPerSocket}`;
        }
        header += `Keep-Alive: timeout=${timeoutSeconds}${max}\r\n`;
      }
    } else {
      self._last = true;
      header += 'Connection: close\r\n';
    }
  }

  let contentLength;
  if (!state.contLen && !state.te) {
    if (!self._hasBody) {
      // Make sure we don't end the 0\r\n\r\n at the end of the message.
      self.chunkedEncoding = false;
    } else if (!self.useChunkedEncodingByDefault) {
      self._last = true;
    } else if (!state.trailer &&
               !self._removedContLen &&
               typeof (contentLength = self._contentLength) === 'number') {
      header += 'Content-Length: ' + contentLength + '\r\n';
    } else if (!self._removedTE) {
      header += 'Transfer-Encoding: chunked\r\n';
      self.chunkedEncoding = true;
    } else {
      // We should only be able to get here if both Content-Length and
      // Transfer-Encoding are removed by the user.
      // See: test/parallel/test-http-remove-header-stays-removed.js
      debug('Both Content-Length and Transfer-Encoding are removed');

      // We can't keep alive in this case, because with no header info the body
      // is defined as all data until the connection is closed.
      self._last = true;
    }
  }

  // Test non-chunked message does not have trailer header set,
  // message will be terminated by the first empty line after the
  // header fields, regardless of the header fields present in the
  // message, and thus cannot contain a message body or 'trailers'.
  if (self.chunkedEncoding !== true && state.trailer) {
    throw new ERR_HTTP_TRAILER_INVALID();
  }

  self._header = header + '\r\n';
  self._headerSent = false;

  // Wait until the first body chunk, or close(), is sent to flush,
  // UNLESS we're sending Expect: 100-continue.
  if (state.expect) self._send('');
}

function processHeader(self, state, key, value, validate, lenient) {
  if (validate)
    validateHeaderName(key);

  // If key is content-disposition and there is content-length
  // encode the value in latin1
  // https://www.rfc-editor.org/rfc/rfc6266#section-4.3
  // Refs: https://github.com/nodejs/node/pull/46528
  if (isContentDispositionField(key) && self._contentLength) {
    // The value could be an array here
    if (ArrayIsArray(value)) {
      for (let i = 0; i < value.length; i++) {
        value[i] = Buffer.from(value[i], 'latin1');
      }
    } else {
      value = Buffer.from(value, 'latin1');
    }
  }

  if (ArrayIsArray(value)) {
    const valueLength = value.length;
    if (
      (valueLength < 2 || !isCookieField(key)) &&
      (!self[kUniqueHeaders] || !self[kUniqueHeaders].has(key.toLowerCase()))
    ) {
      // Retain for(;;) loop for performance reasons
      // Refs: https://github.com/nodejs/node/pull/30958
      for (let i = 0; i < valueLength; i++)
        storeHeader(self, state, key, value[i], validate, lenient);
      return;
    }
    value = value.join('; ');
  }
  storeHeader(self, state, key, value, validate, lenient);
}

function storeHeader(self, state, key, value, validate, lenient) {
  if (validate)
    validateHeaderValue(key, value, lenient);
  // Buffer values (notably content-disposition when Content-Length is set)
  // must be flattened as latin1 so the subsequent latin1 header write puts
  // the original octets on the wire. Default Buffer->string is utf8, which
  // re-decodes the bytes and breaks binary header values under latin1 write.
  if (typeof value !== 'string') {
    if (isUint8Array(value))
      value = value.toString('latin1');
    else
      value = `${value}`;
  }
  state.header += key + ': ' + value + '\r\n';
  matchHeader(self, state, key, value);
}

function matchHeader(self, state, field, value) {
  const len = field.length;
  // Cheap (length, first character) pre-filter so the common case, a
  // header that is not one of the eight known fields below, returns
  // without paying the per-header toLowerCase() allocation. The filter
  // only rejects names that cannot possibly match the switch: `| 0x20`
  // lower-cases ASCII letters and maps no other token character onto a
  // letter, and every known field length is enumerated.
  const c = field.charCodeAt(0) | 0x20;
  switch (len) {
    case 4: if (c !== 0x64) return; break; // Date
    case 6: if (c !== 0x65) return; break; // Expect
    case 7: if (c !== 0x74) return; break; // Trailer
    case 10: if (c !== 0x63 && c !== 0x6b) return; break; // Connection, Keep-Alive
    case 14: if (c !== 0x63) return; break; // Content-Length
    case 17: if (c !== 0x74) return; break; // Transfer-Encoding
    default: return; // No known field has this length
  }
  field = field.toLowerCase();
  switch (field) {
    case 'connection':
      state.connection = true;
      self._removedConnection = false;
      if (RE_CONN_CLOSE.test(value))
        self._last = true;
      else
        self.shouldKeepAlive = true;
      break;
    case 'transfer-encoding':
      state.te = true;
      self._removedTE = false;
      if (RE_TE_CHUNKED.test(value))
        self.chunkedEncoding = true;
      break;
    case 'content-length':
      state.contLen = true;
      self._contentLength = +value;
      self._removedContLen = false;
      break;
    case 'date':
    case 'expect':
    case 'trailer':
      state[field] = true;
      break;
    case 'keep-alive':
      self._defaultKeepAlive = false;
      break;
  }
}

const validateHeaderName = assignFunctionName('validateHeaderName', hideStackFrames((name, label) => {
  // Common names used by writeHead() in hot servers (simple.js, APIs).
  // Avoid checkIsHttpToken() char scanning for these fixed tokens.
  if (name === 'Content-Type' || name === 'Content-Length' ||
      name === 'Transfer-Encoding' || name === 'Connection' ||
      name === 'Date' || name === 'Keep-Alive' || name === 'Host' ||
      name === 'content-type' || name === 'content-length' ||
      name === 'transfer-encoding') {
    return;
  }
  if (typeof name !== 'string' || !name || !checkIsHttpToken(name)) {
    throw new ERR_INVALID_HTTP_TOKEN.HideStackFramesError(label || 'Header name', name);
  }
}));

const validateHeaderValue = assignFunctionName('validateHeaderValue', hideStackFrames((name, value, lenient) => {
  if (value === undefined) {
    throw new ERR_HTTP_INVALID_HEADER_VALUE.HideStackFramesError(value, name);
  }
  // Hot path: pure printable ASCII string values (digits, tokens, simple
  // media types). Skip the regex scan used by checkInvalidHeaderChar.
  if (typeof value === 'string') {
    const len = value.length;
    // Empty is valid. Fast reject only on CR/LF/NUL via a tight loop that
    // V8 can optimize better than the general regex for short values.
    let ok = true;
    for (let i = 0; i < len; i++) {
      const c = value.charCodeAt(i);
      // Allow tab (9) and 0x20-0x7e; reject CTL and DEL. High bytes go to
      // the full checker (content-disposition latin1 etc.).
      if (c === 9 || (c >= 32 && c <= 126)) continue;
      ok = false;
      break;
    }
    if (ok) return;
  }
  if (checkInvalidHeaderChar(value, lenient)) {
    debug('Header "%s" contains invalid characters', name);
    throw new ERR_INVALID_CHAR.HideStackFramesError('header content', name);
  }
}));

function parseUniqueHeadersOption(headers) {
  if (!ArrayIsArray(headers)) {
    return null;
  }

  const unique = new SafeSet();
  const l = headers.length;
  for (let i = 0; i < l; i++) {
    unique.add(headers[i].toLowerCase());
  }

  return unique;
}

OutgoingMessage.prototype.setHeader = function setHeader(name, value) {
  if (this._header) {
    throw new ERR_HTTP_HEADERS_SENT('set');
  }
  validateHeaderName(name);
  if (value === undefined) {
    throw new ERR_HTTP_INVALID_HEADER_VALUE(value, name);
  }
  if (checkInvalidHeaderChar(value, this._isLenientHeaderValidation())) {
    debug('Header "%s" contains invalid characters', name);
    throw new ERR_INVALID_CHAR('header content', name);
  }

  let headers = this[kOutHeaders];
  if (headers === null)
    this[kOutHeaders] = headers = { __proto__: null };

  headers[name.toLowerCase()] = [name, value];
  return this;
};

OutgoingMessage.prototype.setHeaders = function setHeaders(headers) {
  if (this._header) {
    throw new ERR_HTTP_HEADERS_SENT('set');
  }


  if (
    !headers ||
    ArrayIsArray(headers) ||
    typeof headers.keys !== 'function' ||
    typeof headers.get !== 'function'
  ) {
    throw new ERR_INVALID_ARG_TYPE('headers', ['Headers', 'Map'], headers);
  }

  // Headers object joins multiple cookies with a comma when using
  // the getter to retrieve the value,
  // unless iterating over the headers directly.
  // We also cannot safely split by comma.
  // To avoid setHeader overwriting the previous value we push
  // set-cookie values in array and set them all at once.
  let cookies = null;

  for (const { 0: key, 1: value } of headers) {
    if (key === 'set-cookie') {
      if (ArrayIsArray(value)) {
        cookies ??= [];
        cookies.push(...value);
      } else {
        cookies ??= [];
        cookies.push(value);
      }
      continue;
    }
    this.setHeader(key, value);
  }
  if (cookies != null) {
    this.setHeader('set-cookie', cookies);
  }

  return this;
};

OutgoingMessage.prototype.appendHeader = function appendHeader(name, value) {
  if (this._header) {
    throw new ERR_HTTP_HEADERS_SENT('append');
  }
  validateHeaderName(name);
  if (value === undefined) {
    throw new ERR_HTTP_INVALID_HEADER_VALUE(value, name);
  }
  if (checkInvalidHeaderChar(value, this._isLenientHeaderValidation())) {
    debug('Header "%s" contains invalid characters', name);
    throw new ERR_INVALID_CHAR('header content', name);
  }

  const field = name.toLowerCase();
  const headers = this[kOutHeaders];
  if (headers === null || !headers[field]) {
    return this.setHeader(name, value);
  }

  // Prepare the field for appending, if required
  if (!ArrayIsArray(headers[field][1])) {
    headers[field][1] = [headers[field][1]];
  }

  const existingValues = headers[field][1];
  if (ArrayIsArray(value)) {
    for (let i = 0, length = value.length; i < length; i++) {
      existingValues.push(value[i]);
    }
  } else {
    existingValues.push(value);
  }

  return this;
};


OutgoingMessage.prototype.getHeader = function getHeader(name) {
  validateString(name, 'name');

  const headers = this[kOutHeaders];
  if (headers === null)
    return;

  const entry = headers[name.toLowerCase()];
  return entry?.[1];
};


// Returns an array of the names of the current outgoing headers.
OutgoingMessage.prototype.getHeaderNames = function getHeaderNames() {
  return this[kOutHeaders] !== null ? ObjectKeys(this[kOutHeaders]) : [];
};


// Returns an array of the names of the current outgoing raw headers.
OutgoingMessage.prototype.getRawHeaderNames = function getRawHeaderNames() {
  const headersMap = this[kOutHeaders];
  if (headersMap === null) return [];

  const values = ObjectValues(headersMap);
  const headers = Array(values.length);
  // Retain for(;;) loop for performance reasons
  // Refs: https://github.com/nodejs/node/pull/30958
  for (let i = 0, l = values.length; i < l; i++) {
    headers[i] = values[i][0];
  }

  return headers;
};


// Returns a shallow copy of the current outgoing headers.
OutgoingMessage.prototype.getHeaders = function getHeaders() {
  const headers = this[kOutHeaders];
  const ret = { __proto__: null };
  if (headers) {
    const keys = ObjectKeys(headers);
    // Retain for(;;) loop for performance reasons
    // Refs: https://github.com/nodejs/node/pull/30958
    for (let i = 0; i < keys.length; ++i) {
      const key = keys[i];
      const val = headers[key][1];
      ret[key] = val;
    }
  }
  return ret;
};


OutgoingMessage.prototype.hasHeader = function hasHeader(name) {
  validateString(name, 'name');
  return this[kOutHeaders] !== null &&
    !!this[kOutHeaders][name.toLowerCase()];
};


OutgoingMessage.prototype.removeHeader = function removeHeader(name) {
  validateString(name, 'name');

  if (this._header) {
    throw new ERR_HTTP_HEADERS_SENT('remove');
  }

  const key = name.toLowerCase();

  switch (key) {
    case 'connection':
      this._removedConnection = true;
      break;
    case 'content-length':
      this._removedContLen = true;
      break;
    case 'transfer-encoding':
      this._removedTE = true;
      break;
    case 'date':
      this.sendDate = false;
      break;
  }

  if (this[kOutHeaders] !== null) {
    delete this[kOutHeaders][key];
  }
};


OutgoingMessage.prototype._implicitHeader = function _implicitHeader() {
  throw new ERR_METHOD_NOT_IMPLEMENTED('_implicitHeader()');
};

ObjectDefineProperty(OutgoingMessage.prototype, 'headersSent', {
  __proto__: null,
  configurable: true,
  enumerable: true,
  get: function() { return !!this._header; },
});

ObjectDefineProperty(OutgoingMessage.prototype, 'writableEnded', {
  __proto__: null,
  get: function() { return this.finished; },
});

ObjectDefineProperty(OutgoingMessage.prototype, 'writableNeedDrain', {
  __proto__: null,
  get: function() {
    return !this.destroyed && !this.finished && this[kNeedDrain];
  },
});

const crlf_buf = Buffer.from('\r\n');
OutgoingMessage.prototype.write = function write(chunk, encoding, callback) {
  if (typeof encoding === 'function') {
    callback = encoding;
    encoding = null;
  }

  const ret = write_(this, chunk, encoding, callback, false);
  if (!ret)
    this[kNeedDrain] = true;
  return ret;
};

function onError(msg, err, callback) {
  if (msg.destroyed) {
    return;
  }

  const triggerAsyncId = msg.socket ? msg.socket[async_id_symbol] : undefined;
  defaultTriggerAsyncIdScope(triggerAsyncId,
                             process.nextTick,
                             emitErrorNt,
                             msg,
                             err,
                             callback);
}

function emitErrorNt(msg, err, callback) {
  callback(err);
  if (typeof msg.emit === 'function' && !msg.destroyed) {
    msg.emit('error', err);
  }
}

function strictContentLength(msg) {
  return (
    msg.strictContentLength &&
    msg._contentLength != null &&
    msg._hasBody &&
    !msg._removedContLen &&
    !msg.chunkedEncoding &&
    !msg.hasHeader('transfer-encoding')
  );
}

function write_(msg, chunk, encoding, callback, fromEnd) {
  if (typeof callback !== 'function')
    callback = nop;

  if (chunk === null) {
    throw new ERR_STREAM_NULL_VALUES();
  } else if (typeof chunk !== 'string' && !isUint8Array(chunk)) {
    throw new ERR_INVALID_ARG_TYPE(
      'chunk', ['string', 'Buffer', 'Uint8Array'], chunk);
  }

  let err;
  if (msg.finished) {
    err = new ERR_STREAM_WRITE_AFTER_END();
  } else if (msg.destroyed) {
    err = new ERR_STREAM_DESTROYED('write');
  }

  if (err) {
    if (!msg.destroyed) {
      onError(msg, err, callback);
    } else {
      process.nextTick(callback, err);
    }
    return false;
  }

  let len;

  if (msg.strictContentLength) {
    len ??= typeof chunk === 'string' ? Buffer.byteLength(chunk, encoding) : chunk.byteLength;

    if (
      strictContentLength(msg) &&
      (fromEnd ? msg[kBytesWritten] + len !== msg._contentLength : msg[kBytesWritten] + len > msg._contentLength)
    ) {
      throw new ERR_HTTP_CONTENT_LENGTH_MISMATCH(len + msg[kBytesWritten], msg._contentLength);
    }

    msg[kBytesWritten] += len;
  }

  if (!msg._header) {
    if (fromEnd) {
      len ??= typeof chunk === 'string' ? Buffer.byteLength(chunk, encoding) : chunk.byteLength;
      msg._contentLength = len;
    }
    msg._implicitHeader();
  }

  if (!msg._hasBody) {
    if (msg[kRejectNonStandardBodyWrites]) {
      throw new ERR_HTTP_BODY_NOT_ALLOWED();
    } else {
      debug('This type of response MUST NOT have a body. ' +
        'Ignoring write() calls.');
      process.nextTick(callback);
      return true;
    }
  }

  // `socket` is an accessor on the prototype: load it once instead of
  // paying the getter three times on every body write.
  let socket;
  if (!fromEnd && (socket = msg.socket) && !socket.writableCorked) {
    socket.cork();
    process.nextTick(connectionCorkNT, socket);
  }

  let ret;
  if (msg.chunkedEncoding && chunk.length !== 0) {
    len ??= typeof chunk === 'string' ? Buffer.byteLength(chunk, encoding) : chunk.byteLength;
    if (msg[kCorked] && msg._headerSent) {
      msg[kChunkedBuffer].push(chunk, encoding, callback);
      msg[kChunkedLength] += len;
      ret = msg[kChunkedLength] < msg[kHighWaterMark];
    } else {
      msg._send(len.toString(16), 'latin1', null);
      msg._send(crlf_buf, null, null);
      msg._send(chunk, encoding, null, len);
      ret = msg._send(crlf_buf, null, callback);
    }
  } else {
    ret = msg._send(chunk, encoding, callback, len);
  }

  debug('write ret = ' + ret);
  return ret;
}


function connectionCorkNT(conn) {
  conn.uncork();
}

function runChunkCallbacks(callbacks, err) {
  for (let i = 0; i < callbacks.length; i++) {
    callbacks[i](err);
  }
}

OutgoingMessage.prototype.addTrailers = function addTrailers(headers) {
  if (this.finished) {
    throw new ERR_HTTP_HEADERS_SENT('set trailing');
  }

  this._trailer = '';
  const keys = ObjectKeys(headers);
  const isArray = ArrayIsArray(headers);
  const lenient = this._isLenientHeaderValidation();
  // Retain for(;;) loop for performance reasons
  // Refs: https://github.com/nodejs/node/pull/30958
  for (let i = 0, l = keys.length; i < l; i++) {
    let field, value;
    const key = keys[i];
    if (isArray) {
      field = headers[key][0];
      value = headers[key][1];
    } else {
      field = key;
      value = headers[key];
    }
    validateHeaderName(field, 'Trailer name');

    // Check if the field must be sent several times
    const isArrayValue = ArrayIsArray(value);
    if (
      isArrayValue && value.length > 1 &&
      (!this[kUniqueHeaders] || !this[kUniqueHeaders].has(field.toLowerCase()))
    ) {
      for (let j = 0, l = value.length; j < l; j++) {
        if (checkInvalidHeaderChar(value[j], lenient)) {
          debug('Trailer "%s"[%d] contains invalid characters', field, j);
          throw new ERR_INVALID_CHAR('trailer content', field);
        }
        this._trailer += field + ': ' + value[j] + '\r\n';
      }
    } else {
      if (isArrayValue) {
        value = value.join('; ');
      }

      if (checkInvalidHeaderChar(value, lenient)) {
        debug('Trailer "%s" contains invalid characters', field);
        throw new ERR_INVALID_CHAR('trailer content', field);
      }
      this._trailer += field + ': ' + value + '\r\n';
    }
  }
};

function onFinish(outmsg) {
  if (outmsg?.socket?._hadError) return;
  // ServerResponse installs kServerFinishHook; the hook no-ops unless
  // parserOnIncoming armed finish state (kFinishReq).
  const hook = outmsg[kServerFinishHook];
  if (typeof hook === 'function')
    hook.call(outmsg);
  outmsg.emit('finish');
}

OutgoingMessage.prototype.end = function end(chunk, encoding, callback) {
  if (typeof chunk === 'function') {
    callback = chunk;
    chunk = null;
    encoding = null;
  } else if (typeof encoding === 'function') {
    callback = encoding;
    encoding = null;
  }

  // Single-shot fast path: headers not yet rendered, socket assigned, one
  // body chunk (or empty). Builds the entire HTTP message in C++ and issues
  // a single socket.write().
  if (tryFastEnd(this, chunk, encoding, callback)) {
    return this;
  }

  if (chunk) {
    if (this.finished) {
      onError(this,
              new ERR_STREAM_WRITE_AFTER_END(),
              typeof callback !== 'function' ? nop : callback);
      return this;
    }

    if (this[kSocket]) {
      this[kSocket].cork();
    }

    try {
      write_(this, chunk, encoding, null, true);
    } catch (err) {
      // write_() can throw on invalid chunk types before any data is
      // queued; undo the cork so a subsequent end() is not stuck.
      if (this[kSocket]) {
        this[kSocket]._writableState.corked = 1;
        this[kSocket].uncork();
      }
      throw err;
    }
  } else if (this.finished) {
    if (typeof callback === 'function') {
      if (!this.writableFinished) {
        this.on('finish', callback);
      } else {
        callback(new ERR_STREAM_ALREADY_FINISHED('end'));
      }
    }
    return this;
  } else if (!this._header) {
    if (this[kSocket]) {
      this[kSocket].cork();
    }

    this._contentLength = 0;
    this._implicitHeader();
  }

  if (typeof callback === 'function')
    this.once('finish', callback);

  if (strictContentLength(this) && this[kBytesWritten] !== this._contentLength) {
    throw new ERR_HTTP_CONTENT_LENGTH_MISMATCH(this[kBytesWritten], this._contentLength);
  }

  const finish = onFinish.bind(undefined, this);

  if (this._hasBody && this.chunkedEncoding) {
    this._send('0\r\n' + this._trailer + '\r\n', 'latin1', finish);
  } else if (!this._headerSent || this.writableLength || chunk) {
    this._send('', 'latin1', finish);
  } else {
    process.nextTick(finish);
  }

  if (this[kSocket]) {
    // Fully uncork connection on end().
    this[kSocket]._writableState.corked = 1;
    this[kSocket].uncork();
  }
  this[kCorked] = 1;
  this.uncork();

  this.finished = true;

  // There is the first message on the outgoing queue, and we've sent
  // everything to the socket.
  debug('outgoing message end.');
  if (this.outputData.length === 0 &&
      this[kSocket] &&
      this[kSocket]._httpMessage === this) {
    this._finish();
  }

  return this;
};

function prepareFastBody(msg, chunk, encoding) {
  let body = null;
  let bodyEncoding = 0; // 0 = buffer/none, 1 = latin1, 2 = utf8
  let bodyLen = 0;
  if (chunk) {
    if (typeof chunk === 'string') {
      if (encoding && encoding !== 'utf8' && encoding !== 'utf-8' &&
          encoding !== 'latin1' && encoding !== 'binary' &&
          encoding !== 'ascii') {
        return null;
      }
      body = chunk;
      if (encoding === 'latin1' || encoding === 'binary' ||
          encoding === 'ascii') {
        bodyEncoding = 1;
        bodyLen = chunk.length;
      } else {
        bodyEncoding = 2;
        bodyLen = Buffer.byteLength(chunk, 'utf8');
      }
    } else if (isUint8Array(chunk)) {
      body = chunk;
      bodyEncoding = 0;
      bodyLen = chunk.byteLength;
    } else {
      return null;
    }
  }

  if (!msg._hasBody && body && bodyLen > 0) {
    if (msg[kRejectNonStandardBodyWrites]) {
      throw new ERR_HTTP_BODY_NOT_ALLOWED();
    }
    body = null;
    bodyLen = 0;
  }
  return { body, bodyEncoding, bodyLen };
}

function finishFastSocketWrite(msg, socket, ret) {
  if (!ret) msg[kNeedDrain] = true;
  debug('outgoing message fast end.');
  if (msg.outputData.length === 0 && socket._httpMessage === msg) {
    msg._finish();
  }
}

// Write the complete response Buffer/string straight to the socket handle,
// bypassing stream.Writable.write() (and its nextTick deferral of the
// write callback on the sync-write path). This is safe for the single-shot
// end() path: we never half-close the socket here and we still drive the
// OutgoingMessage finish lifecycle via the write callback.
// `msg` is the OutgoingMessage; finish is driven via onFinish(msg).
function writeResponseDirect(socket, data, encoding, msg) {
  // Require a real TCP/libuv stream handle (writeBuffer). Fall back for:
  // - mock sockets used in tests (dummy _handle)
  // - TLS sockets (TLSWrap implements writeBuffer but is not safe to drive
  //   via writeGeneric outside Socket._writeGeneric)
  // - sockets still connecting
  if (socket.encrypted || socket.connecting || !socket._handle ||
      typeof socket._handle.writeBuffer !== 'function') {
    // Fall back to the normal Writable path.
    return socket.write(data, encoding, () => onFinish(msg));
  }
  // Refresh the keep-alive / inactivity timer the same way Socket._writeGeneric
  // does before issuing the write.
  if (typeof socket._unrefTimer === 'function')
    socket._unrefTimer();

  // Mirror stream.Writable: if the handle write completes synchronously,
  // defer finish to nextTick so end()/finish ordering stays correct
  // (finish must not run re-entrantly inside end()).
  let sync = true;
  writeGeneric(socket, data, encoding || 'buffer', function onDirectWrite() {
    if (sync) {
      process.nextTick(onFinish, msg);
    } else {
      onFinish(msg);
    }
  });
  sync = false;
  return true;
}

// Build (or reuse) a single Buffer containing headers + body for the
// small-response hot path. The map lives on the cached header Buffer so a
// Date: rollover naturally drops stale entries when the header is rebuilt.
// `chunked`: when true, wrap body in a single chunk + terminator.
function getCombinedResponse(headerBuf, body, bodyLen, bodyEncoding, chunked) {
  const mapSym = chunked ? kChunkedFullBodyMap : kFullBodyMap;
  let map = headerBuf[mapSym];
  if (map !== undefined) {
    const hit = map.get(body);
    if (hit !== undefined) return hit;
  }
  const hLen = headerBuf.byteLength;
  let out;
  if (chunked) {
    if (bodyLen === 0) {
      // headers + 0\r\n\r\n
      out = Buffer.allocUnsafe(hLen + 5);
      headerBuf.copy(out, 0, 0, hLen);
      out.write('0\r\n\r\n', hLen, 5, 'latin1');
    } else {
      const hex = bodyLen.toString(16);
      // Header + hex + CRLF + body + chunked terminator.
      const prefixLen = hex.length + 2;
      const suffixLen = 7; // Length of \r\n0\r\n\r\n
      out = Buffer.allocUnsafe(hLen + prefixLen + bodyLen + suffixLen);
      headerBuf.copy(out, 0, 0, hLen);
      let off = hLen;
      off += out.write(hex, off, 'latin1');
      out[off++] = 13; out[off++] = 10;
      if (bodyEncoding === 0) {
        if (typeof body.copy === 'function')
          body.copy(out, off, 0, bodyLen);
        else
          out.set(body.subarray(0, bodyLen), off);
      } else {
        out.write(body, off, bodyLen, 'latin1');
      }
      off += bodyLen;
      out.write('\r\n0\r\n\r\n', off, 7, 'latin1');
    }
  } else {
    out = Buffer.allocUnsafe(hLen + bodyLen);
    headerBuf.copy(out, 0, 0, hLen);
    if (bodyEncoding === 0) {
      if (typeof body.copy === 'function')
        body.copy(out, hLen, 0, bodyLen);
      else
        out.set(body.subarray(0, bodyLen), hLen);
    } else {
      out.write(body, hLen, bodyLen, 'latin1');
    }
  }
  if (map === undefined) {
    map = new SafeMap();
    headerBuf[mapSym] = map;
  }
  if (map.size < kFullBodyMapMax)
    map.set(body, out);
  return out;
}

// writeHead()+end(body) hot path used by benchmark/http/simple.js.
// Skips write_()/cork for a single socket.write() of headers (+ body).
// Handles both Content-Length and Transfer-Encoding: chunked (no trailers).
function tryFastFlushEnd(msg, chunk, encoding, callback) {
  if (!msg._header || msg._headerSent || msg.finished || msg.destroyed)
    return false;
  // Trailers need the multi-write terminator path (addTrailers after body).
  if (msg._trailer) return false;
  if (msg.outputData.length !== 0 || msg[kCorked]) return false;
  if (msg._expect_continue && !msg._sent100) return false;
  if (strictContentLength(msg)) return false;

  const socket = msg[kSocket];
  if (!socket || socket._httpMessage !== msg || !socket.writable ||
      socket.destroyed || !socket._writableState || !socket._handle) {
    return false;
  }

  const prepared = prepareFastBody(msg, chunk, encoding);
  if (prepared === null) return false;
  const { body, bodyEncoding, bodyLen } = prepared;

  if (typeof callback === 'function')
    msg.once('finish', callback);

  // Bound finish for the multi-write / socket.write fallback paths only.
  // writeResponseDirect takes `msg` and calls onFinish directly.
  let finish;
  const getFinish = () => {
    if (finish === undefined)
      finish = onFinish.bind(undefined, msg);
    return finish;
  };
  const header = msg._header;
  if (typeof header !== 'string' && !isUint8Array(header))
    return false;

  const hasBody = body !== null && bodyLen > 0 && msg._hasBody;
  const largeBody = bodyLen > kMaxSingleShotBody;
  const chunked = msg.chunkedEncoding;
  let ret;

  if (typeof header === 'string') {
    if (chunked) {
      // Complete chunked message: headers + [chunk] + terminator.
      // Avoids write_() hex framing + multiple _send() calls.
      // Match legacy end(): only emit the chunked terminator when the
      // response is allowed a body (not HEAD / 204 / 304).
      if (!hasBody) {
        if (msg._hasBody) {
          ret = writeResponseDirect(socket, header + '0\r\n\r\n', 'latin1', msg);
        } else {
          ret = writeResponseDirect(socket, header, 'latin1', msg);
        }
      } else if (!largeBody && typeof body === 'string') {
        const hex = bodyLen.toString(16);
        const wire = header + hex + '\r\n' + body + '\r\n0\r\n\r\n';
        if (bodyEncoding === 1 || bodyLen === body.length) {
          ret = writeResponseDirect(socket, wire, 'latin1', msg);
        } else {
          // Header may contain binary (latin1) octets; force latin1 body
          // framing is already pure ASCII so overall write must be latin1.
          ret = writeResponseDirect(socket, wire, 'latin1', msg);
        }
      } else if (hasBody) {
        // Buffer / large body: corked multi-write, zero-copy body.
        if (!socket.writableCorked) socket.cork();
        socket.write(header + bodyLen.toString(16) + '\r\n', 'latin1');
        if (bodyEncoding === 0)
          socket.write(body);
        else if (bodyEncoding === 1)
          socket.write(body, 'latin1');
        else
          socket.write(body, 'utf8');
        ret = socket.write('\r\n0\r\n\r\n', 'latin1', getFinish());
        socket._writableState.corked = 1;
        socket.uncork();
      } else {
        ret = writeResponseDirect(socket, header, 'latin1', msg);
      }
    } else if (!hasBody) {
      ret = writeResponseDirect(socket, header, 'latin1', msg);
    } else if (!largeBody && typeof body === 'string') {
      // Small string body: one write of header+body (no cork).
      // Always latin1: headers may contain binary octets from
      // Buffer.toString('binary'), and pure-ASCII bodies are identical
      // under latin1 vs utf8. Using utf8 would re-encode high bytes.
      ret = writeResponseDirect(socket, header + body, 'latin1', msg);
    } else {
      // Buffer body or large payload: corked dual write (zero-copy body).
      // Combining header+Buffer (alloc + latin1 encode) is slower for small
      // buffers than a corked dual write on the simple.js type=buffer path.
      if (!socket.writableCorked) socket.cork();
      socket.write(header, 'latin1');
      if (bodyEncoding === 0) {
        ret = socket.write(body, getFinish());
      } else if (bodyEncoding === 1) {
        ret = socket.write(body, 'latin1', getFinish());
      } else {
        ret = socket.write(body, 'utf8', getFinish());
      }
      socket._writableState.corked = 1;
      socket.uncork();
    }
  } else if (isUint8Array(header)) {
    // Buffer header (from header-block cache). Prefer a single write of a
    // combined Buffer so we never re-encode the header and pay cork/writev
    // dual-write overhead only for large bodies. Full wire messages are
    // cached per (headerBuf, body) for the simple.js keep-alive path.
    if (chunked && !hasBody) {
      // Match legacy end(): terminator only when a body is allowed.
      if (msg._hasBody) {
        const out = getCombinedResponse(header, '', 0, 1, true);
        ret = writeResponseDirect(socket, out, 'buffer', msg);
      } else {
        ret = writeResponseDirect(socket, header, 'buffer', msg);
      }
    } else if (chunked && hasBody && !largeBody && typeof body === 'string' &&
               (bodyEncoding === 1 || bodyLen === body.length ||
                bodyEncoding === 2)) {
      // BodyEncoding 2 (utf8 string): non-ASCII bodies need multi-write.
      if (bodyEncoding === 2 && bodyLen !== body.length) {
        if (!socket.writableCorked) socket.cork();
        socket.write(header);
        socket.write(bodyLen.toString(16) + '\r\n', 'latin1');
        socket.write(body, 'utf8');
        ret = socket.write('\r\n0\r\n\r\n', 'latin1', getFinish());
        socket._writableState.corked = 1;
        socket.uncork();
      } else {
        const out = getCombinedResponse(header, body, bodyLen, 1, true);
        ret = writeResponseDirect(socket, out, 'buffer', msg);
      }
    } else if (chunked && hasBody && !largeBody && bodyEncoding === 0) {
      const out = getCombinedResponse(header, body, bodyLen, 0, true);
      ret = writeResponseDirect(socket, out, 'buffer', msg);
    } else if (chunked && hasBody) {
      if (!socket.writableCorked) socket.cork();
      socket.write(header);
      socket.write(bodyLen.toString(16) + '\r\n', 'latin1');
      if (bodyEncoding === 0)
        socket.write(body);
      else if (bodyEncoding === 1)
        socket.write(body, 'latin1');
      else
        socket.write(body, 'utf8');
      ret = socket.write('\r\n0\r\n\r\n', 'latin1', getFinish());
      socket._writableState.corked = 1;
      socket.uncork();
    } else if (chunked) {
      ret = writeResponseDirect(socket, header, 'buffer', msg);
    } else if (!hasBody) {
      ret = writeResponseDirect(socket, header, 'buffer', msg);
    } else if (!largeBody && typeof body === 'string' &&
               (bodyEncoding === 1 || bodyLen === body.length)) {
      // String body: combine under latin1 so binary header octets stay intact.
      const out = getCombinedResponse(header, body, bodyLen, 1, false);
      ret = writeResponseDirect(socket, out, 'buffer', msg);
    } else if (!largeBody && typeof body === 'string') {
      // Non-ASCII utf8 body: dual write keeps body encoding correct.
      if (!socket.writableCorked) socket.cork();
      socket.write(header);
      ret = socket.write(body, 'utf8', getFinish());
      socket._writableState.corked = 1;
      socket.uncork();
    } else if (!largeBody && bodyEncoding === 0) {
      // Buffer body: cached combined header+body Buffer.
      const out = getCombinedResponse(header, body, bodyLen, 0, false);
      ret = writeResponseDirect(socket, out, 'buffer', msg);
    } else if (hasBody) {
      if (!socket.writableCorked) socket.cork();
      socket.write(header);
      if (bodyEncoding === 0) {
        ret = socket.write(body, getFinish());
      } else if (bodyEncoding === 1) {
        ret = socket.write(body, 'latin1', getFinish());
      } else {
        ret = socket.write(body, 'utf8', getFinish());
      }
      socket._writableState.corked = 1;
      socket.uncork();
    } else {
      ret = writeResponseDirect(socket, header, 'buffer', msg);
    }
  } else {
    return false;
  }

  msg._headerSent = true;
  msg.finished = true;
  if (bodyLen && msg._hasBody)
    msg[kBytesWritten] = bodyLen;

  finishFastSocketWrite(msg, socket, ret);
  return true;
}

function tryFastEnd(msg, chunk, encoding, callback) {
  // ServerResponse only. ClientRequest has method-specific Content-Length
  // rules (GET/HEAD/DELETE must not emit CL:0) and CONNECT tunnel framing
  // that the pure-JS builder does not model.
  if (typeof msg.statusCode !== 'number')
    return false;

  if (msg.finished || msg.destroyed)
    return false;

  // Hot path after writeHead(): headers already built, not yet flushed.
  if (msg._header && !msg._headerSent) {
    return tryFastFlushEnd(msg, chunk, encoding, callback);
  }

  // setHeader() path: headers not yet rendered. Build them with the JS
  // storeHeader (tryFastStoreHeaderObject for plain objects) then flush.
  // The C++ buildHttpMessage path was slower for small messages.
  if (msg._header || msg._headerSent)
    return false;
  if (msg._trailer) return false;
  if (msg.outputData.length !== 0) return false;
  if (msg[kCorked]) return false;
  if (msg._expect_continue && !msg._sent100) return false;
  // _contentLength is only populated once headers are stored, so the
  // strictContentLength() helper is false here even when the user set
  // Content-Length via setHeader(). Always defer when strict mode is on
  // so write_()/end() enforce ERR_HTTP_CONTENT_LENGTH_MISMATCH.
  if (msg.strictContentLength) return false;

  const socket = msg[kSocket];
  // Require a real net.Socket (has a libuv _handle). Standalone
  // ServerResponse tests assign arbitrary Writables that must keep the
  // multi-write _send() behaviour.
  if (!socket || socket._httpMessage !== msg || !socket.writable ||
      socket.destroyed || !socket._writableState || !socket._handle) {
    return false;
  }

  const prepared = prepareFastBody(msg, chunk, encoding);
  if (prepared === null) return false;
  const { bodyLen } = prepared;

  // Large bodies: let write_()/ _send stream without a combined copy.
  if (bodyLen > kMaxSingleShotBody)
    return false;

  // Seed Content-Length for auto-CL when the user did not set it (same as
  // the fromEnd branch of write_()).
  if (bodyLen > 0 && msg._hasBody && typeof msg._contentLength !== 'number') {
    msg._contentLength = bodyLen;
  } else if (!bodyLen || !msg._hasBody) {
    if (typeof msg._contentLength !== 'number')
      msg._contentLength = 0;
  }

  msg._implicitHeader();
  if (!msg._header || msg._headerSent)
    return false;

  return tryFastFlushEnd(msg, chunk, encoding, callback);
}


// This function is called once all user data are flushed to the socket.
// Note that it has a chance that the socket is not drained.
OutgoingMessage.prototype._finish = function _finish() {
  assert(this[kSocket]);
  this.emit('prefinish');
};


// This logic is probably a bit confusing. Let me explain a bit:
//
// In both HTTP servers and clients it is possible to queue up several
// outgoing messages. This is easiest to imagine in the case of a client.
// Take the following situation:
//
//    req1 = client.request('GET', '/');
//    req2 = client.request('POST', '/');
//
// When the user does
//
//   req2.write('hello world\n');
//
// it's possible that the first request has not been completely flushed to
// the socket yet. Thus the outgoing messages need to be prepared to queue
// up data internally before sending it on further to the socket's queue.
//
// This function, _flush(), is called by both the Server and Client
// to attempt to flush any pending messages out to the socket.
OutgoingMessage.prototype._flush = function _flush() {
  const socket = this[kSocket];

  if (socket?.writable) {
    // There might be remaining data in this.output; write it out
    this._flushOutput(socket);

    if (this.finished) {
      // This is a queue to the server or client to bring in the next this.
      this._finish();
    } else if (this[kNeedDrain] && this.writableLength === 0) {
      this[kNeedDrain] = false;
      this.emit('drain');
    }
  }
};

OutgoingMessage.prototype._flushOutput = function _flushOutput(socket) {
  const outputLength = this.outputData.length;
  if (outputLength <= 0)
    return undefined;

  const outputData = this.outputData;
  socket.cork();
  let ret;
  // Retain for(;;) loop for performance reasons
  // Refs: https://github.com/nodejs/node/pull/30958
  for (let i = 0; i < outputLength; i++) {
    const { data, encoding, callback } = outputData[i];
    // Avoid any potential ref to Buffer in new generation from old generation
    outputData[i].data = null;
    ret = socket.write(data, encoding, callback);
  }
  socket.uncork();

  this.outputData = [];
  this._onPendingData(-this.outputSize);
  this.outputSize = 0;

  return ret;
};


OutgoingMessage.prototype.flushHeaders = function flushHeaders() {
  if (!this._header) {
    this._implicitHeader();
  }

  // Force-flush the headers.
  this._send('');
};

OutgoingMessage.prototype.pipe = function pipe() {
  // OutgoingMessage should be write-only. Piping from it is disabled.
  this.emit('error', new ERR_STREAM_CANNOT_PIPE());
};

OutgoingMessage.prototype[EE.captureRejectionSymbol] =
function(err, event) {
  this.destroy(err);
};

module.exports = {
  kHighWaterMark,
  kUniqueHeaders,
  kServerFinishHook,
  parseUniqueHeadersOption,
  validateHeaderName,
  validateHeaderValue,
  OutgoingMessage,
};

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
  SafeSet,
  Symbol,
  Uint32Array,
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

const {
  buildHttpMessage,
} = internalBinding('http_parser');

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

const nop = () => {};

const RE_CONN_CLOSE = /(?:^|\W)close(?:$|\W)/i;

// Keep in sync with BuildHttpMessageFlags in src/node_http_parser.cc.
const kBuildSendDate = 1 << 0;
const kBuildShouldKeepAlive = 1 << 1;
const kBuildMaxRequestsReached = 1 << 2;
const kBuildDefaultKeepAlive = 1 << 3;
const kBuildHasBody = 1 << 4;
const kBuildUseChunkedByDefault = 1 << 5;
const kBuildRemovedConnection = 1 << 6;
const kBuildRemovedContLen = 1 << 7;
const kBuildRemovedTE = 1 << 8;
const kBuildHasAgent = 1 << 9;

// Keep in sync with BuildHttpMessageOut in src/node_http_parser.cc.
const kOutLast = 0;
const kOutChunked = 1;
const kOutContentLength = 2;
const kOutHasContentLength = 3;
const kOutCount = 4;

// Reused across calls to avoid allocating a Uint32Array per response.
const buildHttpMessageOut = new Uint32Array(kOutCount);

// Lazy to avoid a require cycle with _http_server.
let _statusCodes;
function statusCodes() {
  _statusCodes ??= require('_http_server').STATUS_CODES;
  return _statusCodes;
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
    // Headers are stored as a latin1 JS string of the on-wire bytes. If we
    // naively do `header + body` and write as UTF-8, high-byte header values
    // (e.g. content-disposition) get double-encoded. Prefer Buffer concat
    // when the body is present; pure header flushes use latin1.
    if (typeof data === 'string' && data.length > 0 &&
        (encoding === 'latin1' || encoding === 'binary')) {
      data = header + data;
      encoding = 'latin1';
    } else if (typeof data === 'string' && data.length > 0 &&
               (encoding === 'utf8' || encoding === 'utf-8' || !encoding)) {
      // ASCII headers can concatenate as a string under UTF-8; high-byte
      // latin1 header values must be Buffer-concatenated to avoid double
      // encoding on the wire.
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
        const bodyBuf = Buffer.from(data, encoding || 'utf8');
        data = Buffer.concat([Buffer.from(header, 'latin1'), bodyBuf]);
        encoding = 'buffer';
        byteLength = data.byteLength;
      }
    } else if (isUint8Array(data) && data.byteLength > 0) {
      data = Buffer.concat([Buffer.from(header, 'latin1'), data]);
      encoding = 'buffer';
      byteLength = data.byteLength;
    } else if (typeof data === 'string' && data.length === 0) {
      // Header-only flush (end without body, Expect: 100-continue, etc.).
      data = header;
      encoding = 'latin1';
    } else {
      // Non-utf8/latin1 body encodings (hex, utf16le, base64, ...): keep the
      // header as a separate latin1 write so body encoding is unchanged.
      this.outputData.unshift({
        data: header,
        encoding: 'latin1',
        callback: null,
      });
      this.outputSize += header.length;
      this._onPendingData(header.length);
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
  // Prefer the C++ single-buffer builder. Falls back to the legacy JS path
  // only when headers need JS-only special handling (content-disposition
  // latin1, unique-header cookie joining, etc.).
  if (tryNativeStoreHeader(this, firstLine, headers)) {
    return;
  }
  legacyStoreHeader(this, firstLine, headers);
}

function buildFlags(msg) {
  let flags = 0;
  if (msg.sendDate) flags |= kBuildSendDate;
  if (msg.shouldKeepAlive) flags |= kBuildShouldKeepAlive;
  if (msg.maxRequestsOnConnectionReached) flags |= kBuildMaxRequestsReached;
  if (msg._defaultKeepAlive) flags |= kBuildDefaultKeepAlive;
  if (msg._hasBody) flags |= kBuildHasBody;
  // useChunkedEncodingByDefault and agent are independent: agent only
  // participates in keep-alive decisions, not Content-Length emission.
  if (msg.useChunkedEncodingByDefault) flags |= kBuildUseChunkedByDefault;
  if (msg._removedConnection) flags |= kBuildRemovedConnection;
  if (msg._removedContLen) flags |= kBuildRemovedContLen;
  if (msg._removedTE) flags |= kBuildRemovedTE;
  if (msg.agent) flags |= kBuildHasAgent;
  return flags;
}

function applyBuildResult(msg, out) {
  if (out[kOutLast]) msg._last = true;
  if (out[kOutChunked]) msg.chunkedEncoding = true;
  else msg.chunkedEncoding = false;
  if (out[kOutHasContentLength])
    msg._contentLength = out[kOutContentLength];
}

// Flatten headers into [name, value, ...] for the C++ builder. Returns null
// when a header requires the legacy JS path.
function flattenHeadersForNative(msg, headers, validate) {
  const flat = [];
  const lenient = msg._isLenientHeaderValidation();

  function pushPair(key, value, doValidate) {
    if (doValidate) {
      validateHeaderName(key);
    }
    // content-disposition + content-length needs latin1 Buffer values - JS only.
    if (isContentDispositionField(key) && msg._contentLength) {
      return false;
    }
    if (ArrayIsArray(value)) {
      const valueLength = value.length;
      if (
        (valueLength < 2 || !isCookieField(key)) &&
        (!msg[kUniqueHeaders] || !msg[kUniqueHeaders].has(key.toLowerCase()))
      ) {
        for (let i = 0; i < valueLength; i++) {
          if (doValidate) validateHeaderValue(key, value[i], lenient);
          // Buffers not supported on the native path.
          if (typeof value[i] !== 'string') return false;
          flat.push(key, value[i]);
        }
        return true;
      }
      value = value.join('; ');
    }
    if (doValidate) validateHeaderValue(key, value, lenient);
    if (typeof value !== 'string') return false;
    flat.push(key, value);
    return true;
  }

  if (!headers) return flat;

  if (headers === msg[kOutHeaders]) {
    for (const key in headers) {
      const entry = headers[key];
      if (!pushPair(entry[0], entry[1], false)) return null;
    }
    return flat;
  }

  if (ArrayIsArray(headers)) {
    const headersLength = headers.length;
    if (headersLength && ArrayIsArray(headers[0])) {
      for (let i = 0; i < headersLength; i++) {
        const entry = headers[i];
        if (!pushPair(entry[0], entry[1], validate)) return null;
      }
    } else {
      if (headersLength % 2 !== 0) {
        throw new ERR_INVALID_ARG_VALUE('headers', headers);
      }
      for (let n = 0; n < headersLength; n += 2) {
        if (!pushPair(headers[n], headers[n + 1], validate)) return null;
      }
    }
    return flat;
  }

  for (const key in headers) {
    if (ObjectHasOwn(headers, key)) {
      if (!pushPair(key, headers[key], validate)) return null;
    }
  }
  return flat;
}

function headerListHasChunkedTE(flat) {
  for (let i = 0; i < flat.length; i += 2) {
    const name = flat[i];
    if (name.length === 17 && name.toLowerCase() === 'transfer-encoding') {
      if (RE_TE_CHUNKED.test(flat[i + 1])) return true;
    }
  }
  return false;
}

function tryNativeStoreHeader(msg, firstLine, headers) {
  const flat = flattenHeadersForNative(
    msg, headers, headers !== msg[kOutHeaders]);
  if (flat === null) return false;

  // Force the connection to close when the response is a 204 No Content or
  // a 304 Not Modified and the user has set a "Transfer-Encoding: chunked"
  // header. Must run before building so Connection: close is emitted.
  // RFC 2616: 204/304 MUST NOT have a body; keep TE on the wire for compat
  // but do not actually chunk-encode, and close the connection.
  const noBodyStatus = msg.statusCode === 204 || msg.statusCode === 304;
  if (noBodyStatus && (msg.chunkedEncoding || headerListHasChunkedTE(flat))) {
    debug(msg.statusCode + ' response should not use chunked encoding,' +
          ' closing connection.');
    msg.chunkedEncoding = false;
    msg.shouldKeepAlive = false;
  }

  const knownLength =
    typeof msg._contentLength === 'number' ? msg._contentLength : -1;
  const keepAliveSec = msg._keepAliveTimeout ?
    MathFloor(msg._keepAliveTimeout / 1000) :
    0;
  const maxReq = msg._maxRequestsPerSocket | 0;
  const date = msg.sendDate ? utcDate() : '';

  buildHttpMessageOut[0] = 0;
  buildHttpMessageOut[1] = 0;
  buildHttpMessageOut[2] = 0;
  buildHttpMessageOut[3] = 0;

  const buf = buildHttpMessage(
    firstLine,
    flat,
    null,
    0,
    buildFlags(msg),
    date,
    keepAliveSec,
    maxReq,
    knownLength,
    buildHttpMessageOut,
  );
  if (!buf) return false;

  applyBuildResult(msg, buildHttpMessageOut);

  if (noBodyStatus) {
    msg.chunkedEncoding = false;
  }

  // Trailer without chunked is invalid (same check as legacy path).
  // The C++ builder does not throw; detect trailer presence cheaply.
  for (let i = 0; i < flat.length; i += 2) {
    const name = flat[i];
    if (name.length === 7 && (name === 'Trailer' || name === 'trailer' ||
        name.toLowerCase() === 'trailer') && !msg.chunkedEncoding) {
      throw new ERR_HTTP_TRAILER_INVALID();
    }
  }

  // latin1 string of the wire bytes. _send() must write this as latin1 (or
  // Buffer-concat) rather than UTF-8 so high-byte values (content-disposition)
  // are not double-encoded.
  msg._header = buf.toString('latin1');
  msg._headerSent = false;

  // Expect: 100-continue forces an early header flush (legacy behavior).
  for (let i = 0; i < flat.length; i += 2) {
    const name = flat[i];
    if (name.length === 6 && (name === 'Expect' || name === 'expect' ||
        name.toLowerCase() === 'expect')) {
      msg._send('');
      break;
    }
  }
  return true;
}

function legacyStoreHeader(self, firstLine, headers) {
  // firstLine in the case of request is: 'GET /index.html HTTP/1.1\r\n'
  // in the case of response it is: 'HTTP/1.1 200 OK\r\n'
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
    header += 'Date: ' + utcDate() + '\r\n';
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
  if (typeof name !== 'string' || !name || !checkIsHttpToken(name)) {
    throw new ERR_INVALID_HTTP_TOKEN.HideStackFramesError(label || 'Header name', name);
  }
}));

const validateHeaderValue = assignFunctionName('validateHeaderValue', hideStackFrames((name, value, lenient) => {
  if (value === undefined) {
    throw new ERR_HTTP_INVALID_HEADER_VALUE.HideStackFramesError(value, name);
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

function getFastFirstLine(msg) {
  // ServerResponse
  if (typeof msg.statusCode === 'number') {
    const statusCode = msg.statusCode | 0;
    if (statusCode < 100 || statusCode > 999) return null;
    let statusMessage = msg.statusMessage;
    if (statusMessage === undefined || statusMessage === null) {
      statusMessage = statusCodes()[statusCode] || 'unknown';
      msg.statusMessage = statusMessage;
    }
    if (checkInvalidHeaderChar(statusMessage)) {
      throw new ERR_INVALID_CHAR('statusMessage');
    }
    // Align with writeHead() side effects for informational / no-body codes.
    if (statusCode === 204 || statusCode === 304 ||
        (statusCode >= 100 && statusCode <= 199)) {
      msg._hasBody = false;
    }
    if (msg._expect_continue && !msg._sent100) {
      msg.shouldKeepAlive = false;
    }
    return `HTTP/1.1 ${statusCode} ${statusMessage}\r\n`;
  }
  // ClientRequest
  if (typeof msg.method === 'string' && typeof msg.path === 'string') {
    return msg.method + ' ' + msg.path + ' HTTP/1.1\r\n';
  }
  return null;
}

function tryFastEnd(msg, chunk, encoding, callback) {
  // ServerResponse only. ClientRequest has method-specific Content-Length
  // rules (GET/HEAD/DELETE must not emit CL:0) and CONNECT tunnel framing
  // that the C++ builder does not model.
  if (typeof msg.statusCode !== 'number')
    return false;

  // Preconditions: nothing rendered yet, socket ready, no queued output,
  // no trailers, not already finished/destroyed.
  if (msg._header || msg._headerSent || msg.finished || msg.destroyed)
    return false;
  if (msg._trailer) return false;
  if (msg.outputData.length !== 0) return false;
  if (msg[kCorked]) return false;
  if (msg._expect_continue && !msg._sent100) return false;

  const socket = msg[kSocket];
  // Require a real net.Socket (has a libuv _handle). Standalone
  // ServerResponse tests assign arbitrary Writables that must keep the
  // multi-write _send() behaviour.
  if (!socket || socket._httpMessage !== msg || !socket.writable ||
      socket.destroyed || !socket._writableState || !socket._handle) {
    return false;
  }

  // Body encoding must be latin1/utf8/buffer (what the C++ builder supports).
  let body = null;
  let bodyEncoding = 0; // 0 = buffer/none
  let bodyLen = 0;
  if (chunk) {
    if (typeof chunk === 'string') {
      if (encoding && encoding !== 'utf8' && encoding !== 'utf-8' &&
          encoding !== 'latin1' && encoding !== 'binary' &&
          encoding !== 'ascii') {
        return false;
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
      return false;
    }
  }

  if (!msg._hasBody && body && bodyLen > 0) {
    if (msg[kRejectNonStandardBodyWrites]) {
      throw new ERR_HTTP_BODY_NOT_ALLOWED();
    }
    // Legacy: ignore body for HEAD / 204 / 304.
    body = null;
    bodyLen = 0;
  }

  const firstLine = getFastFirstLine(msg);
  if (firstLine === null) return false;

  let flat;
  try {
    flat = flattenHeadersForNative(msg, msg[kOutHeaders], false);
  } catch {
    return false;
  }
  if (flat === null) return false;

  if (body && msg._hasBody) {
    msg._contentLength = bodyLen;
  } else if (!msg._header) {
    msg._contentLength = 0;
  }

  const knownLength =
    typeof msg._contentLength === 'number' ? msg._contentLength : -1;
  const keepAliveSec = msg._keepAliveTimeout ?
    MathFloor(msg._keepAliveTimeout / 1000) :
    0;
  const maxReq = msg._maxRequestsPerSocket | 0;
  const date = msg.sendDate ? utcDate() : '';

  buildHttpMessageOut[0] = 0;
  buildHttpMessageOut[1] = 0;
  buildHttpMessageOut[2] = 0;
  buildHttpMessageOut[3] = 0;

  const buf = buildHttpMessage(
    firstLine,
    flat,
    body,
    bodyEncoding,
    buildFlags(msg),
    date,
    keepAliveSec,
    maxReq,
    knownLength,
    buildHttpMessageOut,
  );
  if (!buf) return false;

  applyBuildResult(msg, buildHttpMessageOut);

  if (msg.chunkedEncoding && (msg.statusCode === 204 ||
                              msg.statusCode === 304)) {
    // Should not happen with single-shot body preference, but stay safe.
    return false;
  }

  if (typeof callback === 'function')
    msg.once('finish', callback);

  // Mark headers as rendered+sent so subsequent writes fail correctly.
  msg._header = buf.toString('latin1');
  msg._headerSent = true;
  msg.finished = true;
  if (bodyLen && msg._hasBody) {
    msg[kBytesWritten] = bodyLen;
  }

  const finish = onFinish.bind(undefined, msg);

  // The socket is typically corked by OutgoingMessage.socket setter /
  // assignSocket. Mirror end()'s uncork so the single write is flushed;
  // without this the write callback never runs and the response hangs.
  if (!socket.writableCorked) {
    socket.cork();
  }
  const ret = socket.write(buf, finish);
  // Force a full uncork (same as the slow end() path).
  socket._writableState.corked = 1;
  socket.uncork();

  if (!ret) msg[kNeedDrain] = true;

  debug('outgoing message fast end.');
  if (msg.outputData.length === 0 && socket._httpMessage === msg) {
    msg._finish();
  }
  return true;
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
  parseUniqueHeadersOption,
  validateHeaderName,
  validateHeaderValue,
  OutgoingMessage,
};

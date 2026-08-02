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
} = primordials;

const { getDefaultHighWaterMark } = require('internal/streams/state');
const {
  kDestroyMessageBuffer,
  kInternalWritev,
  kPendingMessageBytes,
  kRawWritev,
} = require('internal/streams/utils');
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
    ERR_UNKNOWN_ENCODING,
  },
  hideStackFrames,
} = require('internal/errors');
const { validateString } = require('internal/validators');
const {
  assignFunctionName,
  deprecateInstantiation,
} = require('internal/util');
const { isUint8Array } = require('internal/util/types');

let debug = require('internal/util/debuglog').debuglog('http', (fn) => {
  debug = fn;
});

const kCorked = Symbol('corked');
const kAutoCorked = Symbol('autoCorked');
const kSocket = Symbol('kSocket');
const kWriteBuffer = Symbol('kWriteBuffer');
const kWriteCallbacks = Symbol('kWriteCallbacks');
const kBufferedLength = Symbol('kBufferedLength');
const kBufferedOutputSize = Symbol('kBufferedOutputSize');
const kUniqueHeaders = Symbol('kUniqueHeaders');
const kBytesWritten = Symbol('kBytesWritten');
const kErrored = Symbol('errored');
const kHighWaterMark = Symbol('kHighWaterMark');
const kRejectNonStandardBodyWrites = Symbol('kRejectNonStandardBodyWrites');
const kMaxChunkedFramingFoldLength = 1024;

const nop = () => {};

const RE_CONN_CLOSE = /(?:^|\W)close(?:$|\W)/i;

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
  if (!(this instanceof OutgoingMessage)) {
    return deprecateInstantiation(OutgoingMessage, 'DEP0195', options);
  }

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
  this[kAutoCorked] = false;
  this[kWriteBuffer] = null;
  this[kWriteCallbacks] = null;
  this[kBufferedLength] = 0;
  this[kBufferedOutputSize] = 0;
  this._closed = false;

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
OutgoingMessage.prototype._isLenientHeaderValidation = function() {
  // New httpValidation option takes priority (ClientRequest case)
  if (this.httpValidation !== undefined) {
    return this.httpValidation !== 'strict';
  }
  // ServerResponse: check server's httpValidation option
  const serverHttpValidation = this.req?.socket?.server?.httpValidation;
  if (serverHttpValidation !== undefined) {
    return serverHttpValidation !== 'strict';
  }
  // Legacy insecureHTTPParser - ClientRequest has it directly
  if (typeof this.insecureHTTPParser === 'boolean') {
    return this.insecureHTTPParser;
  }
  // ServerResponse can access via req.socket.server
  const serverOption = this.req?.socket?.server?.insecureHTTPParser;
  if (typeof serverOption === 'boolean') {
    return serverOption;
  }
  // Fall back to global option
  return isLenient();
};

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
    return this.outputSize + this[kPendingMessageBytes]() +
      (this[kSocket] ? this[kSocket].writableLength : 0);
  },
});

OutgoingMessage.prototype[kPendingMessageBytes] = function() {
  const buf = this[kWriteBuffer];
  if (buf === null || buf.length === 0) {
    return 0;
  }

  const len = this[kBufferedLength];
  let pending = len;
  if (this.chunkedEncoding && len !== 0) {
    let hexLength;
    if (len < 0x100) {
      hexLength = len < 0x10 ? 1 : 2;
    } else if (len < 0x10000) {
      hexLength = len < 0x1000 ? 3 : 4;
    } else if (len < 0x1000000) {
      hexLength = len < 0x100000 ? 5 : 6;
    } else if (len < 0x100000000) {
      hexLength = len < 0x10000000 ? 7 : 8;
    } else {
      hexLength = len.toString(16).length;
    }
    pending += hexLength + 4;
  }
  if (!this._headerSent && this._header !== null) {
    pending += this._header.length;
  }
  return pending;
};

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

function canCombineAscii(data, encoding) {
  return typeof data === 'string' &&
    (encoding === 'utf8' || encoding === 'latin1' || !encoding);
}

// Above 1 KiB, preserving separate vectors is faster than flattening the
// V8 cons string created by adjoining HTTP chunk framing.
function canFoldChunkedFraming(data, encoding, byteLength) {
  return byteLength <= kMaxChunkedFramingFoldLength &&
    canCombineAscii(data, encoding);
}

function canFoldChunkedPrefix(msg, encoding) {
  return msg._headerSent || encoding === 'latin1' ||
    Buffer.byteLength(msg._header) === msg._header.length;
}

function sendWriteVector(msg, chunks, callback) {
  const conn = msg[kSocket];
  if (conn && conn._httpMessage === msg && conn.writable &&
      typeof conn[kInternalWritev] === 'function' &&
      typeof conn[kRawWritev] === 'function') {
    if (msg.outputData.length !== 0) {
      msg._flushOutput(conn);
    }

    if (!msg._headerSent && msg._header !== null) {
      if (canCombineAscii(chunks[0], chunks[1])) {
        chunks[0] = msg._header + chunks[0];
      } else {
        chunks.unshift(msg._header, 'latin1');
      }
      msg._headerSent = true;
    }
    return conn[kInternalWritev](chunks, callback);
  }

  for (let i = 0; i < chunks.length - 2; i += 2) {
    msg._send(chunks[i], chunks[i + 1], null);
  }
  const ret = msg._send(
    chunks[chunks.length - 2],
    chunks[chunks.length - 1],
    callback,
  );
  return ret;
}

function writeChunkedVector(msg, chunk, encoding, callback, len) {
  if (canFoldChunkedFraming(chunk, encoding, len)) {
    const framedChunk = chunk + '\r\n';
    if (!canFoldChunkedPrefix(msg, encoding)) {
      return sendWriteVector(
        msg,
        [len.toString(16) + '\r\n', 'latin1', framedChunk, encoding],
        callback,
      );
    }
    return sendWriteVector(
      msg,
      [len.toString(16) + '\r\n' + framedChunk, encoding],
      callback,
    );
  }
  return sendWriteVector(msg, [
    len.toString(16) + '\r\n', 'latin1',
    chunk, encoding,
    crlf_buf, null,
  ], callback);
}

function bufferWriteCallback(msg, callback) {
  const callbacks = msg[kWriteCallbacks];
  if (callbacks === null) {
    msg[kWriteCallbacks] = callback;
  } else if (typeof callbacks === 'function') {
    msg[kWriteCallbacks] = [callbacks, callback];
  } else {
    callbacks.push(callback);
  }
}

function callWriteCallbacks(callbacks, error) {
  if (typeof callbacks === 'function') {
    callbacks(error);
  } else {
    for (let n = 0; n < callbacks.length; n++) {
      callbacks[n](error);
    }
  }
}

function updateBufferedOutputSize(msg, size) {
  const delta = size - msg[kBufferedOutputSize];
  if (delta !== 0) {
    msg[kBufferedOutputSize] = size;
    msg._onPendingData(delta);
  }
}

function releaseBufferedOutputSize(msg) {
  updateBufferedOutputSize(msg, 0);
}

function flushWriteBuffer(msg, ending = false, finalCallback = null) {
  if (msg.destroyed || msg[kSocket]?.destroyed) {
    destroyWriteBuffer(
      msg,
      msg[kErrored] ?? msg[kSocket]?._writableState?.errored,
    );
    return false;
  }

  const buf = msg[kWriteBuffer];
  const len = msg[kBufferedLength];
  const chunked = msg.chunkedEncoding;
  const callbacks = msg[kWriteCallbacks];

  // The vector may be retained by the stream until an asynchronous write
  // completes. Transfer ownership instead of copying it into another array.
  msg[kWriteBuffer] = null;
  msg[kWriteCallbacks] = null;
  msg[kBufferedLength] = 0;

  let callback = finalCallback;
  if (callbacks !== null) {
    if (typeof callbacks === 'function' && finalCallback === null) {
      callback = callbacks;
    } else {
      callback = (err) => {
        callWriteCallbacks(callbacks, err);
        if (finalCallback !== null) {
          finalCallback(err);
        }
      };
    }
  }

  // A message can cross its byte-based high-water mark even when Writable's
  // string-length accounting stays below the socket high-water mark. Recheck
  // message-level drain when an asynchronous vector completes; the socket's
  // own drain event is not guaranteed in that case.
  if (msg[kNeedDrain]) {
    const writeCallback = callback;
    callback = (error) => {
      if (error === null || error === undefined) {
        emitDrainIfNeeded(msg);
      }
      if (writeCallback !== null) {
        writeCallback(error);
      }
    };
  }

  if (chunked) {
    const prefix = len.toString(16) + '\r\n';
    const last = buf.length - 2;
    let foldSuffix = false;
    if ((!ending || msg._trailer.length === 0) &&
        canCombineAscii(buf[last], buf[last + 1])) {
      const singlePayload = buf.length === 2 ||
        (buf[0] === null && buf.length === 4);
      const lastLength = singlePayload ? len :
        Buffer.byteLength(buf[last], buf[last + 1]);
      foldSuffix = lastLength <= kMaxChunkedFramingFoldLength;
    }
    if (buf[0] === null) {
      buf[0] = prefix;
    } else {
      buf[0] = prefix + buf[0];
    }

    const suffix = ending ?
      '\r\n0\r\n' + msg._trailer + '\r\n' : '\r\n';
    if (foldSuffix) {
      buf[last] += suffix;
    } else {
      buf.push(
        ending ? suffix : crlf_buf,
        ending ? 'latin1' : null,
      );
    }
  }

  try {
    sendWriteVector(msg, buf, callback);
  } finally {
    // Inactive messages contribute their message-level buffer to the
    // connection-wide pending-data counter. Release that ownership only
    // after the vector has either reached the socket or outputData.
    releaseBufferedOutputSize(msg);
  }
  return true;
}

function destroyWriteBuffer(msg, error) {
  msg[kAutoCorked] = false;
  const buf = msg[kWriteBuffer];
  const callbacks = msg[kWriteCallbacks];
  if ((buf === null || buf.length === 0) && callbacks === null) {
    return;
  }

  const callbackError = error || new ERR_STREAM_DESTROYED('write');
  msg[kWriteBuffer] = null;
  msg[kWriteCallbacks] = null;
  msg[kBufferedLength] = 0;
  releaseBufferedOutputSize(msg);
  if (callbacks !== null) {
    if (typeof callbacks === 'function') {
      process.nextTick(callbacks, callbackError);
    } else {
      process.nextTick(callWriteCallbacks, callbacks, callbackError);
    }
  }
}

OutgoingMessage.prototype[kDestroyMessageBuffer] = function(error) {
  destroyWriteBuffer(this, error);
};

function emitDrainIfNeeded(msg) {
  if (msg[kNeedDrain] && msg.writableLength === 0) {
    msg[kNeedDrain] = false;
    msg.emit('drain');
  }
}

OutgoingMessage.prototype.uncork = function uncork() {
  if (!this[kCorked]) {
    return;
  }
  this[kCorked]--;

  const hasBufferedWrites = !this[kCorked] && this[kWriteBuffer] !== null &&
    this[kWriteBuffer].length !== 0;
  let flushed = false;
  try {
    if (hasBufferedWrites) {
      flushed = flushWriteBuffer(this);
    }
  } finally {
    if (this[kSocket]) {
      this[kSocket].uncork();
    }
  }

  if (!flushed) {
    return;
  }

  // If we had a pending drain and flushed all data, emit the drain event.
  emitDrainIfNeeded(this);
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
    // Settle writes that have not reached Writable before closing the message.
    try {
      this[kDestroyMessageBuffer](error);
    } finally {
      this[kSocket].destroy(error);
    }
  } else {
    this[kDestroyMessageBuffer](error);
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
  if (!this._headerSent && this._header !== null) {
    // `this._header` can be null if OutgoingMessage is used without a proper Socket
    // See: /test/parallel/test-http-outgoing-message-inheritance.js
    if (canCombineAscii(data, encoding)) {
      data = this._header + data;
    } else {
      const header = this._header;
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
  const lenient = this._isLenientHeaderValidation();

  if (headers) {
    if (headers === this[kOutHeaders]) {
      for (const key in headers) {
        const entry = headers[key];
        processHeader(this, state, entry[0], entry[1], false, lenient);
      }
    } else if (ArrayIsArray(headers)) {
      if (headers.length && ArrayIsArray(headers[0])) {
        for (let i = 0; i < headers.length; i++) {
          const entry = headers[i];
          processHeader(this, state, entry[0], entry[1], true, lenient);
        }
      } else {
        if (headers.length % 2 !== 0) {
          throw new ERR_INVALID_ARG_VALUE('headers', headers);
        }

        for (let n = 0; n < headers.length; n += 2) {
          processHeader(this, state, headers[n + 0], headers[n + 1], true, lenient);
        }
      }
    } else {
      for (const key in headers) {
        if (ObjectHasOwn(headers, key)) {
          processHeader(this, state, key, headers[key], true, lenient);
        }
      }
    }
  }

  let { header } = state;

  // Date header
  if (this.sendDate && !state.date) {
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
  if (this.chunkedEncoding && (this.statusCode === 204 ||
                               this.statusCode === 304)) {
    debug(this.statusCode + ' response should not use chunked encoding,' +
          ' closing connection.');
    this.chunkedEncoding = false;
    this.shouldKeepAlive = false;
  }

  // keep-alive logic
  if (this._removedConnection) {
    // shouldKeepAlive is generally true for HTTP/1.1. In that common case,
    // even if the connection header isn't sent, we still persist by default.
    this._last = !this.shouldKeepAlive;
  } else if (!state.connection) {
    const shouldSendKeepAlive = this.shouldKeepAlive &&
        (state.contLen || this.useChunkedEncodingByDefault || this.agent);
    if (shouldSendKeepAlive && this.maxRequestsOnConnectionReached) {
      header += 'Connection: close\r\n';
    } else if (shouldSendKeepAlive) {
      header += 'Connection: keep-alive\r\n';
      if (this._keepAliveTimeout && this._defaultKeepAlive) {
        const timeoutSeconds = MathFloor(this._keepAliveTimeout / 1000);
        let max = '';
        if (~~this._maxRequestsPerSocket > 0) {
          max = `, max=${this._maxRequestsPerSocket}`;
        }
        header += `Keep-Alive: timeout=${timeoutSeconds}${max}\r\n`;
      }
    } else {
      this._last = true;
      header += 'Connection: close\r\n';
    }
  }

  if (!state.contLen && !state.te) {
    if (!this._hasBody) {
      // Make sure we don't end the 0\r\n\r\n at the end of the message.
      this.chunkedEncoding = false;
    } else if (!this.useChunkedEncodingByDefault) {
      this._last = true;
    } else if (!state.trailer &&
               !this._removedContLen &&
               typeof this._contentLength === 'number') {
      header += 'Content-Length: ' + this._contentLength + '\r\n';
    } else if (!this._removedTE) {
      header += 'Transfer-Encoding: chunked\r\n';
      this.chunkedEncoding = true;
    } else {
      // We should only be able to get here if both Content-Length and
      // Transfer-Encoding are removed by the user.
      // See: test/parallel/test-http-remove-header-stays-removed.js
      debug('Both Content-Length and Transfer-Encoding are removed');

      // We can't keep alive in this case, because with no header info the body
      // is defined as all data until the connection is closed.
      this._last = true;
    }
  }

  // Test non-chunked message does not have trailer header set,
  // message will be terminated by the first empty line after the
  // header fields, regardless of the header fields present in the
  // message, and thus cannot contain a message body or 'trailers'.
  if (this.chunkedEncoding !== true && state.trailer) {
    throw new ERR_HTTP_TRAILER_INVALID();
  }

  this._header = header + '\r\n';
  this._headerSent = false;

  // Wait until the first body chunk, or close(), is sent to flush,
  // UNLESS we're sending Expect: 100-continue.
  if (state.expect) this._send('');
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
    if (
      (value.length < 2 || !isCookieField(key)) &&
      (!self[kUniqueHeaders] || !self[kUniqueHeaders].has(key.toLowerCase()))
    ) {
      // Retain for(;;) loop for performance reasons
      // Refs: https://github.com/nodejs/node/pull/30958
      for (let i = 0; i < value.length; i++)
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

function isHeaderField(field, lowerCase, canonicalCase) {
  return field === lowerCase || field === canonicalCase ||
    field.toLowerCase() === lowerCase;
}

function matchHeader(self, state, field, value) {
  const len = field.length;
  switch (field.charCodeAt(0) | 0x20) {
    case 0x63: // c
      if (len === 10 &&
          isHeaderField(field, 'connection', 'Connection')) {
        state.connection = true;
        self._removedConnection = false;
        if (RE_CONN_CLOSE.test(value))
          self._last = true;
        else
          self.shouldKeepAlive = true;
      } else if (len === 14 &&
                 isHeaderField(field, 'content-length', 'Content-Length')) {
        state.contLen = true;
        self._contentLength = +value;
        self._removedContLen = false;
      }
      break;
    case 0x64: // d
      if (len === 4 && isHeaderField(field, 'date', 'Date'))
        state.date = true;
      break;
    case 0x65: // e
      if (len === 6 && isHeaderField(field, 'expect', 'Expect'))
        state.expect = true;
      break;
    case 0x6b: // k
      if (len === 10 && isHeaderField(field, 'keep-alive', 'Keep-Alive'))
        self._defaultKeepAlive = false;
      break;
    case 0x74: // t
      if (len === 7 && isHeaderField(field, 'trailer', 'Trailer')) {
        state.trailer = true;
      } else if (len === 17 &&
                 isHeaderField(field,
                               'transfer-encoding',
                               'Transfer-Encoding')) {
        state.te = true;
        self._removedTE = false;
        if (RE_TE_CHUNKED.test(value))
          self.chunkedEncoding = true;
      }
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

  const socket = msg.socket;
  const activeSocket = socket?._httpMessage === msg && socket.writable;
  if (!fromEnd && socket && !socket.writableCorked) {
    socket.cork();
    msg[kAutoCorked] = true;
    process.nextTick(connectionCorkNT, msg, socket);
  }

  let ret;
  const chunked = msg.chunkedEncoding;
  const bufferable = chunked || msg._contentLength !== null;
  const buf = msg[kWriteBuffer];
  const buffering = bufferable &&
    ((chunk.length !== 0 &&
      (msg[kAutoCorked] ||
       (fromEnd && socket?._httpMessage === msg) ||
       msg[kCorked])) ||
     (chunk.length === 0 && buf !== null && buf.length !== 0));

  if (buffering && encoding &&
      (encoding === 'buffer' ? typeof chunk === 'string' :
        !Buffer.isEncoding(encoding))) {
    throw new ERR_UNKNOWN_ENCODING(encoding);
  }

  if (buffering) {
    const trackPendingData = !activeSocket;
    if (chunk.length === 0) {
      // Preserve callback ordering without adding an empty socket write.
      if (callback !== nop) {
        bufferWriteCallback(msg, callback);
      }
    } else {
      len ??= typeof chunk === 'string' ?
        Buffer.byteLength(chunk, encoding) : chunk.byteLength;
      // Writable normalizes Uint8Array views synchronously. Preserve that
      // timing when the message-level buffer replaces an immediate
      // Socket.write(), including its detached-ArrayBuffer behavior.
      if (activeSocket && typeof chunk !== 'string' &&
          !(chunk instanceof Buffer)) {
        chunk = Stream._uint8ArrayToBuffer(chunk);
      }
      let writeBuffer = buf;
      if (writeBuffer === null) {
        writeBuffer = chunked &&
          (!canFoldChunkedFraming(chunk, encoding, len) ||
           !canFoldChunkedPrefix(msg, encoding)) ?
          [null, 'latin1'] : [];
        msg[kWriteBuffer] = writeBuffer;
      }
      writeBuffer.push(chunk, encoding);
      if (callback !== nop) {
        bufferWriteCallback(msg, callback);
      }
      msg[kBufferedLength] += len;
    }
    if (trackPendingData) {
      updateBufferedOutputSize(msg, msg[kPendingMessageBytes]());
    }
    ret = msg.writableLength < msg.writableHighWaterMark;
  } else if (bufferable && chunk.length !== 0) {
    len ??= typeof chunk === 'string' ? Buffer.byteLength(chunk, encoding) : chunk.byteLength;
    if (chunked) {
      ret = writeChunkedVector(msg, chunk, encoding, callback, len);
    } else {
      ret = msg._send(chunk, encoding, callback, len);
    }
  } else {
    ret = msg._send(chunk, encoding, callback, len);
  }

  debug('write ret = ' + ret);
  return ret;
}


function connectionCorkNT(msg, conn) {
  let flushed = false;
  try {
    if (msg[kAutoCorked]) {
      msg[kAutoCorked] = false;
      const hasBufferedWrites = !msg[kCorked] &&
        msg[kWriteBuffer] !== null &&
        msg[kWriteBuffer].length !== 0;
      if (hasBufferedWrites) {
        flushed = flushWriteBuffer(msg);
      }
    }
  } finally {
    conn.uncork();
  }

  if (flushed) {
    emitDrainIfNeeded(msg);
  }
}

OutgoingMessage.prototype.addTrailers = function addTrailers(headers) {
  if (this.finished) {
    throw new ERR_HTTP_HEADERS_SENT('set trailing');
  }

  this._trailer = '';
  const keys = ObjectKeys(headers);
  const isArray = ArrayIsArray(headers);
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
    const lenient = this._isLenientHeaderValidation();
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

    write_(this, chunk, encoding, null, true);
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

  // Flush message-level corked data together with the terminating chunk.
  // Keep the socket corked so all HTTP framing is one logical write.
  const hasBufferedWrites = this[kWriteBuffer] !== null &&
    this[kWriteBuffer].length !== 0;
  let flushed = false;

  if (hasBufferedWrites) {
    flushed = flushWriteBuffer(this, true, finish);
  } else if (this._hasBody && this.chunkedEncoding) {
    sendWriteVector(
      this,
      ['0\r\n' + this._trailer + '\r\n', 'latin1'],
      finish,
    );
  } else if (!this._headerSent || this.writableLength || chunk) {
    sendWriteVector(this, ['', 'latin1'], finish);
  } else {
    process.nextTick(finish);
  }

  if (this[kSocket]) {
    // Fully uncork connection on end().
    this[kAutoCorked] = false;
    this[kSocket]._writableState.corked = 1;
    this[kSocket].uncork();
  }
  this[kCorked] = 1;
  this.uncork();

  // Mark the message as ended before emitting drain. A synchronous drain
  // listener must not be able to write after the terminating chunk.
  this.finished = true;

  if (flushed) {
    emitDrainIfNeeded(this);
  }

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
  if (socket._httpMessage === this && socket.writable &&
      typeof socket[kInternalWritev] === 'function' &&
      typeof socket[kRawWritev] === 'function') {
    const vector = new Array(outputLength << 1);
    let callbacks = null;
    for (let i = 0; i < outputLength; i++) {
      const entry = outputData[i];
      vector[i * 2] = entry.data;
      vector[i * 2 + 1] = entry.encoding;
      if (entry.callback !== null && entry.callback !== undefined &&
          entry.callback !== nop) {
        if (callbacks === null) {
          callbacks = entry.callback;
        } else if (typeof callbacks === 'function') {
          callbacks = [callbacks, entry.callback];
        } else {
          callbacks.push(entry.callback);
        }
      }
    }

    let callback = callbacks;
    if (callbacks !== null && typeof callbacks !== 'function') {
      callback = (error) => callWriteCallbacks(callbacks, error);
    }

    const ret = socket[kInternalWritev](vector, callback);
    this.outputData = [];
    this._onPendingData(-this.outputSize);
    this.outputSize = 0;
    return ret;
  }

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

  // Force-flush the headers. If an inactive message already owns a buffered
  // contribution, move the header bytes from that contribution to outputData
  // instead of counting the same bytes in both places.
  try {
    this._send('');
  } finally {
    if (this[kBufferedOutputSize] !== 0) {
      updateBufferedOutputSize(this, this[kPendingMessageBytes]());
    }
  }
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

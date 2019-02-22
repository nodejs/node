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

const assert = require('internal/assert');
const Stream = require('stream');
const util = require('util');
const internalUtil = require('internal/util');
const { outHeadersKey, utcDate } = require('internal/http');
const { Buffer } = require('buffer');
const common = require('_http_common');
const checkIsHttpToken = common._checkIsHttpToken;
const checkInvalidHeaderChar = common._checkInvalidHeaderChar;
const {
  defaultTriggerAsyncIdScope,
  symbols: { async_id_symbol }
} = require('internal/async_hooks');
const {
  ERR_HTTP_HEADERS_SENT,
  ERR_HTTP_INVALID_HEADER_VALUE,
  ERR_HTTP_TRAILER_INVALID,
  ERR_INVALID_HTTP_TOKEN,
  ERR_INVALID_ARG_TYPE,
  ERR_INVALID_CHAR,
  ERR_METHOD_NOT_IMPLEMENTED,
  ERR_STREAM_CANNOT_PIPE,
  ERR_STREAM_WRITE_AFTER_END
} = require('internal/errors').codes;
const { validateString } = require('internal/validators');

const { CRLF, debug } = common;

const kIsCorked = Symbol('isCorked');

const hasOwnProperty = Function.call.bind(Object.prototype.hasOwnProperty);

var RE_CONN_CLOSE = /(?:^|\W)close(?:$|\W)/i;
var RE_TE_CHUNKED = common.chunkExpression;

// isCookieField performs a case-insensitive comparison of a provided string
// against the word "cookie." As of V8 6.6 this is faster than handrolling or
// using a case-insensitive RegExp.
function isCookieField(s) {
  return s.length === 6 && s.toLowerCase() === 'cookie';
}

function noopPendingOutput(amount) {}

function OutgoingMessage() {
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

  this._last = false;
  this.chunkedEncoding = false;
  this.shouldKeepAlive = true;
  this.useChunkedEncodingByDefault = true;
  this.sendDate = false;
  this._removedConnection = false;
  this._removedContLen = false;
  this._removedTE = false;

  this._contentLength = null;
  this._hasBody = true;
  this._trailer = '';

  this.finished = false;
  this._headerSent = false;
  this[kIsCorked] = false;

  this.socket = null;
  this.connection = null;
  this._header = null;
  this[outHeadersKey] = null;

  this._onPendingData = noopPendingOutput;
}
Object.setPrototypeOf(OutgoingMessage.prototype, Stream.prototype);
Object.setPrototypeOf(OutgoingMessage, Stream);


Object.defineProperty(OutgoingMessage.prototype, '_headers', {
  get: util.deprecate(function() {
    return this.getHeaders();
  }, 'OutgoingMessage.prototype._headers is deprecated', 'DEP0066'),
  set: util.deprecate(function(val) {
    if (val == null) {
      this[outHeadersKey] = null;
    } else if (typeof val === 'object') {
      const headers = this[outHeadersKey] = Object.create(null);
      const keys = Object.keys(val);
      for (var i = 0; i < keys.length; ++i) {
        const name = keys[i];
        headers[name.toLowerCase()] = [name, val[name]];
      }
    }
  }, 'OutgoingMessage.prototype._headers is deprecated', 'DEP0066')
});

Object.defineProperty(OutgoingMessage.prototype, '_headerNames', {
  get: util.deprecate(function() {
    const headers = this[outHeadersKey];
    if (headers !== null) {
      const out = Object.create(null);
      const keys = Object.keys(headers);
      for (var i = 0; i < keys.length; ++i) {
        const key = keys[i];
        const val = headers[key][0];
        out[key] = val;
      }
      return out;
    }
    return null;
  }, 'OutgoingMessage.prototype._headerNames is deprecated', 'DEP0066'),
  set: util.deprecate(function(val) {
    if (typeof val === 'object' && val !== null) {
      const headers = this[outHeadersKey];
      if (!headers)
        return;
      const keys = Object.keys(val);
      for (var i = 0; i < keys.length; ++i) {
        const header = headers[keys[i]];
        if (header)
          header[0] = val[keys[i]];
      }
    }
  }, 'OutgoingMessage.prototype._headerNames is deprecated', 'DEP0066')
});


OutgoingMessage.prototype._renderHeaders = function _renderHeaders() {
  if (this._header) {
    throw new ERR_HTTP_HEADERS_SENT('render');
  }

  var headersMap = this[outHeadersKey];
  const headers = {};

  if (headersMap !== null) {
    const keys = Object.keys(headersMap);
    for (var i = 0, l = keys.length; i < l; i++) {
      const key = keys[i];
      headers[headersMap[key][0]] = headersMap[key][1];
    }
  }
  return headers;
};


OutgoingMessage.prototype.setTimeout = function setTimeout(msecs, callback) {

  if (callback) {
    this.on('timeout', callback);
  }

  if (!this.socket) {
    this.once('socket', function socketSetTimeoutOnConnect(socket) {
      socket.setTimeout(msecs);
    });
  } else {
    this.socket.setTimeout(msecs);
  }
  return this;
};


// It's possible that the socket will be destroyed, and removed from
// any messages, before ever calling this.  In that case, just skip
// it, since something else is destroying this connection anyway.
OutgoingMessage.prototype.destroy = function destroy(error) {
  if (this.socket) {
    this.socket.destroy(error);
  } else {
    this.once('socket', function socketDestroyOnConnect(socket) {
      socket.destroy(error);
    });
  }
};


// This abstract either writing directly to the socket or buffering it.
OutgoingMessage.prototype._send = function _send(data, encoding, callback) {
  // This is a shameful hack to get the headers and first body chunk onto
  // the same packet. Future versions of Node are going to take care of
  // this at a lower level and in a more general way.
  if (!this._headerSent) {
    if (typeof data === 'string' &&
        (encoding === 'utf8' || encoding === 'latin1' || !encoding)) {
      data = this._header + data;
    } else {
      var header = this._header;
      if (this.outputData.length === 0) {
        this.outputData = [{
          data: header,
          encoding: 'latin1',
          callback: null
        }];
      } else {
        this.outputData.unshift({
          data: header,
          encoding: 'latin1',
          callback: null
        });
      }
      this.outputSize += header.length;
      this._onPendingData(header.length);
    }
    this._headerSent = true;
  }
  return this._writeRaw(data, encoding, callback);
};


OutgoingMessage.prototype._writeRaw = _writeRaw;
function _writeRaw(data, encoding, callback) {
  const conn = this.connection;
  if (conn && conn.destroyed) {
    // The socket was destroyed. If we're still trying to write to it,
    // then we haven't gotten the 'close' event yet.
    return false;
  }

  if (typeof encoding === 'function') {
    callback = encoding;
    encoding = null;
  }

  if (conn && conn._httpMessage === this && conn.writable && !conn.destroyed) {
    // There might be pending data in the this.output buffer.
    if (this.outputData.length) {
      this._flushOutput(conn);
    } else if (!data.length) {
      if (typeof callback === 'function') {
        // If the socket was set directly it won't be correctly initialized
        // with an async_id_symbol.
        // TODO(AndreasMadsen): @trevnorris suggested some more correct
        // solutions in:
        // https://github.com/nodejs/node/pull/14389/files#r128522202
        defaultTriggerAsyncIdScope(conn[async_id_symbol],
                                   process.nextTick,
                                   callback);
      }
      return true;
    }
    // Directly write to socket.
    return conn.write(data, encoding, callback);
  }
  // Buffer, as long as we're not destroyed.
  this.outputData.push({ data, encoding, callback });
  this.outputSize += data.length;
  this._onPendingData(data.length);
  return false;
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
    header: firstLine
  };

  var key;
  if (headers === this[outHeadersKey]) {
    for (key in headers) {
      const entry = headers[key];
      processHeader(this, state, entry[0], entry[1], false);
    }
  } else if (Array.isArray(headers)) {
    for (var i = 0; i < headers.length; i++) {
      const entry = headers[i];
      processHeader(this, state, entry[0], entry[1], true);
    }
  } else if (headers) {
    for (key in headers) {
      if (hasOwnProperty(headers, key)) {
        processHeader(this, state, key, headers[key], true);
      }
    }
  }

  let { header } = state;

  // Date header
  if (this.sendDate && !state.date) {
    header += 'Date: ' + utcDate() + CRLF;
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
    this._last = true;
    this.shouldKeepAlive = false;
  } else if (!state.connection) {
    const shouldSendKeepAlive = this.shouldKeepAlive &&
        (state.contLen || this.useChunkedEncodingByDefault || this.agent);
    if (shouldSendKeepAlive) {
      header += 'Connection: keep-alive\r\n';
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
      header += 'Content-Length: ' + this._contentLength + CRLF;
    } else if (!this._removedTE) {
      header += 'Transfer-Encoding: chunked\r\n';
      this.chunkedEncoding = true;
    } else {
      // We should only be able to get here if both Content-Length and
      // Transfer-Encoding are removed by the user.
      // See: test/parallel/test-http-remove-header-stays-removed.js
      debug('Both Content-Length and Transfer-Encoding are removed');
    }
  }

  // Test non-chunked message does not have trailer header set,
  // message will be terminated by the first empty line after the
  // header fields, regardless of the header fields present in the
  // message, and thus cannot contain a message body or 'trailers'.
  if (this.chunkedEncoding !== true && state.trailer) {
    throw new ERR_HTTP_TRAILER_INVALID();
  }

  this._header = header + CRLF;
  this._headerSent = false;

  // Wait until the first body chunk, or close(), is sent to flush,
  // UNLESS we're sending Expect: 100-continue.
  if (state.expect) this._send('');
}

function processHeader(self, state, key, value, validate) {
  if (validate)
    validateHeaderName(key);
  if (Array.isArray(value)) {
    if (value.length < 2 || !isCookieField(key)) {
      for (var i = 0; i < value.length; i++)
        storeHeader(self, state, key, value[i], validate);
      return;
    }
    value = value.join('; ');
  }
  storeHeader(self, state, key, value, validate);
}

function storeHeader(self, state, key, value, validate) {
  if (validate)
    validateHeaderValue(key, value);
  state.header += key + ': ' + value + CRLF;
  matchHeader(self, state, key, value);
}

function matchHeader(self, state, field, value) {
  if (field.length < 4 || field.length > 17)
    return;
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
      if (RE_TE_CHUNKED.test(value)) self.chunkedEncoding = true;
      break;
    case 'content-length':
      state.contLen = true;
      self._removedContLen = false;
      break;
    case 'date':
    case 'expect':
    case 'trailer':
      state[field] = true;
      break;
  }
}

function validateHeaderName(name) {
  if (typeof name !== 'string' || !name || !checkIsHttpToken(name)) {
    // Reducing the limit improves the performance significantly. We do not
    // lose the stack frames due to the `captureStackTrace()` function that is
    // called later.
    const tmpLimit = Error.stackTraceLimit;
    Error.stackTraceLimit = 0;
    const err = new ERR_INVALID_HTTP_TOKEN('Header name', name);
    Error.stackTraceLimit = tmpLimit;
    Error.captureStackTrace(err, validateHeaderName);
    throw err;
  }
}

function validateHeaderValue(name, value) {
  let err;
  // Reducing the limit improves the performance significantly. We do not loose
  // the stack frames due to the `captureStackTrace()` function that is called
  // later.
  const tmpLimit = Error.stackTraceLimit;
  Error.stackTraceLimit = 0;
  if (value === undefined) {
    err = new ERR_HTTP_INVALID_HEADER_VALUE(value, name);
  } else if (checkInvalidHeaderChar(value)) {
    debug('Header "%s" contains invalid characters', name);
    err = new ERR_INVALID_CHAR('header content', name);
  }
  Error.stackTraceLimit = tmpLimit;
  if (err !== undefined) {
    Error.captureStackTrace(err, validateHeaderValue);
    throw err;
  }
}

OutgoingMessage.prototype.setHeader = function setHeader(name, value) {
  if (this._header) {
    throw new ERR_HTTP_HEADERS_SENT('set');
  }
  validateHeaderName(name);
  validateHeaderValue(name, value);

  let headers = this[outHeadersKey];
  if (headers === null)
    this[outHeadersKey] = headers = Object.create(null);

  headers[name.toLowerCase()] = [name, value];
};


OutgoingMessage.prototype.getHeader = function getHeader(name) {
  validateString(name, 'name');

  const headers = this[outHeadersKey];
  if (headers === null)
    return;

  const entry = headers[name.toLowerCase()];
  return entry && entry[1];
};


// Returns an array of the names of the current outgoing headers.
OutgoingMessage.prototype.getHeaderNames = function getHeaderNames() {
  return this[outHeadersKey] !== null ? Object.keys(this[outHeadersKey]) : [];
};


// Returns a shallow copy of the current outgoing headers.
OutgoingMessage.prototype.getHeaders = function getHeaders() {
  const headers = this[outHeadersKey];
  const ret = Object.create(null);
  if (headers) {
    const keys = Object.keys(headers);
    for (var i = 0; i < keys.length; ++i) {
      const key = keys[i];
      const val = headers[key][1];
      ret[key] = val;
    }
  }
  return ret;
};


OutgoingMessage.prototype.hasHeader = function hasHeader(name) {
  validateString(name, 'name');
  return this[outHeadersKey] !== null &&
    !!this[outHeadersKey][name.toLowerCase()];
};


OutgoingMessage.prototype.removeHeader = function removeHeader(name) {
  validateString(name, 'name');

  if (this._header) {
    throw new ERR_HTTP_HEADERS_SENT('remove');
  }

  var key = name.toLowerCase();

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

  if (this[outHeadersKey] !== null) {
    delete this[outHeadersKey][key];
  }
};


OutgoingMessage.prototype._implicitHeader = function _implicitHeader() {
  this.emit('error', new ERR_METHOD_NOT_IMPLEMENTED('_implicitHeader()'));
};

Object.defineProperty(OutgoingMessage.prototype, 'headersSent', {
  configurable: true,
  enumerable: true,
  get: function() { return !!this._header; }
});


const crlf_buf = Buffer.from('\r\n');
OutgoingMessage.prototype.write = function write(chunk, encoding, callback) {
  return write_(this, chunk, encoding, callback, false);
};

function write_(msg, chunk, encoding, callback, fromEnd) {
  if (msg.finished) {
    const err = new ERR_STREAM_WRITE_AFTER_END();
    const triggerAsyncId = msg.socket ? msg.socket[async_id_symbol] : undefined;
    defaultTriggerAsyncIdScope(triggerAsyncId,
                               process.nextTick,
                               writeAfterEndNT,
                               msg,
                               err,
                               callback);

    return true;
  }

  if (!msg._header) {
    msg._implicitHeader();
  }

  if (!msg._hasBody) {
    debug('This type of response MUST NOT have a body. ' +
          'Ignoring write() calls.');
    return true;
  }

  if (!fromEnd && typeof chunk !== 'string' && !(chunk instanceof Buffer)) {
    throw new ERR_INVALID_ARG_TYPE('first argument',
                                   ['string', 'Buffer'], chunk);
  }


  // If we get an empty string or buffer, then just do nothing, and
  // signal the user to keep writing.
  if (chunk.length === 0) {
    debug('received empty string or buffer and waiting for more input');
    return true;
  }

  if (!fromEnd && msg.connection && !msg[kIsCorked]) {
    msg.connection.cork();
    msg[kIsCorked] = true;
    process.nextTick(connectionCorkNT, msg, msg.connection);
  }

  var len, ret;
  if (msg.chunkedEncoding) {
    if (typeof chunk === 'string')
      len = Buffer.byteLength(chunk, encoding);
    else
      len = chunk.length;

    msg._send(len.toString(16), 'latin1', null);
    msg._send(crlf_buf, null, null);
    msg._send(chunk, encoding, null);
    ret = msg._send(crlf_buf, null, callback);
  } else {
    ret = msg._send(chunk, encoding, callback);
  }

  debug('write ret = ' + ret);
  return ret;
}


function writeAfterEndNT(msg, err, callback) {
  msg.emit('error', err);
  if (callback) callback(err);
}


function connectionCorkNT(msg, conn) {
  msg[kIsCorked] = false;
  conn.uncork();
}


OutgoingMessage.prototype.addTrailers = function addTrailers(headers) {
  this._trailer = '';
  var keys = Object.keys(headers);
  var isArray = Array.isArray(headers);
  var field, value;
  for (var i = 0, l = keys.length; i < l; i++) {
    var key = keys[i];
    if (isArray) {
      field = headers[key][0];
      value = headers[key][1];
    } else {
      field = key;
      value = headers[key];
    }
    if (typeof field !== 'string' || !field || !checkIsHttpToken(field)) {
      throw new ERR_INVALID_HTTP_TOKEN('Trailer name', field);
    }
    if (checkInvalidHeaderChar(value)) {
      debug('Trailer "%s" contains invalid characters', field);
      throw new ERR_INVALID_CHAR('trailer content', field);
    }
    this._trailer += field + ': ' + value + CRLF;
  }
};

function onFinish(outmsg) {
  outmsg.emit('finish');
}

OutgoingMessage.prototype.end = function end(chunk, encoding, callback) {
  if (typeof chunk === 'function') {
    callback = chunk;
    chunk = null;
  } else if (typeof encoding === 'function') {
    callback = encoding;
    encoding = null;
  }

  if (this.finished) {
    return this;
  }

  var uncork;
  if (chunk) {
    if (typeof chunk !== 'string' && !(chunk instanceof Buffer)) {
      throw new ERR_INVALID_ARG_TYPE('chunk', ['string', 'Buffer'], chunk);
    }
    if (!this._header) {
      if (typeof chunk === 'string')
        this._contentLength = Buffer.byteLength(chunk, encoding);
      else
        this._contentLength = chunk.length;
    }
    if (this.connection) {
      this.connection.cork();
      uncork = true;
    }
    write_(this, chunk, encoding, null, true);
  } else if (!this._header) {
    this._contentLength = 0;
    this._implicitHeader();
  }

  if (typeof callback === 'function')
    this.once('finish', callback);

  var finish = onFinish.bind(undefined, this);

  if (this._hasBody && this.chunkedEncoding) {
    this._send('0\r\n' + this._trailer + '\r\n', 'latin1', finish);
  } else {
    // Force a flush, HACK.
    this._send('', 'latin1', finish);
  }

  if (uncork)
    this.connection.uncork();

  this.finished = true;

  // There is the first message on the outgoing queue, and we've sent
  // everything to the socket.
  debug('outgoing message end.');
  if (this.outputData.length === 0 &&
      this.connection &&
      this.connection._httpMessage === this) {
    this._finish();
  }

  return this;
};


OutgoingMessage.prototype._finish = function _finish() {
  assert(this.connection);
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
// This function, outgoingFlush(), is called by both the Server and Client
// to attempt to flush any pending messages out to the socket.
OutgoingMessage.prototype._flush = function _flush() {
  var socket = this.socket;
  var ret;

  if (socket && socket.writable) {
    // There might be remaining data in this.output; write it out
    ret = this._flushOutput(socket);

    if (this.finished) {
      // This is a queue to the server or client to bring in the next this.
      this._finish();
    } else if (ret) {
      // This is necessary to prevent https from breaking
      this.emit('drain');
    }
  }
};

OutgoingMessage.prototype._flushOutput = function _flushOutput(socket) {
  var ret;
  var outputLength = this.outputData.length;
  if (outputLength <= 0)
    return ret;

  var outputData = this.outputData;
  socket.cork();
  for (var i = 0; i < outputLength; i++) {
    const { data, encoding, callback } = outputData[i];
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

OutgoingMessage.prototype.flush = internalUtil.deprecate(function() {
  this.flushHeaders();
}, 'OutgoingMessage.flush is deprecated. Use flushHeaders instead.', 'DEP0001');

OutgoingMessage.prototype.pipe = function pipe() {
  // OutgoingMessage should be write-only. Piping from it is disabled.
  this.emit('error', new ERR_STREAM_CANNOT_PIPE());
};

module.exports = {
  OutgoingMessage
};

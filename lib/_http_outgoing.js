'use strict';

const assert = require('assert').ok;
const Stream = require('stream');
const timers = require('timers');
const util = require('util');
const internalUtil = require('internal/util');
const Buffer = require('buffer').Buffer;
const common = require('_http_common');
const checkIsHttpToken = common._checkIsHttpToken;
const checkInvalidHeaderChar = common._checkInvalidHeaderChar;

const CRLF = common.CRLF;
const trfrEncChunkExpression = common.chunkExpression;
const debug = common.debug;

const upgradeExpression = /^Upgrade$/i;
const transferEncodingExpression = /^Transfer-Encoding$/i;
const contentLengthExpression = /^Content-Length$/i;
const dateExpression = /^Date$/i;
const expectExpression = /^Expect$/i;
const trailerExpression = /^Trailer$/i;
const connectionExpression = /^Connection$/i;
const connCloseExpression = /(^|\W)close(\W|$)/i;
const connUpgradeExpression = /(^|\W)upgrade(\W|$)/i;

const automaticHeaders = {
  connection: true,
  'content-length': true,
  'transfer-encoding': true,
  date: true
};


var dateCache;
function utcDate() {
  if (!dateCache) {
    var d = new Date();
    dateCache = d.toUTCString();
    timers.enroll(utcDate, 1000 - d.getMilliseconds());
    timers._unrefActive(utcDate);
  }
  return dateCache;
}
utcDate._onTimeout = function _onTimeout() {
  dateCache = undefined;
};


function OutgoingMessage() {
  Stream.call(this);

  // Queue that holds all currently pending data, until the response will be
  // assigned to the socket (until it will its turn in the HTTP pipeline).
  this.output = [];
  this.outputEncodings = [];
  this.outputCallbacks = [];

  // `outputSize` is an approximate measure of how much data is queued on this
  // response. `_onPendingData` will be invoked to update similar global
  // per-connection counter. That counter will be used to pause/unpause the
  // TCP socket and HTTP Parser and thus handle the backpressure.
  this.outputSize = 0;

  this.writable = true;

  this._last = false;
  this.upgrading = false;
  this.chunkedEncoding = false;
  this.shouldKeepAlive = true;
  this.useChunkedEncodingByDefault = true;
  this.sendDate = false;
  this._removedHeader = {};

  this._contentLength = null;
  this._hasBody = true;
  this._trailer = '';

  this.finished = false;
  this._headerSent = false;

  this.socket = null;
  this.connection = null;
  this._header = null;
  this._headers = null;
  this._headerNames = {};

  this._onPendingData = null;
}
util.inherits(OutgoingMessage, Stream);


exports.OutgoingMessage = OutgoingMessage;


OutgoingMessage.prototype.setTimeout = function setTimeout(msecs, callback) {

  if (callback) {
    this.on('timeout', callback);
  }

  if (!this.socket) {
    this.once('socket', function(socket) {
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
  if (this.socket)
    this.socket.destroy(error);
  else
    this.once('socket', function(socket) {
      socket.destroy(error);
    });
};


// This abstract either writing directly to the socket or buffering it.
OutgoingMessage.prototype._send = function _send(data, encoding, callback) {
  // This is a shameful hack to get the headers and first body chunk onto
  // the same packet. Future versions of Node are going to take care of
  // this at a lower level and in a more general way.
  if (!this._headerSent) {
    if (typeof data === 'string' &&
        encoding !== 'hex' &&
        encoding !== 'base64') {
      data = this._header + data;
    } else {
      this.output.unshift(this._header);
      this.outputEncodings.unshift('latin1');
      this.outputCallbacks.unshift(null);
      this.outputSize += this._header.length;
      if (typeof this._onPendingData === 'function')
        this._onPendingData(this._header.length);
    }
    this._headerSent = true;
  }
  return this._writeRaw(data, encoding, callback);
};


OutgoingMessage.prototype._writeRaw = _writeRaw;
function _writeRaw(data, encoding, callback) {
  if (typeof encoding === 'function') {
    callback = encoding;
    encoding = null;
  }

  var connection = this.connection;
  if (connection &&
      connection._httpMessage === this &&
      connection.writable &&
      !connection.destroyed) {
    // There might be pending data in the this.output buffer.
    var outputLength = this.output.length;
    if (outputLength > 0) {
      this._flushOutput(connection);
    } else if (data.length === 0) {
      if (typeof callback === 'function')
        process.nextTick(callback);
      return true;
    }

    // Directly write to socket.
    return connection.write(data, encoding, callback);
  } else if (connection && connection.destroyed) {
    // The socket was destroyed.  If we're still trying to write to it,
    // then we haven't gotten the 'close' event yet.
    return false;
  } else {
    // buffer, as long as we're not destroyed.
    return this._buffer(data, encoding, callback);
  }
}


OutgoingMessage.prototype._buffer = function _buffer(data, encoding, callback) {
  this.output.push(data);
  this.outputEncodings.push(encoding);
  this.outputCallbacks.push(callback);
  this.outputSize += data.length;
  if (typeof this._onPendingData === 'function')
    this._onPendingData(data.length);
  return false;
};


OutgoingMessage.prototype._storeHeader = _storeHeader;
function _storeHeader(firstLine, headers) {
  // firstLine in the case of request is: 'GET /index.html HTTP/1.1\r\n'
  // in the case of response it is: 'HTTP/1.1 200 OK\r\n'
  var state = {
    sentConnectionHeader: false,
    sentConnectionUpgrade: false,
    sentContentLengthHeader: false,
    sentTransferEncodingHeader: false,
    sentDateHeader: false,
    sentExpect: false,
    sentTrailer: false,
    sentUpgrade: false,
    messageHeader: firstLine
  };

  if (headers) {
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

      if (Array.isArray(value)) {
        for (var j = 0; j < value.length; j++) {
          storeHeader(this, state, field, value[j]);
        }
      } else {
        storeHeader(this, state, field, value);
      }
    }
  }

  // Are we upgrading the connection?
  if (state.sentConnectionUpgrade && state.sentUpgrade)
    this.upgrading = true;

  // Date header
  if (this.sendDate && !state.sentDateHeader) {
    state.messageHeader += 'Date: ' + utcDate() + CRLF;
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
  var statusCode = this.statusCode;
  if ((statusCode === 204 || statusCode === 304) && this.chunkedEncoding) {
    debug(statusCode + ' response should not use chunked encoding,' +
          ' closing connection.');
    this.chunkedEncoding = false;
    this.shouldKeepAlive = false;
  }

  // keep-alive logic
  if (this._removedHeader.connection) {
    this._last = true;
    this.shouldKeepAlive = false;
  } else if (!state.sentConnectionHeader) {
    var shouldSendKeepAlive = this.shouldKeepAlive &&
        (state.sentContentLengthHeader ||
         this.useChunkedEncodingByDefault ||
         this.agent);
    if (shouldSendKeepAlive) {
      state.messageHeader += 'Connection: keep-alive\r\n';
    } else {
      this._last = true;
      state.messageHeader += 'Connection: close\r\n';
    }
  }

  if (!state.sentContentLengthHeader && !state.sentTransferEncodingHeader) {
    if (!this._hasBody) {
      // Make sure we don't end the 0\r\n\r\n at the end of the message.
      this.chunkedEncoding = false;
    } else if (!this.useChunkedEncodingByDefault) {
      this._last = true;
    } else {
      if (!state.sentTrailer &&
          !this._removedHeader['content-length'] &&
          typeof this._contentLength === 'number') {
        state.messageHeader += 'Content-Length: ' + this._contentLength +
                               '\r\n';
      } else if (!this._removedHeader['transfer-encoding']) {
        state.messageHeader += 'Transfer-Encoding: chunked\r\n';
        this.chunkedEncoding = true;
      } else {
        // We should only be able to get here if both Content-Length and
        // Transfer-Encoding are removed by the user.
        // See: test/parallel/test-http-remove-header-stays-removed.js
        debug('Both Content-Length and Transfer-Encoding are removed');
      }
    }
  }

  this._header = state.messageHeader + CRLF;
  this._headerSent = false;

  // wait until the first body chunk, or close(), is sent to flush,
  // UNLESS we're sending Expect: 100-continue.
  if (state.sentExpect) this._send('');
}

function storeHeader(self, state, field, value) {
  if (!checkIsHttpToken(field)) {
    throw new TypeError(
      'Header name must be a valid HTTP Token ["' + field + '"]');
  }
  if (checkInvalidHeaderChar(value)) {
    debug('Header "%s" contains invalid characters', field);
    throw new TypeError('The header content contains invalid characters');
  }
  state.messageHeader += field + ': ' + escapeHeaderValue(value) + CRLF;

  if (connectionExpression.test(field)) {
    state.sentConnectionHeader = true;
    if (connCloseExpression.test(value)) {
      self._last = true;
    } else {
      self.shouldKeepAlive = true;
    }
    if (connUpgradeExpression.test(value))
      state.sentConnectionUpgrade = true;
  } else if (transferEncodingExpression.test(field)) {
    state.sentTransferEncodingHeader = true;
    if (trfrEncChunkExpression.test(value)) self.chunkedEncoding = true;

  } else if (contentLengthExpression.test(field)) {
    state.sentContentLengthHeader = true;
  } else if (dateExpression.test(field)) {
    state.sentDateHeader = true;
  } else if (expectExpression.test(field)) {
    state.sentExpect = true;
  } else if (trailerExpression.test(field)) {
    state.sentTrailer = true;
  } else if (upgradeExpression.test(field)) {
    state.sentUpgrade = true;
  }
}


OutgoingMessage.prototype.setHeader = function setHeader(name, value) {
  if (!checkIsHttpToken(name))
    throw new TypeError(
      'Header name must be a valid HTTP Token ["' + name + '"]');
  if (value === undefined)
    throw new Error('"value" required in setHeader("' + name + '", value)');
  if (this._header)
    throw new Error('Can\'t set headers after they are sent.');
  if (checkInvalidHeaderChar(value)) {
    debug('Header "%s" contains invalid characters', name);
    throw new TypeError('The header content contains invalid characters');
  }
  if (this._headers === null)
    this._headers = {};

  var key = name.toLowerCase();
  this._headers[key] = value;
  this._headerNames[key] = name;

  if (automaticHeaders[key])
    this._removedHeader[key] = false;
};


OutgoingMessage.prototype.getHeader = function getHeader(name) {
  if (arguments.length < 1) {
    throw new Error('"name" argument is required for getHeader(name)');
  }

  if (!this._headers) return;

  return this._headers[name.toLowerCase()];
};


OutgoingMessage.prototype.removeHeader = function removeHeader(name) {
  if (arguments.length < 1) {
    throw new Error('"name" argument is required for removeHeader(name)');
  }

  if (this._header) {
    throw new Error('Can\'t remove headers after they are sent');
  }

  var key = name.toLowerCase();

  if (key === 'date')
    this.sendDate = false;
  else if (automaticHeaders[key])
    this._removedHeader[key] = true;

  if (this._headers) {
    delete this._headers[key];
    delete this._headerNames[key];
  }
};


OutgoingMessage.prototype._renderHeaders = function _renderHeaders() {
  if (this._header) {
    throw new Error('Can\'t render headers after they are sent to the client');
  }

  var headersMap = this._headers;
  if (!headersMap) return {};

  var headers = {};
  var keys = Object.keys(headersMap);
  var headerNames = this._headerNames;

  for (var i = 0, l = keys.length; i < l; i++) {
    var key = keys[i];
    headers[headerNames[key]] = headersMap[key];
  }
  return headers;
};

OutgoingMessage.prototype._implicitHeader = function _implicitHeader() {
  throw new Error('_implicitHeader() method is not implemented');
};

Object.defineProperty(OutgoingMessage.prototype, 'headersSent', {
  configurable: true,
  enumerable: true,
  get: function() { return !!this._header; }
});


OutgoingMessage.prototype.write = function write(chunk, encoding, callback) {
  if (this.finished) {
    var err = new Error('write after end');
    process.nextTick(writeAfterEndNT, this, err, callback);

    return true;
  }

  if (!this._header) {
    this._implicitHeader();
  }

  if (!this._hasBody) {
    debug('This type of response MUST NOT have a body. ' +
          'Ignoring write() calls.');
    return true;
  }

  if (typeof chunk !== 'string' && !(chunk instanceof Buffer)) {
    throw new TypeError('First argument must be a string or Buffer');
  }


  // If we get an empty string or buffer, then just do nothing, and
  // signal the user to keep writing.
  if (chunk.length === 0) return true;

  var len, ret;
  if (this.chunkedEncoding) {
    if (typeof chunk === 'string' &&
        encoding !== 'hex' &&
        encoding !== 'base64' &&
        encoding !== 'latin1') {
      len = Buffer.byteLength(chunk, encoding);
      chunk = len.toString(16) + CRLF + chunk + CRLF;
      ret = this._send(chunk, encoding, callback);
    } else {
      // buffer, or a non-toString-friendly encoding
      if (typeof chunk === 'string')
        len = Buffer.byteLength(chunk, encoding);
      else
        len = chunk.length;

      if (this.connection && !this.connection.corked) {
        this.connection.cork();
        process.nextTick(connectionCorkNT, this.connection);
      }
      this._send(len.toString(16), 'latin1', null);
      this._send(crlf_buf, null, null);
      this._send(chunk, encoding, null);
      ret = this._send(crlf_buf, null, callback);
    }
  } else {
    ret = this._send(chunk, encoding, callback);
  }

  debug('write ret = ' + ret);
  return ret;
};


function writeAfterEndNT(self, err, callback) {
  self.emit('error', err);
  if (callback) callback(err);
}


function connectionCorkNT(conn) {
  conn.uncork();
}


function escapeHeaderValue(value) {
  // Protect against response splitting. The regex test is there to
  // minimize the performance impact in the common case.
  return /[\r\n]/.test(value) ? value.replace(/[\r\n]+[ \t]*/g, '') : value;
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
    if (!checkIsHttpToken(field)) {
      throw new TypeError(
        'Trailer name must be a valid HTTP Token ["' + field + '"]');
    }
    if (checkInvalidHeaderChar(value)) {
      debug('Trailer "%s" contains invalid characters', field);
      throw new TypeError('The trailer content contains invalid characters');
    }
    this._trailer += field + ': ' + escapeHeaderValue(value) + CRLF;
  }
};


const crlf_buf = Buffer.from('\r\n');

function onFinish(outmsg) {
  outmsg.emit('finish');
}

OutgoingMessage.prototype.end = function end(data, encoding, callback) {
  if (typeof data === 'function') {
    callback = data;
    data = null;
  } else if (typeof encoding === 'function') {
    callback = encoding;
    encoding = null;
  }

  if (data && typeof data !== 'string' && !(data instanceof Buffer)) {
    throw new TypeError('First argument must be a string or Buffer');
  }

  if (this.finished) {
    return false;
  }

  if (!this._header) {
    if (data) {
      if (typeof data === 'string')
        this._contentLength = Buffer.byteLength(data, encoding);
      else
        this._contentLength = data.length;
    } else {
      this._contentLength = 0;
    }
    this._implicitHeader();
  }

  if (data && !this._hasBody) {
    debug('This type of response MUST NOT have a body. ' +
          'Ignoring data passed to end().');
    data = null;
  }

  if (this.connection && data)
    this.connection.cork();

  if (data) {
    // Normal body write.
    this.write(data, encoding);
  }

  if (typeof callback === 'function')
    this.once('finish', callback);

  var finish = onFinish.bind(undefined, this);

  var ret;
  if (this._hasBody && this.chunkedEncoding) {
    ret = this._send('0\r\n' + this._trailer + '\r\n', 'latin1', finish);
  } else {
    // Force a flush, HACK.
    ret = this._send('', 'latin1', finish);
  }

  if (this.connection && data)
    this.connection.uncork();

  this.finished = true;

  // There is the first message on the outgoing queue, and we've sent
  // everything to the socket.
  debug('outgoing message end.');
  if (this.output.length === 0 &&
      this.connection &&
      this.connection._httpMessage === this) {
    this._finish();
  }

  return ret;
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
  var outputLength = this.output.length;
  if (outputLength <= 0)
    return ret;

  var output = this.output;
  var outputEncodings = this.outputEncodings;
  var outputCallbacks = this.outputCallbacks;
  socket.cork();
  for (var i = 0; i < outputLength; i++) {
    ret = socket.write(output[i], outputEncodings[i], outputCallbacks[i]);
  }
  socket.uncork();

  this.output = [];
  this.outputEncodings = [];
  this.outputCallbacks = [];
  if (typeof this._onPendingData === 'function')
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
}, 'OutgoingMessage.flush is deprecated. Use flushHeaders instead.');

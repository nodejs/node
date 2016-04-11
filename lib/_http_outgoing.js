'use strict';

const assert = require('assert').ok;
const Writable = require('stream').Writable;
const timers = require('timers');
const util = require('util');
const internalUtil = require('internal/util');
const Buffer = require('buffer').Buffer;
const common = require('_http_common');

const CRLF = common.CRLF;
const chunkExpression = common.chunkExpression;
const debug = common.debug;

const connectionExpression = /^Connection$/i;
const transferEncodingExpression = /^Transfer-Encoding$/i;
const closeExpression = /close/i;
const contentLengthExpression = /^Content-Length$/i;
const dateExpression = /^Date$/i;
const expectExpression = /^Expect$/i;
const trailerExpression = /^Trailer$/i;

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
utcDate._onTimeout = function() {
  dateCache = undefined;
};


function OutgoingMessage() {
  Writable.call(this, {
    decodeStrings: false
  });

  // Start corked, will be uncorked on `_assignSocket`
  this.cork();

  this._last = false;
  this.chunkedEncoding = false;
  this.shouldKeepAlive = true;
  this.useChunkedEncodingByDefault = true;
  this.sendDate = false;
  this._removedHeader = {};

  this._contentLength = null;
  this._hasBody = true;
  this._trailer = '';

  this._headerSent = false;

  this.socket = null;
  this.connection = null;
  this._header = null;
  this._headers = null;
  this._headerNames = {};

  this._onPendingData = null;
  this._onPendingLast = 0;

  this.on('prefinish', this._onPrefinish);
}
util.inherits(OutgoingMessage, Writable);


exports.OutgoingMessage = OutgoingMessage;


OutgoingMessage.prototype._assignSocket = function(socket) {
  this.socket = socket;
  this.connection = socket;
  this.uncork();

  this.emit('socket', socket);
};


OutgoingMessage.prototype.setTimeout = function(msecs, callback) {

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
OutgoingMessage.prototype.destroy = function(error) {
  const socket = this.socket;

  if (socket) {
    // NOTE: we may have some nextTick-corked data pending to write. There is
    // no point in nextTick optimization here anymore, so just write everything
    // to the socket
    socket.uncork();
    socket.destroy(error);
  } else {
    this.once('socket', (socket) => {
      socket.uncork();
      socket.destroy(error);
    });
  }
};


OutgoingMessage.prototype._sendHeaders = function(data) {
  if (this._headerSent)
    return;

  if (!this.socket) {
    this.once('socket', () => {
      this._sendHeaders();
    });
    return;
  }

  if (!this._header)
    this._implicitHeader();

  this._headerSent = true;
  if (this.socket.writable && !this.socket.destroyed)
    this.socket.write(this._header);
};


OutgoingMessage.prototype._countPendingData = function _countPendingData() {
  if (!this._onPendingData)
    return;

  const before = this._onPendingLast;
  const now = this._writableState.length;
  this._onPendingLast = now;

  this._onPendingData(now - before);
};


OutgoingMessage.prototype.write = function write(data, encoding, callback) {
  const res = Writable.prototype.write.call(this, data, encoding, callback);

  this._countPendingData();
  debug('write ret = %j', res);

  return res;
};


OutgoingMessage.prototype._corkUncork = function _corkUncork(socket) {
  if (!socket.corked) {
    socket.cork();
    process.nextTick(socketUncorkNT, socket);
  }
};


OutgoingMessage.prototype._write = function _write(data, encoding, callback) {
  const socket = this.socket;

  assert(socket);

  this._countPendingData();

  // Ignore writes after end
  if (!socket.writable || socket.destroyed)
    return;

  // NOTE: Logic is duplicated to concat with `data`
  // (Fast case)
  if (!this._headerSent) {
    if (!this._header)
      this._implicitHeader();

    this._headerSent = true;
    this._corkUncork(socket);
    socket.write(this._header);
  }

  if (!this._hasBody) {
    debug('This type of response MUST NOT have a body. ' +
          'Ignoring write() calls.');
    return callback(null);
  }

  // Empty writes don't play well with chunked encoding
  if (data.length === 0)
    return process.nextTick(callback);

  if (this.chunkedEncoding) {
    if (typeof data === 'string' &&
        encoding !== 'hex' &&
        encoding !== 'base64' &&
        encoding !== 'binary') {
      const len = Buffer.byteLength(data, encoding);
      const chunk = len.toString(16) + CRLF + data + CRLF;
      socket.write(chunk, encoding, callback);
    } else {
      let len;

      // buffer, or a non-toString-friendly encoding
      if (typeof data === 'string')
        len = Buffer.byteLength(data, encoding);
      else
        len = data.length;

      this._corkUncork(socket);
      socket.write(len.toString(16), 'binary', null);
      socket.write(crlf_buf, null, null);
      socket.write(data, encoding, null);
      socket.write(crlf_buf, null, callback);
    }
  } else {
    socket.write(data, encoding, callback);
  }
};


//
// XXX(streams): I wish there was a way to override some of this behavior in
// Writable
//
OutgoingMessage.prototype.end = function end(chunk, encoding, cb) {
  // This is odd legacy behavior, required for compatibility
  // TODO(indutny): perhaps remove it at some point?
  if (this._writableState.ended)
    return false;

  // Fast case `Content-Length`, immediate `res.end(...)`
  if (!this._header && this._writableState.length === 0) {
    if (chunk) {
      if (typeof chunk === 'string')
        this._contentLength = Buffer.byteLength(chunk, encoding);
      else
        this._contentLength = chunk.length;
    } else {
      this._contentLength = 0;
    }
  }

  if (!this._header)
    this._implicitHeader();

  if (this.socket) {
    const ret = Writable.prototype.end.call(this, chunk, encoding, cb);
    this._countPendingData();
    debug('outgoing message end.');
    return ret;
  }

  if (typeof chunk === 'function') {
    cb = chunk;
    chunk = null;
    encoding = null;
  } else if (typeof encoding === 'function') {
    cb = encoding;
    encoding = null;
  }

  if (chunk !== null && chunk !== undefined)
    this.write(chunk, encoding);

  // No more `.write()` calls
  // NOTE: .ending is intentionally `false`
  this._writableState.ended = true;

  this.once('socket', () => {
    debug('outgoing message end.');
    Writable.prototype.end.call(this, cb);
    this._countPendingData();
  });
};


OutgoingMessage.prototype._storeHeader = function(firstLine, headers) {
  // firstLine in the case of request is: 'GET /index.html HTTP/1.1\r\n'
  // in the case of response it is: 'HTTP/1.1 200 OK\r\n'
  var state = {
    sentConnectionHeader: false,
    sentContentLengthHeader: false,
    sentTransferEncodingHeader: false,
    sentDateHeader: false,
    sentExpect: false,
    sentTrailer: false,
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

  // Date header
  if (this.sendDate === true && state.sentDateHeader === false) {
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
  if ((statusCode === 204 || statusCode === 304) &&
      this.chunkedEncoding === true) {
    debug(statusCode + ' response should not use chunked encoding,' +
          ' closing connection.');
    this.chunkedEncoding = false;
    this.shouldKeepAlive = false;
  }

  // keep-alive logic
  if (this._removedHeader.connection) {
    this._last = true;
    this.shouldKeepAlive = false;
  } else if (state.sentConnectionHeader === false) {
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

  if (state.sentContentLengthHeader === false &&
      state.sentTransferEncodingHeader === false) {
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
  if (state.sentExpect)
    this._sendHeaders();
};

function storeHeader(self, state, field, value) {
  if (!common._checkIsHttpToken(field)) {
    throw new TypeError(
      'Header name must be a valid HTTP Token ["' + field + '"]');
  }
  if (common._checkInvalidHeaderChar(value) === true) {
    throw new TypeError('The header content contains invalid characters');
  }
  state.messageHeader += field + ': ' + escapeHeaderValue(value) + CRLF;

  if (connectionExpression.test(field)) {
    state.sentConnectionHeader = true;
    if (closeExpression.test(value)) {
      self._last = true;
    } else {
      self.shouldKeepAlive = true;
    }

  } else if (transferEncodingExpression.test(field)) {
    state.sentTransferEncodingHeader = true;
    if (chunkExpression.test(value)) self.chunkedEncoding = true;

  } else if (contentLengthExpression.test(field)) {
    state.sentContentLengthHeader = true;
  } else if (dateExpression.test(field)) {
    state.sentDateHeader = true;
  } else if (expectExpression.test(field)) {
    state.sentExpect = true;
  } else if (trailerExpression.test(field)) {
    state.sentTrailer = true;
  }
}


OutgoingMessage.prototype.setHeader = function(name, value) {
  if (!common._checkIsHttpToken(name))
    throw new TypeError(
      'Header name must be a valid HTTP Token ["' + name + '"]');
  if (typeof name !== 'string')
    throw new TypeError('"name" should be a string in setHeader(name, value)');
  if (value === undefined)
    throw new Error('"value" required in setHeader("' + name + '", value)');
  if (this._header)
    throw new Error('Can\'t set headers after they are sent.');
  if (common._checkInvalidHeaderChar(value) === true) {
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


OutgoingMessage.prototype.getHeader = function(name) {
  if (arguments.length < 1) {
    throw new Error('"name" argument is required for getHeader(name)');
  }

  if (!this._headers) return;

  var key = name.toLowerCase();
  return this._headers[key];
};


OutgoingMessage.prototype.removeHeader = function(name) {
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


OutgoingMessage.prototype._renderHeaders = function() {
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


Object.defineProperty(OutgoingMessage.prototype, 'headersSent', {
  configurable: true,
  enumerable: true,
  get: function() { return !!this._header; }
});


function socketUncorkNT(conn) {
  if (conn.writable && !conn.destroyed)
    conn.uncork();
}


function escapeHeaderValue(value) {
  // Protect against response splitting. The regex test is there to
  // minimize the performance impact in the common case.
  return /[\r\n]/.test(value) ? value.replace(/[\r\n]+[ \t]*/g, '') : value;
}


OutgoingMessage.prototype.addTrailers = function(headers) {
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
    if (!common._checkIsHttpToken(field)) {
      throw new TypeError(
        'Trailer name must be a valid HTTP Token ["' + field + '"]');
    }
    if (common._checkInvalidHeaderChar(value) === true) {
      throw new TypeError('The header content contains invalid characters');
    }
    this._trailer += field + ': ' + escapeHeaderValue(value) + CRLF;
  }
};


const crlf_buf = Buffer.from('\r\n');


OutgoingMessage.prototype._onPrefinish = function() {
  const socket = this.socket;

  this._countPendingData();
  if (!socket || !socket.writable || socket.destroyed)
    return;

  socket.cork();
  this._sendHeaders();

  if (this._hasBody && this.chunkedEncoding)
    socket.write('0\r\n' + this._trailer + '\r\n', 'binary');
  socket.uncork();
};


OutgoingMessage.prototype.flushHeaders = OutgoingMessage.prototype._sendHeaders;


OutgoingMessage.prototype.flush = internalUtil.deprecate(function() {
  this.flushHeaders();
}, 'OutgoingMessage.flush is deprecated. Use flushHeaders instead.');

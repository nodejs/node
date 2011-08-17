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

var util = require('util');
var net = require('net');
var stream = require('stream');
var EventEmitter = require('events').EventEmitter;
var FreeList = require('freelist').FreeList;
var HTTPParser = process.binding('http_parser').HTTPParser;
var assert = require('assert').ok;


var debug;
if (process.env.NODE_DEBUG && /http/.test(process.env.NODE_DEBUG)) {
  debug = function(x) { console.error('HTTP: %s', x); };
} else {
  debug = function() { };
}


var parsers = new FreeList('parsers', 1000, function() {
  var parser = new HTTPParser('request');

  parser.onMessageBegin = function() {
    parser.incoming = new IncomingMessage(parser.socket);
    parser.field = null;
    parser.value = null;
  };

  // Only servers will get URL events.
  parser.onURL = function(b, start, len) {
    var slice = b.toString('ascii', start, start + len);
    if (parser.incoming.url) {
      parser.incoming.url += slice;
    } else {
      // Almost always will branch here.
      parser.incoming.url = slice;
    }
  };

  parser.onHeaderField = function(b, start, len) {
    var slice = b.toString('ascii', start, start + len).toLowerCase();
    if (parser.value != undefined) {
      parser.incoming._addHeaderLine(parser.field, parser.value);
      parser.field = null;
      parser.value = null;
    }
    if (parser.field) {
      parser.field += slice;
    } else {
      parser.field = slice;
    }
  };

  parser.onHeaderValue = function(b, start, len) {
    var slice = b.toString('ascii', start, start + len);
    if (parser.value) {
      parser.value += slice;
    } else {
      parser.value = slice;
    }
  };

  parser.onHeadersComplete = function(info) {
    if (parser.field && (parser.value != undefined)) {
      parser.incoming._addHeaderLine(parser.field, parser.value);
      parser.field = null;
      parser.value = null;
    }

    parser.incoming.httpVersionMajor = info.versionMajor;
    parser.incoming.httpVersionMinor = info.versionMinor;
    parser.incoming.httpVersion = info.versionMajor + '.' + info.versionMinor;

    if (info.method) {
      // server only
      parser.incoming.method = info.method;
    } else {
      // client only
      parser.incoming.statusCode = info.statusCode;
    }

    parser.incoming.upgrade = info.upgrade;

    var isHeadResponse = false;

    if (!info.upgrade) {
      // For upgraded connections, we'll emit this after parser.execute
      // so that we can capture the first part of the new protocol
      isHeadResponse = parser.onIncoming(parser.incoming, info.shouldKeepAlive);
    }

    return isHeadResponse;
  };

  parser.onBody = function(b, start, len) {
    // TODO body encoding?
    var slice = b.slice(start, start + len);
    if (parser.incoming._decoder) {
      var string = parser.incoming._decoder.write(slice);
      if (string.length) parser.incoming.emit('data', string);
    } else {
      parser.incoming.emit('data', slice);
    }
  };

  parser.onMessageComplete = function() {
    this.incoming.complete = true;
    if (parser.field && (parser.value != undefined)) {
      parser.incoming._addHeaderLine(parser.field, parser.value);
    }
    if (!parser.incoming.upgrade) {
      // For upgraded connections, also emit this after parser.execute
      parser.incoming.readable = false;
      parser.incoming.emit('end');
    }
  };

  return parser;
});
exports.parsers = parsers;


var CRLF = '\r\n';
var STATUS_CODES = exports.STATUS_CODES = {
  100 : 'Continue',
  101 : 'Switching Protocols',
  102 : 'Processing',                 // RFC 2518, obsoleted by RFC 4918
  200 : 'OK',
  201 : 'Created',
  202 : 'Accepted',
  203 : 'Non-Authoritative Information',
  204 : 'No Content',
  205 : 'Reset Content',
  206 : 'Partial Content',
  207 : 'Multi-Status',               // RFC 4918
  300 : 'Multiple Choices',
  301 : 'Moved Permanently',
  302 : 'Moved Temporarily',
  303 : 'See Other',
  304 : 'Not Modified',
  305 : 'Use Proxy',
  307 : 'Temporary Redirect',
  400 : 'Bad Request',
  401 : 'Unauthorized',
  402 : 'Payment Required',
  403 : 'Forbidden',
  404 : 'Not Found',
  405 : 'Method Not Allowed',
  406 : 'Not Acceptable',
  407 : 'Proxy Authentication Required',
  408 : 'Request Time-out',
  409 : 'Conflict',
  410 : 'Gone',
  411 : 'Length Required',
  412 : 'Precondition Failed',
  413 : 'Request Entity Too Large',
  414 : 'Request-URI Too Large',
  415 : 'Unsupported Media Type',
  416 : 'Requested Range Not Satisfiable',
  417 : 'Expectation Failed',
  418 : 'I\'m a teapot',              // RFC 2324
  422 : 'Unprocessable Entity',       // RFC 4918
  423 : 'Locked',                     // RFC 4918
  424 : 'Failed Dependency',          // RFC 4918
  425 : 'Unordered Collection',       // RFC 4918
  426 : 'Upgrade Required',           // RFC 2817
  500 : 'Internal Server Error',
  501 : 'Not Implemented',
  502 : 'Bad Gateway',
  503 : 'Service Unavailable',
  504 : 'Gateway Time-out',
  505 : 'HTTP Version not supported',
  506 : 'Variant Also Negotiates',    // RFC 2295
  507 : 'Insufficient Storage',       // RFC 4918
  509 : 'Bandwidth Limit Exceeded',
  510 : 'Not Extended'                // RFC 2774
};


var connectionExpression = /Connection/i;
var transferEncodingExpression = /Transfer-Encoding/i;
var closeExpression = /close/i;
var chunkExpression = /chunk/i;
var contentLengthExpression = /Content-Length/i;
var expectExpression = /Expect/i;
var continueExpression = /100-continue/i;


/* Abstract base class for ServerRequest and ClientResponse. */
function IncomingMessage(socket) {
  stream.Stream.call(this);

  // TODO Remove one of these eventually.
  this.socket = socket;
  this.connection = socket;

  this.httpVersion = null;
  this.complete = false;
  this.headers = {};
  this.trailers = {};

  this.readable = true;

  // request (server) only
  this.url = '';

  this.method = null;

  // response (client) only
  this.statusCode = null;
  this.client = this.socket;
}
util.inherits(IncomingMessage, stream.Stream);


exports.IncomingMessage = IncomingMessage;


IncomingMessage.prototype.destroy = function(error) {
  this.socket.destroy(error);
};


IncomingMessage.prototype.setEncoding = function(encoding) {
  var StringDecoder = require('string_decoder').StringDecoder; // lazy load
  this._decoder = new StringDecoder(encoding);
};


IncomingMessage.prototype.pause = function() {
  this.socket.pause();
};


IncomingMessage.prototype.resume = function() {
  this.socket.resume();
};


// Add the given (field, value) pair to the message
//
// Per RFC2616, section 4.2 it is acceptable to join multiple instances of the
// same header with a ', ' if the header in question supports specification of
// multiple values this way. If not, we declare the first instance the winner
// and drop the second. Extended header fields (those beginning with 'x-') are
// always joined.
IncomingMessage.prototype._addHeaderLine = function(field, value) {
  var dest = this.complete ? this.trailers : this.headers;

  switch (field) {
    // Array headers:
    case 'set-cookie':
      if (field in dest) {
        dest[field].push(value);
      } else {
        dest[field] = [value];
      }
      break;

    // Comma separate. Maybe make these arrays?
    case 'accept':
    case 'accept-charset':
    case 'accept-encoding':
    case 'accept-language':
    case 'connection':
    case 'cookie':
    case 'pragma':
    case 'link':
      if (field in dest) {
        dest[field] += ', ' + value;
      } else {
        dest[field] = value;
      }
      break;


    default:
      if (field.slice(0, 2) == 'x-') {
        // except for x-
        if (field in dest) {
          dest[field] += ', ' + value;
        } else {
          dest[field] = value;
        }
      } else {
        // drop duplicates
        if (!(field in dest)) dest[field] = value;
      }
      break;
  }
};


function OutgoingMessage() {
  stream.Stream.call(this);

  this.output = [];
  this.outputEncodings = [];

  this.writable = true;

  this._last = false;
  this.chunkedEncoding = false;
  this.shouldKeepAlive = true;
  this.useChunkedEncodingByDefault = true;

  this._hasBody = true;
  this._trailer = '';

  this.finished = false;
}
util.inherits(OutgoingMessage, stream.Stream);


exports.OutgoingMessage = OutgoingMessage;


OutgoingMessage.prototype.assignSocket = function(socket) {
  assert(!socket._httpMessage);
  socket._httpMessage = this;
  this.socket = socket;
  this.connection = socket;
  this._flush();
};


OutgoingMessage.prototype.detachSocket = function(socket) {
  assert(socket._httpMessage == this);
  socket._httpMessage = null;
  this.socket = this.connection = null;
};


OutgoingMessage.prototype.destroy = function(error) {
  this.socket.destroy(error);
};


// This abstract either writing directly to the socket or buffering it.
OutgoingMessage.prototype._send = function(data, encoding) {
  // This is a shameful hack to get the headers and first body chunk onto
  // the same packet. Future versions of Node are going to take care of
  // this at a lower level and in a more general way.
  if (!this._headerSent) {
    if (typeof data === 'string') {
      data = this._header + data;
    } else {
      this.output.unshift(this._header);
      this.outputEncodings.unshift('ascii');
    }
    this._headerSent = true;
  }
  return this._writeRaw(data, encoding);
};


OutgoingMessage.prototype._writeRaw = function(data, encoding) {
  if (this.connection &&
      this.connection._httpMessage === this &&
      this.connection.writable) {
    // There might be pending data in the this.output buffer.
    while (this.output.length) {
      if (!this.connection.writable) {
        this._buffer(data, encoding);
        return false;
      }
      var c = this.output.shift();
      var e = this.outputEncodings.shift();
      this.connection.write(c, e);
    }

    // Directly write to socket.
    return this.connection.write(data, encoding);
  } else {
    this._buffer(data, encoding);
    return false;
  }
};


OutgoingMessage.prototype._buffer = function(data, encoding) {
  if (data.length === 0) return;

  var length = this.output.length;

  if (length === 0 || typeof data != 'string') {
    this.output.push(data);
    this.outputEncodings.push(encoding);
    return false;
  }

  var lastEncoding = this.outputEncodings[length - 1];
  var lastData = this.output[length - 1];

  if ((encoding && lastEncoding === encoding) ||
      (!encoding && data.constructor === lastData.constructor)) {
    this.output[length - 1] = lastData + data;
    return false;
  }

  this.output.push(data);
  this.outputEncodings.push(encoding);

  return false;
};


OutgoingMessage.prototype._storeHeader = function(firstLine, headers) {
  var sentConnectionHeader = false;
  var sentContentLengthHeader = false;
  var sentTransferEncodingHeader = false;
  var sentExpect = false;

  // firstLine in the case of request is: 'GET /index.html HTTP/1.1\r\n'
  // in the case of response it is: 'HTTP/1.1 200 OK\r\n'
  var messageHeader = firstLine;
  var field, value;
  var self = this;

  function store(field, value) {
    messageHeader += field + ': ' + value + CRLF;

    if (connectionExpression.test(field)) {
      sentConnectionHeader = true;
      if (closeExpression.test(value)) {
        self._last = true;
      } else {
        self.shouldKeepAlive = true;
      }

    } else if (transferEncodingExpression.test(field)) {
      sentTransferEncodingHeader = true;
      if (chunkExpression.test(value)) self.chunkedEncoding = true;

    } else if (contentLengthExpression.test(field)) {
      sentContentLengthHeader = true;

    } else if (expectExpression.test(field)) {
      sentExpect = true;
    }
  }

  if (headers) {
    var keys = Object.keys(headers);
    var isArray = (Array.isArray(headers));
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
          store(field, value[j]);
        }
      } else {
        store(field, value);
      }
    }
  }

  // keep-alive logic
  if (sentConnectionHeader == false) {
    if (this.shouldKeepAlive &&
        (sentContentLengthHeader || this.useChunkedEncodingByDefault)) {
      messageHeader += 'Connection: keep-alive\r\n';
    } else {
      this._last = true;
      messageHeader += 'Connection: close\r\n';
    }
  }

  if (sentContentLengthHeader == false && sentTransferEncodingHeader == false) {
    if (this._hasBody) {
      if (this.useChunkedEncodingByDefault) {
        messageHeader += 'Transfer-Encoding: chunked\r\n';
        this.chunkedEncoding = true;
      } else {
        this._last = true;
      }
    } else {
      // Make sure we don't end the 0\r\n\r\n at the end of the message.
      this.chunkedEncoding = false;
    }
  }

  this._header = messageHeader + CRLF;
  this._headerSent = false;

  // wait until the first body chunk, or close(), is sent to flush,
  // UNLESS we're sending Expect: 100-continue.
  if (sentExpect) this._send('');
};


OutgoingMessage.prototype.setHeader = function(name, value) {
  if (arguments.length < 2) {
    throw new Error("`name` and `value` are required for setHeader().");
  }

  if (this._header) {
    throw new Error("Can't set headers after they are sent.");
  }

  var key = name.toLowerCase();
  this._headers = this._headers || {};
  this._headerNames = this._headerNames || {};
  this._headers[key] = value;
  this._headerNames[key] = name;
};


OutgoingMessage.prototype.getHeader = function(name) {
  if (arguments.length < 1) {
    throw new Error("`name` is required for getHeader().");
  }

  if (this._header) {
    throw new Error("Can't use mutable header APIs after sent.");
  }

  if (!this._headers) return;

  var key = name.toLowerCase();
  return this._headers[key];
};


OutgoingMessage.prototype.removeHeader = function(name) {
  if (arguments.length < 1) {
    throw new Error("`name` is required for removeHeader().");
  }

  if (this._header) {
    throw new Error("Can't remove headers after they are sent.");
  }

  if (!this._headers) return;

  var key = name.toLowerCase();
  delete this._headers[key];
  delete this._headerNames[key];
};


OutgoingMessage.prototype._renderHeaders = function() {
  if (this._header) {
    throw new Error("Can't render headers after they are sent to the client.");
  }

  if (!this._headers) return {};

  var headers = {};
  var keys = Object.keys(this._headers);
  for (var i = 0, l = keys.length; i < l; i++) {
    var key = keys[i];
    headers[this._headerNames[key]] = this._headers[key];
  }
  return headers;
};



OutgoingMessage.prototype.write = function(chunk, encoding) {
  if (!this._header) {
    this._implicitHeader();
  }

  if (!this._hasBody) {
    console.error('This type of response MUST NOT have a body. ' +
                  'Ignoring write() calls.');
    return true;
  }

  if (typeof chunk !== 'string' && !Buffer.isBuffer(chunk) &&
      !Array.isArray(chunk)) {
    throw new TypeError('first argument must be a string, Array, or Buffer');
  }

  if (chunk.length === 0) return false;

  var len, ret;
  if (this.chunkedEncoding) {
    if (typeof(chunk) === 'string') {
      len = Buffer.byteLength(chunk, encoding);
      var chunk = len.toString(16) + CRLF + chunk + CRLF;
      ret = this._send(chunk, encoding);
    } else {
      // buffer
      len = chunk.length;
      this._send(len.toString(16) + CRLF);
      this._send(chunk);
      ret = this._send(CRLF);
    }
  } else {
    ret = this._send(chunk, encoding);
  }

  debug('write ret = ' + ret);
  return ret;
};


OutgoingMessage.prototype.addTrailers = function(headers) {
  this._trailer = '';
  var keys = Object.keys(headers);
  var isArray = (Array.isArray(headers));
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

    this._trailer += field + ': ' + value + CRLF;
  }
};


OutgoingMessage.prototype.end = function(data, encoding) {
  if (this.finished) {
    return false;
  }
  if (!this._header) {
    this._implicitHeader();
  }

  if (data && !this._hasBody) {
    console.error('This type of response MUST NOT have a body. ' +
                  'Ignoring data passed to end().');
    data = false;
  }

  var ret;

  var hot = this._headerSent === false &&
            typeof(data) === 'string' &&
            data.length > 0 &&
            this.output.length === 0 &&
            this.connection &&
            this.connection.writable &&
            this.connection._httpMessage === this;

  if (hot) {
    // Hot path. They're doing
    //   res.writeHead();
    //   res.end(blah);
    // HACKY.

    if (this.chunkedEncoding) {
      var l = Buffer.byteLength(data, encoding).toString(16);
      ret = this.connection.write(this._header + l + CRLF +
                                  data + '\r\n0\r\n' +
                                  this._trailer + '\r\n', encoding);
    } else {
      ret = this.connection.write(this._header + data, encoding);
    }
    this._headerSent = true;

  } else if (data) {
    // Normal body write.
    ret = this.write(data, encoding);
  }

  if (!hot) {
    if (this.chunkedEncoding) {
      ret = this._send('0\r\n' + this._trailer + '\r\n'); // Last chunk.
    } else {
      // Force a flush, HACK.
      ret = this._send('');
    }
  }

  this.finished = true;

  // There is the first message on the outgoing queue, and we've sent
  // everything to the socket.
  if (this.output.length === 0 && this.connection._httpMessage === this) {
    debug('outgoing message end.');
    this._finish();
  }

  return ret;
};


OutgoingMessage.prototype._finish = function() {
  assert(this.connection);
  if (this instanceof ServerResponse) {
    DTRACE_HTTP_SERVER_RESPONSE(this.connection);
  } else {
    assert(this instanceof ClientRequest);
    DTRACE_HTTP_CLIENT_REQUEST(this, this.connection);
  }
  this.emit('finish');
};


OutgoingMessage.prototype._flush = function() {
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

  if (!this.socket) return;

  var ret;

  while (this.output.length) {
    if (!this.socket.writable) return; // XXX Necessary?

    var data = this.output.shift();
    var encoding = this.outputEncodings.shift();

    ret = this.socket.write(data, encoding);
  }

  if (this.finished) {
    // This is a queue to the server or client to bring in the next this.
    this._finish();
  } else if (ret) {
    // This is necessary to prevent https from breaking
    this.emit('drain');
  }
};




function ServerResponse(req) {
  OutgoingMessage.call(this);

  if (req.method === 'HEAD') this._hasBody = false;

  if (req.httpVersionMajor < 1 || req.httpVersionMinor < 1) {
    this.useChunkedEncodingByDefault = false;
    this.shouldKeepAlive = false;
  }
}
util.inherits(ServerResponse, OutgoingMessage);


exports.ServerResponse = ServerResponse;

ServerResponse.prototype.statusCode = 200;

ServerResponse.prototype.writeContinue = function() {
  this._writeRaw('HTTP/1.1 100 Continue' + CRLF + CRLF, 'ascii');
  this._sent100 = true;
};

ServerResponse.prototype._implicitHeader = function() {
  this.writeHead(this.statusCode);
};

ServerResponse.prototype.writeHead = function(statusCode) {
  var reasonPhrase, headers, headerIndex;

  if (typeof arguments[1] == 'string') {
    reasonPhrase = arguments[1];
    headerIndex = 2;
  } else {
    reasonPhrase = STATUS_CODES[statusCode] || 'unknown';
    headerIndex = 1;
  }
  this.statusCode = statusCode;

  var obj = arguments[headerIndex];

  if (obj && this._headers) {
    // Slow-case: when progressive API and header fields are passed.
    headers = this._renderHeaders();

    if (Array.isArray(obj)) {
      // handle array case
      // TODO: remove when array is no longer accepted
      var field;
      for (var i = 0, len = obj.length; i < len; ++i) {
        field = obj[i][0];
        if (field in headers) {
          obj.push([field, headers[field]]);
        }
      }
      headers = obj;

    } else {
      // handle object case
      for (var k in obj) {
        if (k) headers[k] = obj[k];
      }
    }
  } else if (this._headers) {
    // only progressive api is used
    headers = this._renderHeaders();
  } else {
    // only writeHead() called
    headers = obj;
  }

  var statusLine = 'HTTP/1.1 ' + statusCode.toString() + ' ' +
                   reasonPhrase + CRLF;

  if (statusCode === 204 || statusCode === 304 ||
      (100 <= statusCode && statusCode <= 199)) {
    // RFC 2616, 10.2.5:
    // The 204 response MUST NOT include a message-body, and thus is always
    // terminated by the first empty line after the header fields.
    // RFC 2616, 10.3.5:
    // The 304 response MUST NOT contain a message-body, and thus is always
    // terminated by the first empty line after the header fields.
    // RFC 2616, 10.1 Informational 1xx:
    // This class of status code indicates a provisional response,
    // consisting only of the Status-Line and optional headers, and is
    // terminated by an empty line.
    this._hasBody = false;
  }

  // don't keep alive connections where the client expects 100 Continue
  // but we sent a final status; they may put extra bytes on the wire.
  if (this._expect_continue && ! this._sent100) {
    this.shouldKeepAlive = false;
  }

  this._storeHeader(statusLine, headers);
};


ServerResponse.prototype.writeHeader = function() {
  this.writeHead.apply(this, arguments);
};


function ClientRequest(options, defaultPort) {
  OutgoingMessage.call(this);

  if (!defaultPort) defaultPort = 80;

  var method = this.method = (options.method || 'GET').toUpperCase();
  this.path = options.path || '/';

  if (!Array.isArray(headers)) {
    if (options.headers) {
      var headers = options.headers;
      var keys = Object.keys(headers);
      for (var i = 0, l = keys.length; i < l; i++) {
        var key = keys[i];
        this.setHeader(key, headers[key]);
      }
    }
    // Host header set by default.
    if (options.host && !this.getHeader('host')) {
      var hostHeader = options.host;
      if (options.port && +options.port !== defaultPort) {
        hostHeader += ':' + options.port;
      }
      this.setHeader("Host", hostHeader);
    }
  }

  this.shouldKeepAlive = false;
  if (method === 'GET' || method === 'HEAD') {
    this.useChunkedEncodingByDefault = false;
  } else {
    this.useChunkedEncodingByDefault = true;
  }

  // By default keep-alive is off. This is the last message unless otherwise
  // specified.
  this._last = true;

  if (Array.isArray(headers)) {
    this._storeHeader(this.method + ' ' + this.path + ' HTTP/1.1\r\n', headers);
  } else if (this.getHeader('expect')) {
    this._storeHeader(this.method + ' ' + this.path + ' HTTP/1.1\r\n', this._renderHeaders());
  }

}
util.inherits(ClientRequest, OutgoingMessage);


exports.ClientRequest = ClientRequest;

ClientRequest.prototype._implicitHeader = function() {
  this._storeHeader(this.method + ' ' + this.path + ' HTTP/1.1\r\n', this._renderHeaders());
}

ClientRequest.prototype.abort = function() {
  if (this._queue) {
    // queued for dispatch
    assert(!this.connection);
    var i = this._queue.indexOf(this);
    this._queue.splice(i, 1);

  } else if (this.connection) {
    // in-progress
    var c = this.connection;
    this.detachSocket(c);
    c.destroy();

  } else {
    // already complete.
  }
};


function httpSocketSetup(socket) {
  // NOTE: be sure not to use ondrain elsewhere in this file!
  socket.ondrain = function() {
    if (socket._httpMessage) {
      socket._httpMessage.emit('drain');
    }
  };
}


function Server(requestListener) {
  if (!(this instanceof Server)) return new Server(requestListener);
  net.Server.call(this, { allowHalfOpen: true });

  if (requestListener) {
    this.addListener('request', requestListener);
  }

  // Similar option to this. Too lazy to write my own docs.
  // http://www.squid-cache.org/Doc/config/half_closed_clients/
  // http://wiki.squid-cache.org/SquidFaq/InnerWorkings#What_is_a_half-closed_filedescriptor.3F
  this.httpAllowHalfOpen = false;

  this.addListener('connection', connectionListener);
}
util.inherits(Server, net.Server);


exports.Server = Server;


exports.createServer = function(requestListener) {
  return new Server(requestListener);
};


function connectionListener(socket) {
  var self = this;
  var outgoing = [];
  var incoming = [];
  var abortError = null;

  function abortIncoming() {
    if (!abortError) {
      abortError = new Error('http.ServerRequest was aborted by the client');
      abortError.code = 'aborted';
    }

    while (incoming.length) {
      var req = incoming.shift();

      // @deprecated, should be removed in 0.5.x
      req.emit('aborted');
      req.emit('close', abortError);
    }
    // abort socket._httpMessage ?
  }

  debug('SERVER new http connection');

  httpSocketSetup(socket);

  socket.setTimeout(2 * 60 * 1000); // 2 minute timeout
  socket.addListener('timeout', function() {
    abortError = new Error('http.ServerRequest timed out');
    abortError.code = 'timeout';
    socket.destroy();
  });

  var parser = parsers.alloc();
  parser.reinitialize('request');
  parser.socket = socket;
  parser.incoming = null;

  socket.addListener('error', function(e) {
    self.emit('clientError', e);
  });

  socket.ondata = function(d, start, end) {
    var ret = parser.execute(d, start, end - start);
    if (ret instanceof Error) {
      debug('parse error');
      socket.destroy(ret);
    } else if (parser.incoming && parser.incoming.upgrade) {
      var bytesParsed = ret;
      socket.ondata = null;
      socket.onend = null;

      var req = parser.incoming;

      // This is start + byteParsed + 1 due to the error of getting \n
      // in the upgradeHead from the closing lines of the headers
      var upgradeHead = d.slice(start + bytesParsed + 1, end);

      if (self.listeners('upgrade').length) {
        self.emit('upgrade', req, req.socket, upgradeHead);
      } else {
        // Got upgrade header, but have no handler.
        socket.destroy();
      }
    }
  };

  socket.onend = function() {
    var ret = parser.finish();

    if (ret instanceof Error) {
      debug('parse error');
      socket.destroy(ret);
      return;
    }

    if (!self.httpAllowHalfOpen) {
      abortIncoming();
      if (socket.writable) socket.end();
    } else if (outgoing.length) {
      outgoing[outgoing.length - 1]._last = true;
    } else if (socket._httpMessage) {
      socket._httpMessage._last = true;
    } else {
      if (socket.writable) socket.end();
    }
  };

  socket.addListener('close', function() {
    debug('server socket close');
    // unref the parser for easy gc
    parsers.free(parser);

    abortIncoming();
  });

  // The following callback is issued after the headers have been read on a
  // new message. In this callback we setup the response object and pass it
  // to the user.
  parser.onIncoming = function(req, shouldKeepAlive) {
    incoming.push(req);

    var res = new ServerResponse(req);
    debug('server response shouldKeepAlive: ' + shouldKeepAlive);
    res.shouldKeepAlive = shouldKeepAlive;
    DTRACE_HTTP_SERVER_REQUEST(req, socket);

    if (socket._httpMessage) {
      // There are already pending outgoing res, append.
      outgoing.push(res);
    } else {
      res.assignSocket(socket);
    }

    // When we're finished writing the response, check if this is the last
    // respose, if so destroy the socket.
    res.on('finish', function() {
      // Usually the first incoming element should be our request.  it may
      // be that in the case abortIncoming() was called that the incoming
      // array will be empty.
      assert(incoming.length == 0 || incoming[0] === req);

      incoming.shift();

      res.detachSocket(socket);

      if (res._last) {
        socket.destroySoon();
      } else {
        // start sending the next message
        var m = outgoing.shift();
        if (m) {
          m.assignSocket(socket);
        }
      }
    });

    if ('expect' in req.headers &&
        (req.httpVersionMajor == 1 && req.httpVersionMinor == 1) &&
        continueExpression.test(req.headers['expect'])) {
      res._expect_continue = true;
      if (self.listeners('checkContinue').length) {
        self.emit('checkContinue', req, res);
      } else {
        res.writeContinue();
        self.emit('request', req, res);
      }
    } else {
      self.emit('request', req, res);
    }
    return false; // Not a HEAD response. (Not even a response!)
  };
}
exports._connectionListener = connectionListener;


function Agent(options) {
  this.options = options;
  this.host = options.host;
  this.port = options.port || this.defaultPort;

  this.queue = [];
  this.sockets = [];
  this.maxSockets = Agent.defaultMaxSockets;
}
util.inherits(Agent, EventEmitter);
exports.Agent = Agent;


Agent.defaultMaxSockets = 5;

Agent.prototype.defaultPort = 80;
Agent.prototype.appendMessage = function(options) {
  var self = this;

  var req = new ClientRequest(options, this.defaultPort);
  this.queue.push(req);
  req._queue = this.queue;

  this._cycle();

  return req;
};


Agent.prototype._removeSocket = function(socket) {
  var i = this.sockets.indexOf(socket);
  if (i >= 0) this.sockets.splice(i, 1);
};


Agent.prototype._establishNewConnection = function() {
  var self = this;
  assert(this.sockets.length < this.maxSockets);

  // Grab a new "socket". Depending on the implementation of _getConnection
  // this could either be a raw TCP socket or a TLS stream.
  var socket = this._getConnection(this.host, this.port, function() {
    socket._httpConnecting = false;
    self.emit('connect'); // mostly for the shim.
    debug('Agent _getConnection callback');
    self._cycle();
  });

  // Use this special mark so that we know if the socket is connecting.
  // TODO: come up with a standard way of specifying that a stream is being
  // connected across tls and net.
  socket._httpConnecting = true;

  this.sockets.push(socket);

  // Cycle so the request can be assigned to this new socket.
  self._cycle();
  assert(socket._httpMessage);

  // Add a parser to the socket.
  var parser = parsers.alloc();
  parser.reinitialize('response');
  parser.socket = socket;
  parser.incoming = null;

  socket.on('error', function(err) {
    debug('AGENT SOCKET ERROR: ' + err.message + '\n' + err.stack);
    var req;
    if (socket._httpMessage) {
      req = socket._httpMessage;
    } else if (self.queue.length) {
      req = self.queue.shift();
      assert(req._queue === self.queue);
      req._queue = null;
    }

    if (req) {
      req.emit('error', err);
      req._hadError = true; // hacky
    }

    // clean up so that agent can handle new requests
    parser.finish();
    socket.destroy();
    self._removeSocket(socket);
    self._cycle();
  });

  socket.ondata = function(d, start, end) {
    var ret = parser.execute(d, start, end - start);
    if (ret instanceof Error) {
      debug('parse error');
      socket.destroy(ret);
    } else if (parser.incoming && parser.incoming.upgrade) {
      var bytesParsed = ret;
      socket.ondata = null;
      socket.onend = null;

      var res = parser.incoming;
      assert(socket._httpMessage);
      socket._httpMessage.res = res;

      // This is start + byteParsed + 1 due to the error of getting \n
      // in the upgradeHead from the closing lines of the headers
      var upgradeHead = d.slice(start + bytesParsed + 1, end);

      // Make sure we don't try to send HTTP requests to it.
      self._removeSocket(socket);

      socket.on('end', function() {
        self.emit('end');
      });

      // XXX free the parser?

      if (self.listeners('upgrade').length) {
        // Emit 'upgrade' on the Agent.
        self.emit('upgrade', res, socket, upgradeHead);
      } else {
        // Got upgrade header, but have no handler.
        socket.destroy();
      }
    }
  };

  socket.onend = function() {
    self.emit('end'); // mostly for the shim.
    parser.finish();
    socket.destroy();
  };

  // When the socket closes remove it from the list of available sockets.
  socket.on('close', function() {
    debug('AGENT socket close');
    // This is really hacky: What if someone issues a request, the server
    // accepts, but then terminates the connection. There is no parse error,
    // there is no socket-level error. How does the user get informed?
    // We check to see if the socket has a request, if so if it has a
    // response (meaning that it emitted a 'response' event). If the socket
    // has a request but no response and it never emitted an error event:
    // THEN we need to trigger it manually.
    // There must be a better way to do this.
    if (socket._httpMessage) {
      if (socket._httpMessage.res) {
        socket._httpMessage.res.emit('aborted');
        socket._httpMessage.res.emit('close');
      } else {
        if (!socket._httpMessage._hadError) {
          socket._httpMessage.emit('error', new Error('socket hang up'));
        }
      }
    }

    self._removeSocket(socket);
    // unref the parser for easy gc
    parsers.free(parser);

    self._cycle();
  });

  parser.onIncoming = function(res, shouldKeepAlive) {
    debug('AGENT incoming response!');

    // If we're receiving a message but we don't have a corresponding
    // request - then somehow the server is seriously messed up and sending
    // multiple responses at us. In this case we'll just bail.
    if (!socket._httpMessage) {
      socket.destroy();
      return;
    }

    var req = socket._httpMessage;
    req.res = res;

    // Responses to HEAD requests are AWFUL. Ask Ryan.
    // A major oversight in HTTP. Hence this nastiness.
    var isHeadResponse = req.method == 'HEAD';
    debug('AGENT isHeadResponse ' + isHeadResponse);

    if (res.statusCode == 100) {
      // restart the parser, as this is a continue message.
      req.emit('continue');
      return true;
    }

    if (req.shouldKeepAlive && res.headers.connection === 'close') {
      req.shouldKeepAlive = false;
    }

    res.addListener('end', function() {
      debug('AGENT request complete');
      // For the moment we reconnect for every request. FIXME!
      // All that should be required for keep-alive is to not reconnect,
      // but outgoingFlush instead.
      if (!req.shouldKeepAlive) {
        if (socket.writable) {
          debug('AGENT socket.destroySoon()');
          socket.destroySoon();
        }
        assert(!socket.writable);
      } else {
        debug('AGENT socket keep-alive');
      }

      // The socket may already be detached and destroyed by an abort call
      if (socket._httpMessage) {
        req.detachSocket(socket);
      }

      assert(!socket._httpMessage);

      self._cycle();
    });

    DTRACE_HTTP_CLIENT_RESPONSE(socket, self);
    req.emit('response', res);

    return isHeadResponse;
  };
};


// Sub-classes can overwrite this method with e.g. something that supplies
// TLS streams.
Agent.prototype._getConnection = function(host, port, cb) {
  debug('Agent connected!');
  var c = net.createConnection(port, host);
  c.on('connect', cb);
  return c;
};


// This method attempts to shuffle items along the queue into one of the
// waiting sockets. If a waiting socket cannot be found, it will
// start the process of establishing one.
Agent.prototype._cycle = function() {
  debug('Agent _cycle sockets=' + this.sockets.length + ' queue=' + this.queue.length);
  var self = this;

  var first = this.queue[0];
  if (!first) return;

  // First try to find an available socket.
  for (var i = 0; i < this.sockets.length; i++) {
    var socket = this.sockets[i];
    // If the socket doesn't already have a message it's sending out
    // and the socket is available for writing or it's connecting.
    // In particular this rules out sockets that are closing.
    if (!socket._httpMessage &&
        ((socket.writable && socket.readable) || socket._httpConnecting)) {
      debug('Agent found socket, shift');
      // We found an available connection!
      this.queue.shift(); // remove first from queue.
      assert(first._queue === this.queue);
      first._queue = null;

      first.assignSocket(socket);
      httpSocketSetup(socket);
      self._cycle(); // try to dispatch another
      return;
    }
  }

  // If no sockets are connecting, and we have space for another we should
  // be starting a new connection to handle this request.
  if (this.sockets.length < this.maxSockets) {
    this._establishNewConnection();
  }

  // All sockets are filled and all sockets are busy.
};


// process-wide hash of agents.
// keys: "host:port" string
// values: instance of Agent
// That is, one agent remote host.
// TODO currently we never remove agents from this hash. This is a small
// memory leak. Have a 2 second timeout after a agent's sockets are to try
// to remove it?
var agents = {};


function getAgent(host, port) {
  port = port || 80;
  var id = host + ':' + port;
  var agent = agents[id];

  if (!agent) {
    agent = agents[id] = new Agent({ host: host, port: port });
  }

  return agent;
}
exports.getAgent = getAgent;


exports._requestFromAgent = function(options, cb) {
  var req = options.agent.appendMessage(options);
  req.agent = options.agent;
  if (cb) req.once('response', cb);
  return req;
};


exports.request = function(options, cb) {
  if (options.agent === undefined) {
    options.agent = getAgent(options.host, options.port);
  } else if (options.agent === false) {
    options.agent = new Agent(options);
  }
  return exports._requestFromAgent(options, cb);
};


exports.get = function(options, cb) {
  options.method = 'GET';
  var req = exports.request(options, cb);
  req.end();
  return req;
};



// Legacy Interface:



function Client() {
  if (!(this instanceof Client)) return new Client();
  net.Stream.call(this, { allowHalfOpen: true });
  var self = this;

  // Possible states:
  // - disconnected
  // - connecting
  // - connected
  this._state = 'disconnected';

  httpSocketSetup(self);
  this._outgoing = [];

  function onData(d, start, end) {
    if (!self.parser) {
      throw new Error('parser not initialized prior to Client.ondata call');
    }
    var ret = self.parser.execute(d, start, end - start);
    if (ret instanceof Error) {
      self.destroy(ret);
    } else if (self.parser.incoming && self.parser.incoming.upgrade) {
      var bytesParsed = ret;
      self.ondata = null;
      self.onend = null;
      self._state = 'upgraded';

      var res = self.parser.incoming;

      if (self._httpMessage) {
        self._httpMessage.detachSocket(self);
      }

      var upgradeHead = d.slice(start + bytesParsed + 1, end);

      if (self.listeners('upgrade').length) {
        self.emit('upgrade', res, self, upgradeHead);
      } else {
        self.destroy();
      }
    }
  };

  self.addListener('connect', function() {
    debug('CLIENT connected');

    self.ondata = onData;
    self.onend = onEnd;

    self._state = 'connected';

    self._initParser();

    self._cycle();
  });

  function onEnd() {
    if (self.parser) self.parser.finish();
    debug('CLIENT got end closing. state = ' + self._state);
    self.end();
  };

  self.addListener('close', function(e) {
    self._state = 'disconnected';
    self._upgraded = false;

    // Free the parser.
    if (self.parser) {
      parsers.free(self.parser);
      self.parser = null;
    }

    if (e) return;

    // If we have an http message, then drop it
    var req = self._httpMessage;
    if (req && !req.res) {
      req.detachSocket(self);
      self.emit('error', new Error('socket hang up'));
    }

    debug('CLIENT onClose. state = ' + self._state);
    self._cycle();
  });
}
util.inherits(Client, net.Stream);


exports.Client = Client;


exports.createClient = function(port, host) {
  var c = new Client();
  c.port = port;
  c.host = host;
  return c;
};


Client.prototype._initParser = function() {
  var self = this;
  if (!self.parser) self.parser = parsers.alloc();
  self.parser.reinitialize('response');
  self.parser.socket = self;
  self.parser.onIncoming = function(res) {
    debug('CLIENT incoming response!');

    assert(self._httpMessage);
    var req = self._httpMessage;

    req.res = res;

    // Responses to HEAD requests are AWFUL. Ask Ryan.
    // A major oversight in HTTP. Hence this nastiness.
    var isHeadResponse = req.method == 'HEAD';
    debug('CLIENT isHeadResponse ' + isHeadResponse);

    if (res.statusCode == 100) {
      // restart the parser, as this is a continue message.
      req.emit('continue');
      return true;
    }

    if (req.shouldKeepAlive && res.headers.connection === 'close') {
      req.shouldKeepAlive = false;
    }

    res.addListener('end', function() {
      debug('CLIENT response complete disconnecting. state = ' + self._state);

      if (!req.shouldKeepAlive) {
        self.end();
      }

      req.detachSocket(self);
      assert(!self._httpMessage);
      self._cycle();
    });

    DTRACE_HTTP_CLIENT_RESPONSE(self, self);
    req.emit('response', res);

    return isHeadResponse;
  };
};


Client.prototype._cycle = function() {
  debug("Client _cycle");
  if (this._upgraded) return;

  switch (this._state) {
    case 'connecting':
      break;

    case 'connected':
      if (this.writable && this.readable) {
        debug("Client _cycle shift()");
        if (this._httpMessage) {
          this._httpMessage._flush();
        } else {
          var req = this._outgoing.shift();
          if (req) {
            req.assignSocket(this);
          }
        }
      }
      break;

    case 'disconnected':
      if (this._httpMessage || this._outgoing.length) {
        this._ensureConnection();
      }
      break;
  }
};


Client.prototype._ensureConnection = function() {
  if (this._state == 'disconnected') {
    debug('CLIENT reconnecting state = ' + this._state);
    this.connect(this.port, this.host);
    this._state = 'connecting';
  }
};


Client.prototype.request = function(method, url, headers) {
  if (typeof(url) != 'string') {
    // assume method was omitted, shift arguments
    headers = url;
    url = method;
    method = 'GET';
  }

  var self = this;

  var options = {
    method: method || 'GET',
    path: url || '/',
    headers: headers
  };

  var req = new ClientRequest(options);
  this._outgoing.push(req);
  this._cycle();

  return req;
};


exports.cat = function(url, encoding_, headers_) {
  var encoding = 'utf8',
      headers = {},
      callback = null;

  // parse the arguments for the various options... very ugly
  if (typeof(arguments[1]) == 'string') {
    encoding = arguments[1];
    if (typeof(arguments[2]) == 'object') {
      headers = arguments[2];
      if (typeof(arguments[3]) == 'function') callback = arguments[3];
    } else {
      if (typeof(arguments[2]) == 'function') callback = arguments[2];
    }
  } else {
    // didn't specify encoding
    if (typeof(arguments[1]) == 'object') {
      headers = arguments[1];
      callback = arguments[2];
    } else {
      callback = arguments[1];
    }
  }

  var url = require('url').parse(url);

  var hasHost = false;
  if (Array.isArray(headers)) {
    for (var i = 0, l = headers.length; i < l; i++) {
      if (headers[i][0].toLowerCase() === 'host') {
        hasHost = true;
        break;
      }
    }
  } else if (typeof headers === 'Object') {
    var keys = Object.keys(headers);
    for (var i = 0, l = keys.length; i < l; i++) {
      var key = keys[i];
      if (key.toLowerCase() == 'host') {
        hasHost = true;
        break;
      }
    }
  }
  if (!hasHost) headers['Host'] = url.hostname;

  var content = '';

  var client = exports.createClient(url.port || 80, url.hostname);
  var req = client.request((url.pathname || '/') +
                           (url.search || '') +
                           (url.hash || ''),
                           headers);

  if (url.protocol == 'https:') {
    client.https = true;
  }

  var callbackSent = false;

  req.addListener('response', function(res) {
    if (res.statusCode < 200 || res.statusCode >= 300) {
      if (callback && !callbackSent) {
        callback(res.statusCode);
        callbackSent = true;
      }
      client.end();
      return;
    }
    res.setEncoding(encoding);
    res.addListener('data', function(chunk) { content += chunk; });
    res.addListener('end', function() {
      if (callback && !callbackSent) {
        callback(null, content);
        callbackSent = true;
      }
    });
  });

  client.addListener('error', function(err) {
    if (callback && !callbackSent) {
      callback(err);
      callbackSent = true;
    }
  });

  client.addListener('close', function() {
    if (callback && !callbackSent) {
      callback(new Error('Connection closed unexpectedly'));
      callbackSent = true;
    }
  });
  req.end();
};

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
  var parser = new HTTPParser(HTTPParser.REQUEST);

  parser._headers = [];
  parser._url = '';

  // Only called in the slow case where slow means
  // that the request headers were either fragmented
  // across multiple TCP packets or too large to be
  // processed in a single run. This method is also
  // called to process trailing HTTP headers.
  parser.onHeaders = function(headers, url) {
    parser._headers = parser._headers.concat(headers);
    parser._url += url;
  };

  // info.headers and info.url are set only if .onHeaders()
  // has not been called for this request.
  //
  // info.url is not set for response parsers but that's not
  // applicable here since all our parsers are request parsers.
  parser.onHeadersComplete = function(info) {
    var headers = info.headers;
    var url = info.url;

    if (!headers) {
      headers = parser._headers;
      parser._headers = [];
    }

    if (!url) {
      url = parser._url;
      parser._url = '';
    }

    parser.incoming = new IncomingMessage(parser.socket);
    parser.incoming.httpVersionMajor = info.versionMajor;
    parser.incoming.httpVersionMinor = info.versionMinor;
    parser.incoming.httpVersion = info.versionMajor + '.' + info.versionMinor;
    parser.incoming.url = url;

    for (var i = 0, n = headers.length; i < n; i += 2) {
      var k = headers[i];
      var v = headers[i + 1];
      parser.incoming._addHeaderLine(k.toLowerCase(), v);
    }

    if (info.method) {
      // server only
      parser.incoming.method = info.method;
    } else {
      // client only
      parser.incoming.statusCode = info.statusCode;
      // CHECKME dead code? we're always a request parser
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
    parser.incoming.complete = true;

    // Emit any trailing headers.
    var headers = parser._headers;
    if (headers) {
      for (var i = 0, n = headers.length; i < n; i += 2) {
        var k = headers[i];
        var v = headers[i + 1];
        parser.incoming._addHeaderLine(k.toLowerCase(), v);
      }
      parser._headers = [];
      parser._url = '';
    }

    if (!parser.incoming.upgrade) {
      // For upgraded connections, also emit this after parser.execute
      parser.incoming.readable = false;
      parser.incoming.emit('end');
    }

    if (parser.socket.readable) {
      // force to read the next incoming message
      parser.socket.resume();
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
    case 'www-authenticate':
    case 'sec-websocket-extensions':
    case 'sec-websocket-protocol':
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
  if (sentConnectionHeader === false) {
    if (this.shouldKeepAlive &&
        (sentContentLengthHeader || this.useChunkedEncodingByDefault || this.agent)) {
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

  if (typeof chunk !== 'string' && !Buffer.isBuffer(chunk)) {
    throw new TypeError('first argument must be a string or Buffer');
  }

  if (chunk.length === 0) return false;

  var len, ret;
  if (this.chunkedEncoding) {
    if (typeof(chunk) === 'string') {
      len = Buffer.byteLength(chunk, encoding);
      chunk = len.toString(16) + CRLF + chunk + CRLF;
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
  debug('outgoing message end.');
  if (this.output.length === 0 && this.connection._httpMessage === this) {
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

function onServerResponseClose() {
  this._httpMessage.emit('close');
}

ServerResponse.prototype.assignSocket = function(socket) {
  assert(!socket._httpMessage);
  socket._httpMessage = this;
  socket.on('close', onServerResponseClose);
  this.socket = socket;
  this.connection = socket;
  this._flush();
};

ServerResponse.prototype.detachSocket = function(socket) {
  assert(socket._httpMessage == this);
  socket.removeListener('close', onServerResponseClose);
  socket._httpMessage = null;
  this.socket = this.connection = null;
};

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
      var keys = Object.keys(obj);
      for (var i = 0; i < keys.length; i++) {
        var k = keys[i];
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


// New Agent code.

// The largest departure from the previous implementation is that
// an Agent instance holds connections for a variable number of host:ports.
// Surprisingly, this is still API compatible as far as third parties are
// concerned. The only code that really notices the difference is the
// request object.

// Another departure is that all code related to HTTP parsing is in
// ClientRequest.onSocket(). The Agent is now *strictly*
// concerned with managing a connection pool.

function Agent(options) {
  var self = this;
  self.options = options || {};
  self.requests = {};
  self.sockets = {};
  self.maxSockets = self.options.maxSockets || Agent.defaultMaxSockets;
  self.on('free', function(socket, host, port) {
    var name = host + ':' + port;
    if (self.requests[name] && self.requests[name].length) {
      self.requests[name].shift().onSocket(socket);
      if (self.requests[name].length === 0) {
        // don't leak
        delete self.requests[name];
      }
    } else {
      // If there are no pending requests just destroy the
      // socket and it will get removed from the pool. This
      // gets us out of timeout issues and allows us to
      // default to Connection:keep-alive.
      socket.destroy();
    }
  });
  self.createConnection = net.createConnection;
}
util.inherits(Agent, EventEmitter);
exports.Agent = Agent;

Agent.defaultMaxSockets = 5;

Agent.prototype.defaultPort = 80;
Agent.prototype.addRequest = function(req, host, port) {
  var name = host + ':' + port;
  if (!this.sockets[name]) {
    this.sockets[name] = [];
  }
  if (this.sockets[name].length < this.maxSockets) {
    // If we are under maxSockets create a new one.
    req.onSocket(this.createSocket(name, host, port));
  } else {
    // We are over limit so we'll add it to the queue.
    if (!this.requests[name]) {
      this.requests[name] = [];
    }
    this.requests[name].push(req);
  }
};
Agent.prototype.createSocket = function(name, host, port) {
  var self = this;
  var s = self.createConnection(port, host, self.options);
  if (!self.sockets[name]) {
    self.sockets[name] = [];
  }
  this.sockets[name].push(s);
  var onFree = function() {
    self.emit('free', s, host, port);
  }
  s.on('free', onFree);
  var onClose = function(err) {
    // This is the only place where sockets get removed from the Agent.
    // If you want to remove a socket from the pool, just close it.
    // All socket errors end in a close event anyway.
    self.removeSocket(s, name, host, port);
  }
  s.on('close', onClose);
  var onRemove = function() {
    // We need this function for cases like HTTP "upgrade"
    // (defined by WebSockets) where we need to remove a socket from the pool
    //  because it'll be locked up indefinitely
    self.removeSocket(s, name, host, port);
    s.removeListener('close', onClose);
    s.removeListener('free', onFree);
    s.removeListener('agentRemove', onRemove);
  }
  s.on('agentRemove', onRemove);
  return s;
};
Agent.prototype.removeSocket = function(s, name, host, port) {
  if (this.sockets[name]) {
    var index = this.sockets[name].indexOf(s);
    if (index !== -1) {
      this.sockets[name].splice(index, 1);
      if (this.sockets[name].length === 0) {
        // don't leak
        delete this.sockets[name];
      }
    }
  }
  if (this.requests[name] && this.requests[name].length) {
    // If we have pending requests and a socket gets closed a new one
    // needs to be created to take over in the pool for the one that closed.
    this.createSocket(name, host, port).emit('free');
  }
};

var globalAgent = new Agent();
exports.globalAgent = globalAgent;


function ClientRequest(options, cb) {
  var self = this;
  OutgoingMessage.call(self);
  self.agent = options.agent;
  options.defaultPort = options.defaultPort || 80;

  options.port = options.port || options.defaultPort;
  options.host = options.hostname || options.host || 'localhost';

  if (options.setHost === undefined) {
    options.setHost = true;
  }

  self.socketPath = options.socketPath;

  var method = self.method = (options.method || 'GET').toUpperCase();
  self.path = options.path || '/';
  if (cb) {
    self.on('response', cb);
  }

  if (!Array.isArray(options.headers)) {
    if (options.headers) {
      var keys = Object.keys(options.headers);
      for (var i = 0, l = keys.length; i < l; i++) {
        var key = keys[i];
        self.setHeader(key, options.headers[key]);
      }
    }
    if (options.host && !this.getHeader('host') && options.setHost) {
      var hostHeader = options.host;
      if (options.port && +options.port !== options.defaultPort) {
        hostHeader += ':' + options.port;
      }
      this.setHeader('Host', hostHeader);
    }
  }

  if (options.auth && !this.getHeader('Authorization')) {
    //basic auth
    this.setHeader('Authorization', 'Basic ' +
                   new Buffer(options.auth).toString('base64'));
  }

  if (method === 'GET' || method === 'HEAD') {
    self.useChunkedEncodingByDefault = false;
  } else {
    self.useChunkedEncodingByDefault = true;
  }

  if (Array.isArray(options.headers)) {
    self._storeHeader(self.method + ' ' + self.path + ' HTTP/1.1\r\n',
                      options.headers);
  } else if (self.getHeader('expect')) {
    self._storeHeader(self.method + ' ' + self.path + ' HTTP/1.1\r\n',
                      self._renderHeaders());
  }
  if (self.socketPath) {
    self._last = true;
    self.shouldKeepAlive = false;
    if (options.createConnection) {
      self.onSocket(options.createConnection(self.socketPath));
    } else {
      self.onSocket(net.createConnection(self.socketPath));
    }
  } else if (self.agent) {
    // If there is an agent we should default to Connection:keep-alive.
    self._last = false;
    self.shouldKeepAlive = true;
    self.agent.addRequest(self, options.host, options.port);
  } else {
    // No agent, default to Connection:close.
    self._last = true;
    self.shouldKeepAlive = false;
    if (options.createConnection) {
      self.onSocket(options.createConnection(options.port, options.host, options));
    } else {
      self.onSocket(net.createConnection(options.port, options.host));
    }
  }

  self._deferToConnect(null, null, function() {
    self._flush();
  });

}
util.inherits(ClientRequest, OutgoingMessage);

exports.ClientRequest = ClientRequest;

ClientRequest.prototype._implicitHeader = function() {
  this._storeHeader(this.method + ' ' + this.path + ' HTTP/1.1\r\n', this._renderHeaders());
};

ClientRequest.prototype.abort = function() {
  if (this.socket) {
    // in-progress
    this.socket.destroy();
  } else {
    // haven't been assigned a socket yet.
    // this could be more efficient, it could
    // remove itself from the pending requests
    this._deferToConnect('destroy', []);
  }
};


function createHangUpError() {
  var error = new Error('socket hang up');
  error.code = 'ECONNRESET';
  return error;
}


ClientRequest.prototype.onSocket = function(socket) {
  var req = this;
  process.nextTick(function() {
    var parser = parsers.alloc();
    req.socket = socket;
    req.connection = socket;
    parser.reinitialize(HTTPParser.RESPONSE);
    parser.socket = socket;
    parser.incoming = null;
    req.parser = parser;

    socket._httpMessage = req;
    // Setup "drain" propogation.
    httpSocketSetup(socket);

    var freeParser = function() {
      if (parser) {
        parsers.free(parser);
        parser = null;
      }
    };

    var errorListener = function(err) {
      debug('HTTP SOCKET ERROR: ' + err.message + '\n' + err.stack);
      req.emit('error', err);
      // For Safety. Some additional errors might fire later on
      // and we need to make sure we don't double-fire the error event.
      req._hadError = true;
      if (parser) {
        parser.finish();
        freeParser();
      }
      socket.destroy();
    }
    socket.on('error', errorListener);

    socket.ondata = function(d, start, end) {
      var ret = parser.execute(d, start, end - start);
      if (ret instanceof Error) {
        debug('parse error');
        freeParser();
        socket.destroy(ret);
      } else if (parser.incoming && parser.incoming.upgrade) {
        var bytesParsed = ret;
        socket.ondata = null;
        socket.onend = null;

        var res = parser.incoming;
        req.res = res;

        // This is start + byteParsed
        var upgradeHead = d.slice(start + bytesParsed, end);
        if (req.listeners('upgrade').length) {
          // Emit 'upgrade' on the Agent.
          req.upgraded = true;
          req.emit('upgrade', res, socket, upgradeHead);
          socket.emit('agentRemove');
        } else {
          // Got upgrade header, but have no handler.
          socket.destroy();
        }
        freeParser();
      } else if (parser.incoming && parser.incoming.complete &&
                 // When the status code is 100 (Continue), the server will
                 // send a final response after this client sends a request
                 // body. So, we must not free the parser.
                 parser.incoming.statusCode !== 100) {
        freeParser();
      }
    };

    socket.onend = function() {
      if (!req.res) {
        // If we don't have a response then we know that the socket
        // ended prematurely and we need to emit an error on the request.
        req.emit('error', createHangUpError());
        req._hadError = true;
      }
      if (parser) {
        parser.finish();
        freeParser();
      }
      socket.destroy();
    };

    var closeListener = function() {
      debug('HTTP socket close');
      req.emit('close');
      if (req.res && req.res.readable) {
        // Socket closed before we emitted "end" below.
        req.res.emit('aborted');
        req.res.emit('end');
        req.res.emit('close');
      } else if (!req.res && !req._hadError) {
        // This socket error fired before we started to
        // receive a response. The error needs to
        // fire on the request.
        req.emit('error', createHangUpError());
      }
    }
    socket.on('close', closeListener);

    parser.onIncoming = function(res, shouldKeepAlive) {
      debug('AGENT incoming response!');

      if (req.res) {
        // We already have a response object, this means the server
        // sent a double response.
        socket.destroy();
        return;
      }
      req.res = res;

      // Responses to HEAD requests are crazy.
      // HEAD responses aren't allowed to have an entity-body
      // but *can* have a content-length which actually corresponds
      // to the content-length of the entity-body had the request
      // been a GET.
      var isHeadResponse = req.method == 'HEAD';
      debug('AGENT isHeadResponse ' + isHeadResponse);

      if (res.statusCode == 100) {
        // restart the parser, as this is a continue message.
        delete req.res; // Clear res so that we don't hit double-responses.
        req.emit('continue');
        return true;
      }

      if (req.shouldKeepAlive && !shouldKeepAlive && !req.upgraded) {
        // Server MUST respond with Connection:keep-alive for us to enable it.
        // If we've been upgraded (via WebSockets) we also shouldn't try to
        // keep the connection open.
        req.shouldKeepAlive = false;
      }

      res.addListener('end', function() {
        if (!req.shouldKeepAlive) {
          if (socket.writable) {
            debug('AGENT socket.destroySoon()');
            socket.destroySoon();
          }
          assert(!socket.writable);
        } else {
          debug('AGENT socket keep-alive');
        }
      });

      DTRACE_HTTP_CLIENT_RESPONSE(socket, req);
      req.emit('response', res);

      res.on('end', function() {
        if (req.shouldKeepAlive) {
          socket.removeListener('close', closeListener);
          socket.removeListener('error', errorListener);
          socket.emit('free');
        }
      });

      return isHeadResponse;
    };
    req.emit('socket', socket);
  });

};
ClientRequest.prototype._deferToConnect = function(method, arguments_, cb) {
  // This function is for calls that need to happen once the socket is
  // connected and writable. It's an important promisy thing for all the socket
  // calls that happen either now (when a socket is assigned) or
  // in the future (when a socket gets assigned out of the pool and is
  // eventually writable).
  var self = this;
  var onSocket = function() {
    if (self.socket.writable) {
      if (method) {
        self.socket[method].apply(self.socket, arguments_);
      }
      if (cb) { cb(); }
    } else {
      self.socket.on('connect', function() {
        if (method) {
          self.socket[method].apply(self.socket, arguments_);
        }
        if (cb) { cb(); }
      });
    }
  }
  if (!self.socket) {
    self.once('socket', onSocket);
  } else {
    onSocket();
  }
};
ClientRequest.prototype.setTimeout = function() {
  this._deferToConnect('setTimeout', arguments);
};
ClientRequest.prototype.setNoDelay = function() {
  this._deferToConnect('setNoDelay', arguments);
};
ClientRequest.prototype.setSocketKeepAlive = function() {
  this._deferToConnect('setKeepAlive', arguments);
};
ClientRequest.prototype.pause = function() {
  var self = this;
  self._deferToConnect(null, null, function() {
    OutgoingMessage.prototype.pause.apply(self, []);
  });
};


exports.request = function(options, cb) {
  if (options.protocol && options.protocol !== 'http:') {
    throw new Error('Protocol:' + options.protocol + ' not supported.');
  }

  if (options.agent === undefined) {
    options.agent = globalAgent;
  }

  return new ClientRequest(options, cb);
};

exports.get = function(options, cb) {
  options.method = 'GET';
  var req = exports.request(options, cb);
  req.end();
  return req;
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

  function abortIncoming() {
    while (incoming.length) {
      var req = incoming.shift();
      req.emit('aborted');
      req.emit('close');
    }
    // abort socket._httpMessage ?
  }

  debug('SERVER new http connection');

  httpSocketSetup(socket);

  socket.setTimeout(2 * 60 * 1000); // 2 minute timeout
  socket.addListener('timeout', function() {
    socket.destroy();
  });

  var parser = parsers.alloc();
  parser.reinitialize(HTTPParser.REQUEST);
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

      // This is start + byteParsed
      var upgradeHead = d.slice(start + bytesParsed, end);

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

// Legacy Interface

function Client(port, host) {
  host = host || 'localhost';
  port = port || 80;
  this.host = host;
  this.port = port;
  this.agent = new Agent({ host: host, port: port, maxSockets: 1 });
}
util.inherits(Client, EventEmitter);
Client.prototype.request = function(method, path, headers) {
  var self = this;
  var options = {};
  options.host = self.host;
  options.port = self.port;
  if (method[0] === '/') {
    headers = path;
    path = method;
    method = 'GET';
  }
  options.method = method;
  options.path = path;
  options.headers = headers;
  options.agent = self.agent;
  var c = new ClientRequest(options);
  c.on('error', function(e) {
    self.emit('error', e);
  });
  // The old Client interface emitted "end" on socket end.
  // This doesn't map to how we want things to operate in the future
  // but it will get removed when we remove this legacy interface.
  c.on('socket', function(s) {
    s.on('end', function() {
      self.emit('end');
    });
  });
  return c;
};

exports.Client = Client;
exports.createClient = function(port, host) {
  return new Client(port, host);
};

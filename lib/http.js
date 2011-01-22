var util = require('util');
var net = require('net');
var stream = require('stream');
var EventEmitter = require('events').EventEmitter;
var FreeList = require('freelist').FreeList;
var HTTPParser = process.binding('http_parser').HTTPParser;
var assert = process.assert;


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
    encoding = encoding || 'ascii';
    this.outputEncodings.push(encoding);
    return false;
  }

  var lastEncoding = this.outputEncodings[length - 1];
  var lastData = this.output[length - 1];

  if ((lastEncoding === encoding) ||
      (!encoding && data.constructor === lastData.constructor)) {
    this.output[length - 1] = lastData + data;
    return false;
  }

  this.output.push(data);
  encoding = encoding || 'ascii';
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


OutgoingMessage.prototype.write = function(chunk, encoding) {
  if (!this._header) {
    throw new Error('You have to call writeHead() before write()');
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
      debug('string chunk = ' + util.inspect(chunk));
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
  var ret;

  var hot = this._headerSent === false &&
            typeof(data) === 'string' &&
            data.length > 0 &&
            this.output.length === 0 &&
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


ServerResponse.prototype.writeContinue = function() {
  this._writeRaw('HTTP/1.1 100 Continue' + CRLF + CRLF, 'ascii');
  this._sent100 = true;
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

  if (typeof arguments[headerIndex] == 'object') {
    headers = arguments[headerIndex];
  } else {
    headers = {};
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


function ClientRequest(options) {
  OutgoingMessage.call(this);

  var method = this.method = (options.method || 'GET').toUpperCase();
  var path = options.path || '/';
  var headers = options.headers || {};

  // Host header set by default.
  if (options.host && !(headers.host || headers.Host || headers.HOST)) {
    headers.Host = options.host;
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

  this._storeHeader(method + ' ' + path + ' HTTP/1.1\r\n', headers);
}
util.inherits(ClientRequest, OutgoingMessage);


exports.ClientRequest = ClientRequest;


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

  debug('SERVER new http connection');

  httpSocketSetup(socket);

  socket.setTimeout(2 * 60 * 1000); // 2 minute timeout
  socket.addListener('timeout', function() {
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
    parser.finish();

    if (outgoing.length) {
      outgoing[outgoing.length - 1]._last = true;
    } else if (socket._httpMessage) {
      socket._httpMessage._last = true;
    } else {
      socket.end();
    }
  };

  socket.addListener('close', function() {
    // unref the parser for easy gc
    parsers.free(parser);
  });

  // The following callback is issued after the headers have been read on a
  // new message. In this callback we setup the response object and pass it
  // to the user.
  parser.onIncoming = function(req, shouldKeepAlive) {
    var res = new ServerResponse(req);
    debug('server response shouldKeepAlive: ' + shouldKeepAlive);
    res.shouldKeepAlive = shouldKeepAlive;

    if (socket._httpMessage) {
      // There are already pending outgoing res, append.
      outgoing.push(res);
    } else {
      res.assignSocket(socket);
    }

    // When we're finished writing the response, check if this is the last
    // respose, if so destroy the socket.
    res.on('finish', function() {
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


function Agent(host, port) {
  this.host = host;
  this.port = port;

  this.queue = [];
  this.sockets = [];
  this.maxSockets = 5;
}
util.inherits(Agent, EventEmitter);
exports.Agent = Agent;


Agent.prototype.appendMessage = function(options) {
  var self = this;

  var req = new ClientRequest(options);
  this.queue.push(req);

  /*
  req.on('finish', function () {
    self._cycle();
  });
  */

  this._cycle();

  return req;
};


Agent.prototype._removeSocket = function(socket) {
  var i = this.sockets.indexOf(socket);
  if (i >= 0) this.sockets.splice(i, 1);
}


Agent.prototype._establishNewConnection = function() {
  var self = this;
  assert(this.sockets.length < this.maxSockets);

  // Grab a new "socket". Depending on the implementation of _getConnection
  // this could either be a raw TCP socket or a TLS stream.
  var socket = this._getConnection(this.host, this.port, function () {
    socket._httpConnecting = false;
    self.emit('connect'); // mostly for the shim.
    debug("Agent _getConnection callback");
    self._cycle();
  });

  // Use this special mark so that we know if the socket is connecting.
  // TODO: come up with a standard way of specifying that a stream is being
  // connected across tls and net.
  socket._httpConnecting = true;

  this.sockets.push(socket);

  // Add a parser to the socket.
  var parser = parsers.alloc();
  parser.reinitialize('response');
  parser.socket = socket;
  parser.incoming = null;

  socket.on('error', function(err) {
    debug("AGENT SOCKET ERROR: " + err.message);
    var req; 
    if (socket._httpMessage) {
      req = socket._httpMessage
    } else if (self.queue.length) {
      req = self.queue.shift();
    } else {
      // No requests on queue? Where is the request
      assert(0);
    }

    req.emit('error', err);
    req._hadError = true; // hacky
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
    // This is really hacky: What if someone issues a request, the server
    // accepts, but then terminates the connection. There is no parse error,
    // there is no socket-level error. How does the user get informed?
    // We check to see if the socket has a request, if so if it has a
    // response (meaning that it emitted a 'response' event). If the socket
    // has a request but no response and it never emitted an error event:
    // THEN we need to trigger it manually.
    // There must be a better way to do this.
    if (socket._httpMessage &&
        !socket._httpMessage.res &&
        !socket._httpMessage._hadError) {
      socket._httpMessage.emit('error', new Error('socket hang up'));
    }

    self._removeSocket(socket);
    // unref the parser for easy gc
    parsers.free(parser);

    self._cycle();
  });

  parser.onIncoming = function(res, shouldKeepAlive) {
    debug('AGENT incoming response!');

    var req = socket._httpMessage;
    assert(req);

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
        debug('AGENT socket.end()');
        socket.end();
      } else {
        debug('AGENT socket keep-alive');
      }

      req.detachSocket(socket);

      assert(!socket._httpMessage);

      self._cycle();
    });

    req.emit('response', res);

    return isHeadResponse;
  };
};


// Sub-classes can overwrite this method with e.g. something that supplies
// TLS streams.
Agent.prototype._getConnection = function(host, port, cb) {
  debug("Agent connected!");
  var c =  net.createConnection(port, host);
  c.on('connect', cb);
  return c;
};


// This method attempts to shuffle items along the queue into one of the
// waiting sockets. If a waiting socket cannot be found, it will
// start the process of establishing one.
Agent.prototype._cycle = function() {
  debug("Agent _cycle sockets=" + this.sockets.length + " queue=" + this.queue.length);
  var self = this;

  var first = this.queue[0];
  if (!first) return;


  var haveConnectingSocket = false;

  // First try to find an available socket.
  for (var i = 0; i < this.sockets.length; i++) {
    var socket = this.sockets[i];
    // If the socket doesn't already have a message it's sending out
    // and the socket is available for writing...
    if (!socket._httpMessage && (socket.writable && socket.readable)) {
      debug("Agent found socket, shift");
      // We found an available connection!
      this.queue.shift(); // remove first from queue.
      first.assignSocket(socket);
      self._cycle(); // try to dispatch another
      return;
    }

    if (socket._httpConnecting) haveConnectingSocket = true;
  }

  // If no sockets are connecting, and we have space for another we should
  // be starting a new connection to handle this request.
  if (!haveConnectingSocket && this.sockets.length < this.maxSockets) {
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
var agents = {}


function getAgent(host, port) {
  var id = host + ':' + port;
  var agent = agents[id];

  if (!agent) {
    agent = agents[id] = new Agent(host, port);
  }

  return agent;
}
exports.getAgent = getAgent;


exports._requestFromAgent = function(agent, options, cb) {
  var req = agent.appendMessage(options);
  if (cb) req.once('response', cb);
  return req;
};


exports.request = function(options, cb) {
  var agent = getAgent(options.host, options.port);
  return exports._requestFromAgent(agent, options, cb);
};


exports.get = function(options, cb) {
  options.method = 'GET';
  var req = exports.request(options, cb);
  req.end();
  return req;
};


// Shims to old interface.

function Client(port, host) {
  var self = this;

  this.port = port;
  this.host = host;
  this.agent = getAgent(this.host, this.port);

  // proxy connect events upwards;
  this.agent.on('connect', function() {
    self.emit('connect');
  });

  this.agent.on('end', function() {
    self.emit('end');
  });

  // proxy upgrade events upwards;
  this.agent.on('upgrade', function (res, socket, upgradeHead) {
    if (self.listeners('upgrade').length) {
      self.emit('upgrade', res, socket, upgradeHead);
    } else {
      socket.destroy();
    }
  });
}
util.inherits(Client, EventEmitter);


// This method is used in a few tests to force the connections closed.
// Again - just a shim so as not to break code. Not really important.
Client.prototype.end = function() {
  for (var i = 0; i < this.agent.sockets.length; i++) {
    var socket = this.agent.sockets[i];
    if (!socket._httpMessage && socket.writable) socket.end();
  }
};


Client.prototype.destroy = function(e) {
  for (var i = 0; i < this.agent.sockets.length; i++) {
    var socket = this.agent.sockets[i];
    socket.destroy(e);
  }
};


Client.prototype.request = function(method, path, headers) {
  if (typeof(path) != 'string') {
    // assume method was omitted, shift arguments
    headers = path;
    path = method;
    method = 'GET';
  }

  var options = {
    method: method,
    path: path,
    headers: headers,
    port: this.port,
    host: this.host
  };

  var self = this;
  var req = exports.request(options);

  // proxy error events from req to Client
  req.on('error', function(err) {
    self.emit('error', err);
  });

  return req;
};


exports.createClient = function(port, host) {
  return new Client(port, host);
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

  var options = {
    method: 'GET',
    port: url.port || 80,
    host: url.hostname,
    headers: headers,
    path: (url.pathname || '/') + (url.search || '') + (url.hash || '')
  };

  var content = '';
  var callbackSent = false;

  var req = exports.request(options, function(res) {
    if (res.statusCode < 200 || res.statusCode >= 300) {
      if (callback && !callbackSent) {
        callback(res.statusCode);
        callbackSent = true;
      }
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
  req.end();


  req.on('error', function(err) {
    if (callback && !callbackSent) {
      callback(err);
      callbackSent = true;
    }
  });
};

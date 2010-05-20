var debugLevel = 0;
if ("NODE_DEBUG" in process.env) debugLevel = 1;

function debug (x) {
  if (debugLevel > 0) {
    process.binding('stdio').writeError(x + "\n");
  }
}

var sys = require('sys');
var net = require('net');
var Utf8Decoder = require('utf8decoder').Utf8Decoder;
var events = require('events');
var Buffer = require('buffer').Buffer;

var FreeList = require('freelist').FreeList;
var HTTPParser = process.binding('http_parser').HTTPParser;

var parsers = new FreeList('parsers', 1000, function () {
  var parser = new HTTPParser('request');

  parser.onMessageBegin = function () {
    parser.incoming = new IncomingMessage(parser.socket);
    parser.field = null;
    parser.value = null;
  };

  // Only servers will get URL events.
  parser.onURL = function (b, start, len) {
    var slice = b.toString('ascii', start, start+len);
    if (parser.incoming.url) {
      parser.incoming.url += slice;
    } else {
      // Almost always will branch here.
      parser.incoming.url = slice;
    }
  };

  parser.onHeaderField = function (b, start, len) {
    var slice = b.toString('ascii', start, start+len).toLowerCase();
    if (parser.value) {
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

  parser.onHeaderValue = function (b, start, len) {
    var slice = b.toString('ascii', start, start+len);
    if (parser.value) {
      parser.value += slice;
    } else {
      parser.value = slice;
    }
  };

  parser.onHeadersComplete = function (info) {
    if (parser.field && parser.value) {
      parser.incoming._addHeaderLine(parser.field, parser.value);
    }

    parser.incoming.httpVersionMajor = info.versionMajor;
    parser.incoming.httpVersionMinor = info.versionMinor;
    parser.incoming.httpVersion = info.versionMajor
                                + '.'
                                + info.versionMinor ;

    if (info.method) {
      // server only
      parser.incoming.method = info.method;
    } else {
      // client only
      parser.incoming.statusCode = info.statusCode;
    }

    parser.incoming.upgrade = info.upgrade;

    if (!info.upgrade) {
      // For upgraded connections, we'll emit this after parser.execute
      // so that we can capture the first part of the new protocol
      parser.onIncoming(parser.incoming, info.shouldKeepAlive);
    }
  };

  parser.onBody = function (b, start, len) {
    // TODO body encoding?
    var enc = parser.incoming._encoding;
    if (!enc) {
      parser.incoming.emit('data', b.slice(start, start+len));
    } else if (this._decoder) {
      this._decoder.write(pool.slice(start, end));
    } else {
      var string = b.toString(enc, start, start+len);
      parser.incoming.emit('data', string);
    }
  };

  parser.onMessageComplete = function () {
    if (!parser.incoming.upgrade) {
      // For upgraded connections, also emit this after parser.execute
      parser.incoming.emit("end");
    }
  };

  return parser;
});


var CRLF = "\r\n";
var STATUS_CODES = exports.STATUS_CODES = {
  100 : 'Continue',
  101 : 'Switching Protocols',
  200 : 'OK',
  201 : 'Created',
  202 : 'Accepted',
  203 : 'Non-Authoritative Information',
  204 : 'No Content',
  205 : 'Reset Content',
  206 : 'Partial Content',
  300 : 'Multiple Choices',
  301 : 'Moved Permanently',
  302 : 'Moved Temporarily',
  303 : 'See Other',
  304 : 'Not Modified',
  305 : 'Use Proxy',
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
  500 : 'Internal Server Error',
  501 : 'Not Implemented',
  502 : 'Bad Gateway',
  503 : 'Service Unavailable',
  504 : 'Gateway Time-out',
  505 : 'HTTP Version not supported'
};

var connectionExpression = /Connection/i;
var transferEncodingExpression = /Transfer-Encoding/i;
var closeExpression = /close/i;
var chunkExpression = /chunk/i;
var contentLengthExpression = /Content-Length/i;


/* Abstract base class for ServerRequest and ClientResponse. */
function IncomingMessage (socket) {
  events.EventEmitter.call(this);

  // TODO Remove one of these eventually.
  this.socket = socket;
  this.connection = socket;

  this.httpVersion = null;
  this.headers = {};

  // request (server) only
  this.url = "";

  this.method = null;

  // response (client) only
  this.statusCode = null;
  this.client = this.socket;
}
sys.inherits(IncomingMessage, events.EventEmitter);
exports.IncomingMessage = IncomingMessage;

IncomingMessage.prototype._parseQueryString = function () {
  throw new Error("_parseQueryString is deprecated. Use require(\"querystring\") to parse query strings.\n");
};

var setBodyEncodingWarning;

IncomingMessage.prototype.setBodyEncoding = function (enc) {
  // deprecation message
  if (!setBodyEncodingWarning) {
    setBodyEncodingWarning = "setBodyEncoding has been renamed to setEncoding, please update your code.";
    sys.error(setBodyEncodingWarning);
  }

  this.setEncoding(enc);
};

IncomingMessage.prototype.setEncoding = function (enc) {
  // TODO check values, error out on bad, and deprecation message?
  this._encoding = enc.toLowerCase();
  if (this._encoding == 'utf-8' || this._encoding == 'utf8') {
    this._decoder = new Utf8Decoder();
    this._decoder.onString = function(str) {
      this.emit('data', str);
    };
  } else if (this._decoder) {
    delete this._decoder;
  }

};

IncomingMessage.prototype.pause = function () {
  this.socket.pause();
};

IncomingMessage.prototype.resume = function () {
  this.socket.resume();
};

IncomingMessage.prototype._addHeaderLine = function (field, value) {
  if (field in this.headers) {
    // TODO Certain headers like 'Content-Type' should not be concatinated.
    // See https://www.google.com/reader/view/?tab=my#overview-page
    this.headers[field] += ", " + value;
  } else {
    this.headers[field] = value;
  }
};

function OutgoingMessage (socket) {
  events.EventEmitter.call(this, socket);

  // TODO Remove one of these eventually.
  this.socket = socket;
  this.connection = socket;

  this.output = [];
  this.outputEncodings = [];

  this.closeOnFinish = false;
  this.chunkedEncoding = false;
  this.shouldKeepAlive = true;
  this.useChunkedEncodingByDefault = true;

  this.flushing = false;
  this.headWritten = false;

  this._hasBody = true;

  this.finished = false;
}
sys.inherits(OutgoingMessage, events.EventEmitter);
exports.OutgoingMessage = OutgoingMessage;

OutgoingMessage.prototype._send = function (data, encoding) {
  var length = this.output.length;

  if (length === 0 || typeof data != 'string') {
    this.output.push(data);
    encoding = encoding || "ascii";
    this.outputEncodings.push(encoding);
    return;
  }

  var lastEncoding = this.outputEncodings[length-1];
  var lastData = this.output[length-1];

  if ((lastEncoding === encoding) ||
      (!encoding && data.constructor === lastData.constructor)) {
    this.output[length-1] = lastData + data;
    return;
  }

  this.output.push(data);
  encoding = encoding || "ascii";
  this.outputEncodings.push(encoding);
};

OutgoingMessage.prototype.sendHeaderLines = function (firstLine, headers) {
  var sentConnectionHeader = false;
  var sentContentLengthHeader = false;
  var sentTransferEncodingHeader = false;

  // firstLine in the case of request is: "GET /index.html HTTP/1.1\r\n"
  // in the case of response it is: "HTTP/1.1 200 OK\r\n"
  var messageHeader = firstLine;
  var field, value;

  if (headers) {
    var keys = Object.keys(headers);
    var isArray = (headers instanceof Array);
    for (var i = 0, l = keys.length; i < l; i++) {
      var key = keys[i];
      if (isArray) {
        field = headers[key][0];
        value = headers[key][1];
      } else {
        field = key;
        value = headers[key];
      }

      messageHeader += field + ": " + value + CRLF;

      if (connectionExpression.test(field)) {
        sentConnectionHeader = true;
        if (closeExpression.test(value)) this.closeOnFinish = true;

      } else if (transferEncodingExpression.test(field)) {
        sentTransferEncodingHeader = true;
        if (chunkExpression.test(value)) this.chunkedEncoding = true;

      } else if (contentLengthExpression.test(field)) {
        sentContentLengthHeader = true;

      }
    }
  }

  // keep-alive logic
  if (sentConnectionHeader == false) {
    if (this.shouldKeepAlive &&
        (sentContentLengthHeader || this.useChunkedEncodingByDefault)) {
      messageHeader += "Connection: keep-alive\r\n";
    } else {
      this.closeOnFinish = true;
      messageHeader += "Connection: close\r\n";
    }
  }

  if (sentContentLengthHeader == false && sentTransferEncodingHeader == false) {
    if (this._hasBody) {
      if (this.useChunkedEncodingByDefault) {
        messageHeader += "Transfer-Encoding: chunked\r\n";
        this.chunkedEncoding = true;
      } else {
        this.closeOnFinish = true;
      }
    } else {
      // Make sure we don't end the 0\r\n\r\n at the end of the message.
      this.chunkedEncoding = false;
    }
  }

  messageHeader += CRLF;

  this._send(messageHeader);
  // wait until the first body chunk, or close(), is sent to flush.
};


OutgoingMessage.prototype.sendBody = function () {
  throw new Error("sendBody() has been renamed to write(). ");
};


OutgoingMessage.prototype.write = function (chunk, encoding) {
  if ( (this instanceof ServerResponse) && !this.headWritten) {
    throw new Error("writeHead() must be called before write()")
  }

  if (!this._hasBody) {
    throw new Error("This type of response MUST NOT have a body.");
  }

  if (typeof chunk !== "string"
      && !(chunk instanceof Buffer)
      && !Array.isArray(chunk)) {
    throw new TypeError("first argument must be a string, Array, or Buffer");
  }

  encoding = encoding || "ascii";
  if (this.chunkedEncoding) {
    if (typeof chunk == 'string') {
      this._send(process._byteLength(chunk, encoding).toString(16));
    } else {
      this._send(chunk.length.toString(16));
    }
    this._send(CRLF);
    this._send(chunk, encoding);
    this._send(CRLF);
  } else {
    this._send(chunk, encoding);
  }

  if (this.flushing) {
    this.flush();
  } else {
    this.flushing = true;
  }
};

OutgoingMessage.prototype.flush = function () {
  this.emit("flush");
};

OutgoingMessage.prototype.finish = function () {
  throw new Error("finish() has been renamed to close().");
};

var closeWarning;

OutgoingMessage.prototype.close = function (data, encoding) {
  if (!closeWarning) {
    closeWarning = "OutgoingMessage.prototype.close has been renamed to end()";
    sys.error(closeWarning);
  }
  return this.end(data, encoding);
};

OutgoingMessage.prototype.end = function (data, encoding) {
  if (data) this.write(data, encoding);
  if (this.chunkedEncoding) this._send("0\r\n\r\n"); // last chunk
  this.finished = true;
  this.flush();
};


function ServerResponse (req) {
  OutgoingMessage.call(this, req.socket);

  if (req.httpVersionMajor < 1 || req.httpVersionMinor < 1) {
    this.useChunkedEncodingByDefault = false;
    this.shouldKeepAlive = false;
  }
}
sys.inherits(ServerResponse, OutgoingMessage);
exports.ServerResponse = ServerResponse;


ServerResponse.prototype.writeHead = function (statusCode) {
  var reasonPhrase, headers, headerIndex;

  if (typeof arguments[1] == 'string') {
    reasonPhrase = arguments[1];
    headerIndex = 2;
  } else {
    reasonPhrase = STATUS_CODES[statusCode] || "unknown";
    headerIndex = 1;
  }

  if (typeof arguments[headerIndex] == 'object') {
    headers = arguments[headerIndex];
  } else {
    headers = {};
  }

  var statusLine = "HTTP/1.1 " + statusCode.toString() + " "
                  + reasonPhrase + CRLF;

  if (statusCode === 204 || statusCode === 304) {
    // RFC 2616, 10.2.5:
    // The 204 response MUST NOT include a message-body, and thus is always
    // terminated by the first empty line after the header fields.
    // RFC 2616, 10.3.5:
    // The 304 response MUST NOT contain a message-body, and thus is always
    // terminated by the first empty line after the header fields.
    this._hasBody = false;
  }


  this.sendHeaderLines(statusLine, headers);
  this.headWritten = true;
};

// TODO Eventually remove
var sendHeaderWarning, writeHeaderWarning;
ServerResponse.prototype.sendHeader = function () {
  if (!sendHeaderWarning) {
    sendHeaderWarning = "sendHeader() has been renamed to writeHead()";
    sys.error(sendHeaderWarning);
  }
  this.writeHead.apply(this, arguments);
};
ServerResponse.prototype.writeHeader = function () {
  if (!writeHeaderWarning) {
    writeHeaderWarning = "writeHeader() has been renamed to writeHead()";
    sys.error(writeHeaderWarning);
  }
  this.writeHead.apply(this, arguments);
};

function ClientRequest (socket, method, url, headers) {
  OutgoingMessage.call(this, socket);

  this.shouldKeepAlive = false;
  if (method === "GET" || method === "HEAD") {
    this.useChunkedEncodingByDefault = false;
  } else {
    this.useChunkedEncodingByDefault = true;
  }
  this.closeOnFinish = true;

  this.sendHeaderLines(method + " " + url + " HTTP/1.1\r\n", headers);
}
sys.inherits(ClientRequest, OutgoingMessage);
exports.ClientRequest = ClientRequest;

ClientRequest.prototype.finish = function () {
  throw new Error( "finish() has been renamed to end() and no longer takes "
                 + "a response handler as an argument. Manually add a 'response' listener "
                 + "to the request object."
                 );
};

var clientRequestCloseWarning;

ClientRequest.prototype.close = function () {
  if (!clientRequestCloseWarning) {
    clientRequestCloseWarning = "Warning: ClientRequest.prototype.close has been renamed to end()";
    sys.error(clientRequestCloseWarning);
  }
  if (arguments.length > 0) {
    throw new Error( "ClientRequest.prototype.end does not take any arguments. "
                   + "Add a response listener manually to the request object."
                   );
  }
  return this.end();
};

ClientRequest.prototype.end = function () {
  if (arguments.length > 0) {
    throw new Error( "ClientRequest.prototype.end does not take any arguments. "
                   + "Add a response listener manually to the request object."
                   );
  }
  OutgoingMessage.prototype.end.call(this);
};


/* Returns true if the message queue is finished and the socket
 * should be closed. */
function flushMessageQueue (socket, queue) {
  while (queue[0]) {
    var message = queue[0];

    while (message.output.length > 0) {
      if (!socket.writable) return true;

      var data = message.output.shift();
      var encoding = message.outputEncodings.shift();

      socket.write(data, encoding);
    }

    if (!message.finished) break;

    message.emit("sent");
    queue.shift();

    if (message.closeOnFinish) return true;
  }
  return false;
}


function Server (requestListener) {
  net.Server.call(this);

  if(requestListener){
    this.addListener("request", requestListener);
  }

  this.addListener("connection", connectionListener);
}
sys.inherits(Server, net.Server);

Server.prototype.setSecure = function (credentials) {
  this.secure = true;
  this.credentials = credentials;
}

exports.Server = Server;

exports.createServer = function (requestListener) {
  return new Server(requestListener);
};

function connectionListener (socket) {
  var self = this;
  // An array of responses for each socket. In pipelined connections
  // we need to keep track of the order they were sent.
  var responses = [];

  socket.setTimeout(2*60*1000); // 2 minute timeout
  socket.addListener('timeout', function () {
    socket.destroy();
  });

  var parser = parsers.alloc();
  parser.reinitialize('request');
  parser.socket = socket;

  if (self.secure) {
    socket.setSecure(self.credentials);
  }

  socket.addListener('error', function (e) {
    self.emit('clientError', e);
  });

  socket.ondata = function (d, start, end) {
    var ret = parser.execute(d, start, end - start);
    if (ret instanceof Error) {
      socket.destroy(ret);
    } else if (parser.incoming && parser.incoming.upgrade) {
      var bytesParsed = ret;
      socket.ondata = null;
      socket.onend = null;

      var req = parser.incoming;

      // This is start + byteParsed + 1 due to the error of getting \n
      // in the upgradeHead from the closing lines of the headers
      var upgradeHead = d.slice(start + bytesParsed + 1, end);

      if (self.listeners("upgrade").length) {
        self.emit('upgrade', req, req.socket, upgradeHead);
      } else {
        // Got upgrade header, but have no handler.
        socket.destroy();
      }
    }
  };

  socket.onend = function () {
    parser.finish();

    // unref the parser for easy gc
    parsers.free(parser);

    if (responses.length == 0) {
      socket.end();
    } else {
      responses[responses.length-1].closeOnFinish = true;
    }
  };

  // The following callback is issued after the headers have been read on a
  // new message. In this callback we setup the response object and pass it
  // to the user.
  parser.onIncoming = function (req, shouldKeepAlive) {
    var res = new ServerResponse(req);

    res.shouldKeepAlive = shouldKeepAlive;
    res.addListener('flush', function () {
      if (flushMessageQueue(socket, responses)) {
        socket.end();
      }
    });
    responses.push(res);

    self.emit('request', req, res);
  };
}


function Client ( ) {
  net.Stream.call(this);

  var self = this;

  var requests = [];
  var currentRequest;
  var parser;

  self._initParser = function () {
    if (!parser) parser = parsers.alloc();
    parser.reinitialize('response');
    parser.socket = self;
    parser.onIncoming = function (res) {
      debug("incoming response!");

      res.addListener('end', function ( ) {
        debug("request complete disconnecting. readyState = " + self.readyState);
        self.end();
      });

      currentRequest.emit("response", res);
    };
  };

  self._reconnect = function () {
    if (self.readyState != "opening") {
      debug("HTTP CLIENT: reconnecting readyState = " + self.readyState);
      self.connect(self.port, self.host);
    }
  };

  self._pushRequest = function (req) {
    req.addListener("flush", function () {
      if (self.readyState == "closed") {
        debug("HTTP CLIENT request flush. reconnect.  readyState = " + self.readyState);
        self._reconnect();
        return;
      }

      debug("self flush  readyState = " + self.readyState);
      if (req == currentRequest) flushMessageQueue(self, [req]);
    });
    requests.push(req);
  };

  self.ondata = function (d, start, end) {
    if (!parser) {
      throw new Error("parser not initialized prior to Client.ondata call");
    }
    var ret = parser.execute(d, start, end - start);
    if (ret instanceof Error) {
      self.destroy(ret);
    } else if (parser.incoming && parser.incoming.upgrade) {
      var bytesParsed = ret;
      var upgradeHead = d.slice(start + bytesParsed, end - start);
      parser.incoming.upgradeHead = upgradeHead;
      currentRequest.emit("response", parser.incoming);
      parser.incoming.emit('end');
      self.ondata = null;
      self.onend = null
    }
  };

  self.addListener("connect", function () {
    if (this.https) {
      this.setSecure(this.credentials);
    } else {
      self._initParser();
      debug('requests: ' + sys.inspect(requests));
      currentRequest = requests.shift()
      currentRequest.flush();
    }
  });

  self.addListener("secure", function () {
    self._initParser();
    debug('requests: ' + sys.inspect(requests));
    currentRequest = requests.shift()
    currentRequest.flush();
  });

  self.onend = function () {
    parser.finish();
    debug("self got end closing. readyState = " + self.readyState);
    self.end();
  };

  self.addListener("close", function (e) {
    if (e) return;

    debug("HTTP CLIENT onClose. readyState = " + self.readyState);

    // If there are more requests to handle, reconnect.
    if (requests.length > 0) {
      self._reconnect();
    } else if (parser) {
      parsers.free(parser);
      parser = null;
    }
  });

};
sys.inherits(Client, net.Stream);

exports.Client = Client;

exports.createClient = function (port, host, https, credentials) {
  var c = new Client;
  c.port = port;
  c.host = host;
  c.https = https;
  c.credentials = credentials;
  return c;
}

Client.prototype.get = function () {
  throw new Error("client.get(...) is now client.request('GET', ...)");
};

Client.prototype.head = function () {
  throw new Error("client.head(...) is now client.request('HEAD', ...)");
};

Client.prototype.post = function () {
  throw new Error("client.post(...) is now client.request('POST', ...)");
};

Client.prototype.del = function () {
  throw new Error("client.del(...) is now client.request('DELETE', ...)");
};

Client.prototype.put = function () {
  throw new Error("client.put(...) is now client.request('PUT', ...)");
};

Client.prototype.request = function (method, url, headers) {
  if (typeof(url) != "string") { // assume method was omitted, shift arguments
    headers = url;
    url = method;
    method = null;
  }
  var req = new ClientRequest(this, method || "GET", url, headers);
  this._pushRequest(req);
  return req;
};


exports.cat = function (url, encoding_, headers_) {
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

  var url = require("url").parse(url);

  var hasHost = false;
  if (headers instanceof Array) {
    for (var i = 0, l = headers.length; i < l; i++) {
      if (headers[i][0].toLowerCase() === 'host') {
        hasHost = true;
        break;
      }
    }
  } else if (typeof headers === "Object") {
    var keys = Object.keys(headers);
    for (var i = 0, l = keys.length; i < l; i++) {
      var key = keys[i];
      if (key.toLowerCase() == 'host') {
        hasHost = true;
        break;
      }
    }
  }
  if (!hasHost) headers["Host"] = url.hostname;

  var content = "";

  var client = exports.createClient(url.port || 80, url.hostname);
  var req = client.request((url.pathname || "/")+(url.search || "")+(url.hash || ""), headers);

  if (url.protocol=="https:") {
      client.https = true;
  }

  var callbackSent = false;

  req.addListener('response', function (res) {
    if (res.statusCode < 200 || res.statusCode >= 300) {
      if (callback && !callbackSent) {
        callback(res.statusCode);
        callbackSent = true;
      }
      client.end();
      return;
    }
    res.setBodyEncoding(encoding);
    res.addListener('data', function (chunk) { content += chunk; });
    res.addListener('end', function () {
      if (callback && !callbackSent) {
        callback(null, content);
        callbackSent = true;
      }
    });
  });

  client.addListener("error", function (err) {
    if (callback && !callbackSent) {
      callback(err);
      callbackSent = true;
    }
  });

  client.addListener("close", function () {
    if (callback && !callbackSent) {
      callback(new Error('Connection closed unexpectedly'));
      callbackSent = true;
    }
  });
  req.end();
};

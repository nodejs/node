var debugLevel = 0;
if ("NODE_DEBUG" in process.env) debugLevel = 1;

function debug (x) {
  if (debugLevel > 0) {
    process.binding('stdio').writeError(x + "\n");
  }
}

var sys = require('sys');
var net = require('net');
var events = require('events');

var HTTPParser = process.binding('http_parser').HTTPParser;

var parserFreeList = [];

function newParser (type) {
  var parser;
  if (parserFreeList.length) {
    parser = parserFreeList.shift();
    parser.reinitialize(type);
  } else {
    parser = new HTTPParser(type);

    parser.onMessageBegin = function () {
      parser.incoming = new IncomingMessage(parser.socket);
      parser.field = null;
      parser.value = null;
    };

    // Only servers will get URL events.
    parser.onURL = function (b, start, len) {
      var slice = b.asciiSlice(start, start+len);
      if (parser.incoming.url) {
        parser.incoming.url += slice;
      } else {
        // Almost always will branch here.
        parser.incoming.url = slice;
      }
    };

    parser.onHeaderField = function (b, start, len) {
      var slice = b.asciiSlice(start, start+len).toLowerCase();
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
      var slice = b.asciiSlice(start, start+len);
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

      if (info.method) {
        // server only
        parser.incoming.method = info.method;
      } else {
        // client only
        parser.incoming.statusCode = info.statusCode;
      }

      parser.onIncoming(parser.incoming, info.shouldKeepAlive);
    };

    parser.onBody = function (b, start, len) {
      // TODO body encoding?
      var enc = parser.incoming._encoding;
      if (!enc) {
        parser.incoming.emit('data', b.slice(start, start+len));
      } else {
        var string;
        switch (enc) {
          case 'utf8':
            string = b.utf8Slice(start, start+len);
            break;
          case 'ascii':
            string = b.asciiSlice(start, start+len);
            break;
          case 'binary':
            string = b.binarySlice(start, start+len);
            break;
          default:
            throw new Error('Unsupported encoding ' + enc + '. Use Buffer');
        }
        parser.incoming.emit('data', string);
      }
    };

    parser.onMessageComplete = function () {
      parser.incoming.emit("end");
    };
  }
  return parser;
}

function freeParser (parser) {
  if (parserFreeList.length < 1000) parserFreeList.push(parser);
}


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

var connection_expression = /Connection/i;
var transfer_encoding_expression = /Transfer-Encoding/i;
var close_expression = /close/i;
var chunk_expression = /chunk/i;
var content_length_expression = /Content-Length/i;


/* Abstract base class for ServerRequest and ClientResponse. */
function IncomingMessage (socket) {
  events.EventEmitter.call(this);

  this.socket = socket;
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

IncomingMessage.prototype.setBodyEncoding = function (enc) {
  // TODO deprecation message?
  this.setEncoding(enc);
};

IncomingMessage.prototype.setEncoding = function (enc) {
  // TODO check values, error out on bad, and deprecation message?
  this._encoding = enc.toLowerCase();
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

  this.socket = socket;

  this.output = [];
  this.outputEncodings = [];

  this.closeOnFinish = false;
  this.chunked_encoding = false;
  this.should_keep_alive = true;
  this.use_chunked_encoding_by_default = true;

  this.flushing = false;
  this.headWritten = false;

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

OutgoingMessage.prototype.sendHeaderLines = function (first_line, headers) {
  var sent_connection_header = false;
  var sent_content_length_header = false;
  var sent_transfer_encoding_header = false;

  // first_line in the case of request is: "GET /index.html HTTP/1.1\r\n"
  // in the case of response it is: "HTTP/1.1 200 OK\r\n"
  var message_header = first_line;
  var field, value;
  for (var i in headers) {
    if (headers[i] instanceof Array) {
      field = headers[i][0];
      value = headers[i][1];
    } else {
      if (!headers.hasOwnProperty(i)) continue;
      field = i;
      value = headers[i];
    }

    message_header += field + ": " + value + CRLF;

    if (connection_expression.test(field)) {
      sent_connection_header = true;
      if (close_expression.test(value)) this.closeOnFinish = true;

    } else if (transfer_encoding_expression.test(field)) {
      sent_transfer_encoding_header = true;
      if (chunk_expression.test(value)) this.chunked_encoding = true;

    } else if (content_length_expression.test(field)) {
      sent_content_length_header = true;

    }
  }

  // keep-alive logic
  if (sent_connection_header == false) {
    if (this.should_keep_alive &&
        (sent_content_length_header || this.use_chunked_encoding_by_default)) {
      message_header += "Connection: keep-alive\r\n";
    } else {
      this.closeOnFinish = true;
      message_header += "Connection: close\r\n";
    }
  }

  if (sent_content_length_header == false && sent_transfer_encoding_header == false) {
    if (this.use_chunked_encoding_by_default) {
      message_header += "Transfer-Encoding: chunked\r\n";
      this.chunked_encoding = true;
    }
    else {
      this.closeOnFinish = true;
    }
  }

  message_header += CRLF;

  this._send(message_header);
  // wait until the first body chunk, or close(), is sent to flush.
};


OutgoingMessage.prototype.sendBody = function () {
  throw new Error("sendBody() has been renamed to write(). " + 
                  "The 'body' event has been renamed to 'data' and " +
                  "the 'complete' event has been renamed to 'end'.");
};


OutgoingMessage.prototype.write = function (chunk, encoding) {
  if ( (this instanceof ServerResponse) && !this.headWritten) {
    throw new Error("writeHead() must be called before write()")
  }

  encoding = encoding || "ascii";
  if (this.chunked_encoding) {
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

OutgoingMessage.prototype.close = function () {
  if (this.chunked_encoding) this._send("0\r\n\r\n"); // last chunk
  this.finished = true;
  this.flush();
};


function ServerResponse (req) {
  OutgoingMessage.call(this, req.socket);

  if (req.httpVersionMajor < 1 || req.httpVersionMinor < 1) {
    this.use_chunked_encoding_by_default = false;
    this.should_keep_alive = false;
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

  var status_line = "HTTP/1.1 " + statusCode.toString() + " "
                  + reasonPhrase + CRLF;
  this.sendHeaderLines(status_line, headers);
  this.headWritten = true;
};

// TODO eventually remove sendHeader(), writeHeader()
ServerResponse.prototype.sendHeader = ServerResponse.prototype.writeHead;
ServerResponse.prototype.writeHeader = ServerResponse.prototype.writeHead;

function ClientRequest (socket, method, url, headers) {
  OutgoingMessage.call(this, socket);

  this.should_keep_alive = false;
  if (method === "GET" || method === "HEAD") {
    this.use_chunked_encoding_by_default = false;
  } else {
    this.use_chunked_encoding_by_default = true;
  }
  this.closeOnFinish = true;

  this.sendHeaderLines(method + " " + url + " HTTP/1.1\r\n", headers);
}
sys.inherits(ClientRequest, OutgoingMessage);
exports.ClientRequest = ClientRequest;

ClientRequest.prototype.finish = function () {
  throw new Error( "finish() has been renamed to close() and no longer takes "
                 + "a response handler as an argument. Manually add a 'response' listener "
                 + "to the request object."
                 );
};

ClientRequest.prototype.close = function () {
  if (arguments.length > 0) {
    throw new Error( "ClientRequest.prototype.close does not take any arguments. "
                   + "Add a response listener manually to the request object."
                   );
  }
  OutgoingMessage.prototype.close.call(this);
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
  this.addListener("request", requestListener);
  this.addListener("connection", connectionListener);
}
sys.inherits(Server, net.Server);

exports.Server = Server;

exports.createServer = function (requestListener) {
  return new Server(requestListener);
};

function connectionListener (socket) {
  var self = this;
  // An array of responses for each socket. In pipelined connections
  // we need to keep track of the order they were sent.
  var responses = [];

  var parser = newParser('request');

  socket.ondata = function (d, start, end) {
    parser.execute(d, start, end - start);
  };

  socket.onend = function () {
    parser.finish();
    // unref the parser for easy gc
    freeParser(parser);

    if (responses.length == 0) {
      socket.close();
    } else {
      responses[responses.length-1].closeOnFinish = true;
    }
  };

  parser.socket = socket;
  // The following callback is issued after the headers have been read on a
  // new message. In this callback we setup the response object and pass it
  // to the user.
  parser.onIncoming = function (incoming, shouldKeepAlive) {
    var req = incoming;
    var res = new ServerResponse(req);

    res.shouldKeepAlive = shouldKeepAlive;
    res.addListener('flush', function () {
      if (flushMessageQueue(socket, responses)) {
        socket.close();
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

  var parser = newParser('response');
  parser.socket = this;

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

  this.ondata = function (d, start, end) {
    parser.execute(d, start, end - start);
  };

  self.addListener("connect", function () {
    parser.reinitialize('response');
    sys.puts('requests: ' + sys.inspect(requests));
    currentRequest = requests.shift()
    currentRequest.flush();
  });

  self.addListener("end", function () {
    parser.finish();
    freeParser(parser);

    debug("self got end closing. readyState = " + self.readyState);
    self.close();
  });

  self.addListener("close", function (had_error) {
    if (had_error) {
      self.emit("error");
      return;
    }

    debug("HTTP CLIENT onClose. readyState = " + self.readyState);

    // If there are more requests to handle, reconnect.
    if (requests.length > 0) {
      self._reconnect();
    }
  });

  parser.onIncoming = function (res) {
    debug("incoming response!");

    res.addListener('end', function ( ) {
      debug("request complete disconnecting. readyState = " + self.readyState);
      self.close();
    });

    currentRequest.emit("response", res);
  };
};
sys.inherits(Client, net.Stream);

exports.Client = Client;

exports.createClient = function (port, host) {
  var c = new Client;
  c.port = port;
  c.host = host;
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
  for (var i in headers) {
    if (i.toLowerCase() === "host") {
      hasHost = true;
      break;
    }
  }
  if (!hasHost) headers["Host"] = url.hostname;

  var content = "";
  
  var client = exports.createClient(url.port || 80, url.hostname);
  var req = client.request((url.pathname || "/")+(url.search || "")+(url.hash || ""), headers);

  req.addListener('response', function (res) {
    if (res.statusCode < 200 || res.statusCode >= 300) {
      if (callback) callback(res.statusCode);
      client.close();
      return;
    }
    res.setBodyEncoding(encoding);
    res.addListener('data', function (chunk) { content += chunk; });
    res.addListener('end', function () {
      if (callback) callback(null, content);
    });
  });

  client.addListener("error", function (err) {
    // todo an error should actually be passed here...
    if (callback) callback(new Error('Connection error'));
  });

  req.close();
};

var sys = require('./sys');
var net = require('./net');
var events = require('events');

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

  this.socket = socket;
  this.httpVersion = null;
  this.headers = {};

  this.method = null;

  // response (client) only
  this.statusCode = null;
}
sys.inherits(IncomingMessage, events.EventEmitter);
exports.IncomingMessage = IncomingMessage;

IncomingMessage.prototype._parseQueryString = function () {
  throw new Error("_parseQueryString is deprecated. Use require(\"querystring\") to parse query strings.\n");
};

IncomingMessage.prototype.setBodyEncoding = function (enc) {
  // TODO: Find a cleaner way of doing this.
  this.socket.setEncoding(enc);
};

IncomingMessage.prototype.pause = function () {
  this.socket.readPause();
};

IncomingMessage.prototype.resume = function () {
  this.socket.readResume();
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

function OutgoingMessage () {
  events.EventEmitter.call(this);

  this.output = [];
  this.outputEncodings = [];

  this.closeOnFinish = false;
  this.chunkEncoding = false;
  this.shouldKeepAlive = true;
  this.useChunkedEncodingByDefault = true;

  this.flushing = false;

  this.finished = false;
}
sys.inherits(OutgoingMessage, events.EventEmitter);
exports.OutgoingMessage = OutgoingMessage;

OutgoingMessage.prototype.send = function (data, encoding) {
  var length = this.output.length;

  if (length === 0) {
    this.output.push(data);
    encoding = encoding || "ascii";
    this.outputEncodings.push(encoding);
    return;
  }

  var lastEncoding = this.outputEncodings[length-1];
  var lastData = this.output[length-1];

  if ((lastEncoding === encoding) ||
      (!encoding && data.constructor === lastData.constructor)) {
    if (lastData.constructor === String) {
      this.output[length-1] = lastData + data;
    } else {
      this.output[length-1] = lastData.concat(data);
    }
    return;
  }

  this.output.push(data);
  encoding = encoding || "ascii";
  this.outputEncodings.push(encoding);
};

OutgoingMessage.prototype.sendHeaderLines = function (first_line, headers) {
  var sentConnectionHeader = false;
  var sendContentLengthHeader = false;
  var sendTransferEncodingHeader = false;

  // first_line in the case of request is: "GET /index.html HTTP/1.1\r\n"
  // in the case of response it is: "HTTP/1.1 200 OK\r\n"
  var messageHeader = first_line;
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

    messageHeader += field + ": " + value + CRLF;

    if (connectionExpression.test(field)) {
      sentConnectionHeader = true;
      if (closeExpression.test(value)) this.closeOnFinish = true;

    } else if (transferEncodingExpression.test(field)) {
      sendTransferEncodingHeader = true;
      if (chunkExpression.test(value)) this.chunkEncoding = true;

    } else if (contentLengthExpression.test(field)) {
      sendContentLengthHeader = true;

    }
  }

  // keep-alive logic
  if (sentConnectionHeader == false) {
    if (this.shouldKeepAlive &&
        (sendContentLengthHeader || this.useChunkedEncodingByDefault)) {
      messageHeader += "Connection: keep-alive\r\n";
    } else {
      this.closeOnFinish = true;
      messageHeader += "Connection: close\r\n";
    }
  }

  if (sendContentLengthHeader == false && sendTransferEncodingHeader == false) {
    if (this.useChunkedEncodingByDefault) {
      messageHeader += "Transfer-Encoding: chunked\r\n";
      this.chunkEncoding = true;
    }
    else {
      this.closeOnFinish = true;
    }
  }

  messageHeader += CRLF;

  this.send(messageHeader);
  // wait until the first body chunk, or finish(), is sent to flush.
};

OutgoingMessage.prototype.sendBody = function (chunk, encoding) {
  encoding = encoding || "ascii";
  if (this.chunkEncoding) {
    this.send(process._byteLength(chunk, encoding).toString(16));
    this.send(CRLF);
    this.send(chunk, encoding);
    this.send(CRLF);
  } else {
    this.send(chunk, encoding);
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
  if (this.chunkEncoding) this.send("0\r\n\r\n"); // last chunk
  this.finished = true;
  this.flush();
};


function ServerResponse (req) {
  OutgoingMessage.call(this);

  if (req.httpVersionMajor < 1 || req.httpVersionMinor < 1) {
    this.useChunkedEncodingByDefault = false;
    this.shouldKeepAlive = false;
  }
}
sys.inherits(ServerResponse, OutgoingMessage);
exports.ServerResponse = ServerResponse;

ServerResponse.prototype.sendHeader = function (statusCode, headers) {
  var reason = STATUS_CODES[statusCode] || "unknown";
  var status_line = "HTTP/1.1 " + statusCode.toString() + " " + reason + CRLF;
  this.sendHeaderLines(status_line, headers);
};


function ClientRequest (method, url, headers) {
  OutgoingMessage.call(this);

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

ClientRequest.prototype.finish = function (responseListener) {
  this.addListener("response", responseListener);
  OutgoingMessage.prototype.finish.call(this);
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

      socket.send(data, encoding);
    }

    if (!message.finished) break;

    message.emit("sent");
    queue.shift();

    if (message.closeOnFinish) return true;
  }
  return false;
}


var parserFreeList = [];

function newParser (type) {
  var parser;
  if (parserFreeList.length) {
    parser = parserFreeList.shift();
    parser.reinitialize(type);
  } else {
    parser = new process.HTTPParser(type);

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
      parser.incoming.emit("data", b.slice(start, start+len));
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

function connectionListener (socket) {
  var self = this;
  var parser = newParser('request');
  // An array of responses for each socket. In pipelined connections
  // we need to keep track of the order they were sent.
  var responses = [];

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


function Server (requestListener, options) {
  net.Server.call(this, connectionListener);
  //server.setOptions(options);
  this.addListener('request', requestListener);
}
process.inherits(Server, net.Server);
exports.Server = Server;
exports.createServer = function (requestListener, options) {
  return new Server(requestListener, options);
};



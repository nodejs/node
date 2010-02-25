var sys = require('sys');
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

var connection_expression = /Connection/i;
var transfer_encoding_expression = /Transfer-Encoding/i;
var close_expression = /close/i;
var chunk_expression = /chunk/i;
var content_length_expression = /Content-Length/i;


/* Abstract base class for ServerRequest and ClientResponse. */
function IncomingMessage (connection) {
  events.EventEmitter.call(this);

  this.connection = connection;
  this.httpVersion = null;
  this.headers = {};

  // request (server) only
  this.url = "";

  this.method = null;

  // response (client) only
  this.statusCode = null;
  this.client = this.connection;
}
sys.inherits(IncomingMessage, events.EventEmitter);
exports.IncomingMessage = IncomingMessage;

IncomingMessage.prototype._parseQueryString = function () {
  throw new Error("_parseQueryString is deprecated. Use require(\"querystring\") to parse query strings.\n");
};

IncomingMessage.prototype.setBodyEncoding = function (enc) {
  // TODO: Find a cleaner way of doing this.
  this.connection.setEncoding(enc);
};

IncomingMessage.prototype.pause = function () {
  this.connection.pause();
};

IncomingMessage.prototype.resume = function () {
  this.connection.resume();
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
  this.chunked_encoding = false;
  this.should_keep_alive = true;
  this.use_chunked_encoding_by_default = true;

  this.flushing = false;

  this.finished = false;
}
sys.inherits(OutgoingMessage, events.EventEmitter);
exports.OutgoingMessage = OutgoingMessage;

OutgoingMessage.prototype._send = function (data, encoding) {
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
  encoding = encoding || "ascii";
  if (this.chunked_encoding) {
    this._send(process._byteLength(chunk, encoding).toString(16));
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
  OutgoingMessage.call(this);

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
};

// TODO eventually remove sendHeader(), writeHeader()
ServerResponse.prototype.sendHeader = ServerResponse.prototype.writeHead;
ServerResponse.prototype.writeHeader = ServerResponse.prototype.writeHead;

function ClientRequest (method, url, headers) {
  OutgoingMessage.call(this);

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


function createIncomingMessageStream (connection, incoming_listener) {
  var stream = new events.EventEmitter();

  stream.addListener("incoming", incoming_listener);

  var incoming, field, value;

  connection.addListener("messageBegin", function () {
    incoming = new IncomingMessage(connection);
    field = null;
    value = null;
  });
  
  // Only servers will get URL events.
  connection.addListener("url", function (data) {
    incoming.url += data;
  });

  connection.addListener("headerField", function (data) {
    if (value) {
      incoming._addHeaderLine(field, value);
      field = null;
      value = null;
    }
    if (field) {
      field += data;
    } else {
      field = data;
    }
  });

  connection.addListener("headerValue", function (data) {
    if (value) {
      value += data;
    } else {
      value = data;
    }
  });

  connection.addListener("headerComplete", function (info) {
    if (field && value) {
      incoming._addHeaderLine(field, value);
    }

    incoming.httpVersion = info.httpVersion;
    incoming.httpVersionMajor = info.versionMajor;
    incoming.httpVersionMinor = info.versionMinor;

    if (info.method) {
      // server only
      incoming.method = info.method;
    } else {
      // client only
      incoming.statusCode = info.statusCode;
    }

    stream.emit("incoming", incoming, info.should_keep_alive);
  });

  connection.addListener("body", function (chunk) {
    incoming.emit('data', chunk);
  });

  connection.addListener("messageComplete", function () {
    incoming.emit('end');
  });

  return stream;
}

/* Returns true if the message queue is finished and the connection
 * should be closed. */
function flushMessageQueue (connection, queue) {
  while (queue[0]) {
    var message = queue[0];

    while (message.output.length > 0) {
      if (connection.readyState !== "open" && connection.readyState !== "writeOnly") {
        return true;
      }

      var data = message.output.shift();
      var encoding = message.outputEncodings.shift();

      connection.write(data, encoding);
    }

    if (!message.finished) break;

    message.emit("sent");
    queue.shift();

    if (message.closeOnFinish) return true;
  }
  return false;
}


exports.createServer = function (requestListener, options) {
  var server = new process.http.Server();
  //server.setOptions(options);
  server.addListener("request", requestListener);
  server.addListener("connection", connectionListener);
  return server;
};

function connectionListener (connection) {
  // An array of responses for each connection. In pipelined connections
  // we need to keep track of the order they were sent.
  var responses = [];

  connection.resetParser();

  // is this really needed?
  connection.addListener("end", function () {
    if (responses.length == 0) {
      connection.close();
    } else {
      responses[responses.length-1].closeOnFinish = true;
    }
  });


  createIncomingMessageStream(connection, function (incoming, should_keep_alive) {
    var req = incoming;

    var res = new ServerResponse(req);
    res.should_keep_alive = should_keep_alive;
    res.addListener("flush", function () {
      if (flushMessageQueue(connection, responses)) {
        connection.close();
      }
    });
    responses.push(res);

    connection.server.emit("request", req, res);
  });
}


exports.createClient = function (port, host) {
  var client = new process.http.Client();
  var secure_credentials={ secure : false };

  var requests = [];
  var currentRequest;

  client._pushRequest = function (req) {
    req.addListener("flush", function () {
      if (client.readyState == "closed") {
        //sys.debug("HTTP CLIENT request flush. reconnect.  readyState = " + client.readyState);
        client.connect(port, host); // reconnect
        if (secure_credentials.secure) {
          client.tcpSetSecure(secure_credentials.format_type,
                              secure_credentials.ca_certs,
                              secure_credentials.crl_list,
                              secure_credentials.private_key,
                              secure_credentials.certificate);
        }
        return;
      }
      //sys.debug("client flush  readyState = " + client.readyState);
      if (req == currentRequest) flushMessageQueue(client, [req]);
    });
    requests.push(req);
  };

  client.tcpSetSecure = client.setSecure;
  client.setSecure = function(format_type, ca_certs, crl_list, private_key, certificate) {
    secure_credentials.secure = true;
    secure_credentials.format_type = format_type;
    secure_credentials.ca_certs = ca_certs;
    secure_credentials.crl_list = crl_list;
    secure_credentials.private_key = private_key;
    secure_credentials.certificate = certificate;
  }

  client.addListener("connect", function () {
    client.resetParser();
    currentRequest = requests.shift();
    currentRequest.flush();
  });

  client.addListener("end", function () {
    //sys.debug("client got end closing. readyState = " + client.readyState);
    client.close();
  });

  client.addListener("close", function (had_error) {
    if (had_error) {
      client.emit("error");
      return;
    }

    //sys.debug("HTTP CLIENT onClose. readyState = " + client.readyState);

    // If there are more requests to handle, reconnect.
    if (requests.length > 0 && client.readyState != "opening") {
      //sys.debug("HTTP CLIENT: reconnecting readyState = " + client.readyState);
      client.connect(port, host); // reconnect
      if (secure_credentials.secure) {
        client.tcpSetSecure(secure_credentials.format_type,
                            secure_credentials.ca_certs,
                            secure_credentials.crl_list,
                            secure_credentials.private_key,
                            secure_credentials.certificate);
      }
    }
  });

  createIncomingMessageStream(client, function (res) {
   //sys.debug("incoming response!");

    res.addListener('end', function ( ) {
      //sys.debug("request complete disconnecting. readyState = " + client.readyState);
      client.close();
    });

    currentRequest.emit("response", res);
  });

  return client;
};

process.http.Client.prototype.get = function () {
  throw new Error("client.get(...) is now client.request('GET', ...)");
};

process.http.Client.prototype.head = function () {
  throw new Error("client.head(...) is now client.request('HEAD', ...)");
};

process.http.Client.prototype.post = function () {
  throw new Error("client.post(...) is now client.request('POST', ...)");
};

process.http.Client.prototype.del = function () {
  throw new Error("client.del(...) is now client.request('DELETE', ...)");
};

process.http.Client.prototype.put = function () {
  throw new Error("client.put(...) is now client.request('PUT', ...)");
};

process.http.Client.prototype.request = function (method, url, headers) {
  if (typeof(url) != "string") { // assume method was omitted, shift arguments
    headers = url;
    url = method;
    method = null;
  }
  var req = new ClientRequest(method || "GET", url, headers);
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

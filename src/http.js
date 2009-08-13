(function () {
CRLF = "\r\n";
node.http.STATUS_CODES = {
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

/*
  parseUri 1.2.1
  (c) 2007 Steven Levithan <stevenlevithan.com>
  MIT License
*/

function decode (s) {
  return decodeURIComponent(s.replace(/\+/g, ' '));
}

node.http.parseUri = function (str) {
  var o   = node.http.parseUri.options,
      m   = o.parser[o.strictMode ? "strict" : "loose"].exec(str),
      uri = {},
      i   = 14;

  while (i--) uri[o.key[i]] = m[i] || "";

  uri[o.q.name] = {};
  uri[o.key[12]].replace(o.q.parser, function ($0, $1, $2) {
    if ($1) {
      var key = decode($1);
      var val = decode($2);
      uri[o.q.name][key] = val;
    }
  });
  uri.toString = function () { return str; };
  
  for (i = o.key.length - 1; i >= 0; i--){
    if (uri[o.key[i]] == "") delete uri[o.key[i]];
  }
  
  return uri;
};

node.http.parseUri.options = {
  strictMode: false,
  key: [ 
    "source",
    "protocol",
    "authority",
    "userInfo",
    "user",
    "password",
    "host",
    "port",
    "relative",
    "path",
    "directory",
    "file",
    "query",
    "anchor"
  ],
  q: {
    name:   "params",
    parser: /(?:^|&)([^&=]*)=?([^&]*)/g
  },
  parser: {
    strict: /^(?:([^:\/?#]+):)?(?:\/\/((?:(([^:@]*):?([^:@]*))?@)?([^:\/?#]*)(?::(\d*))?))?((((?:[^?#\/]*\/)*)([^?#]*))(?:\?([^#]*))?(?:#(.*))?)/,
    loose:  /^(?:(?![^:@]+:[^:@\/]*@)([^:\/?#.]+):)?(?:\/\/)?((?:(([^:@]*):?([^:@]*))?@)?([^:\/?#]*)(?::(\d*))?)(((\/(?:[^?#](?![^?#\/]*\.[^?#\/.]+(?:[?#]|$)))*\/?)?([^?#\/]*))(?:\?([^#]*))?(?:#(.*))?)/
  }
};


var connection_expression = /Connection/i;
var transfer_encoding_expression = /Transfer-Encoding/i;
var close_expression = /close/i;
var chunk_expression = /chunk/i;
var content_length_expression = /Content-Length/i;


/* Abstract base class for ServerRequest and ClientResponse. */
function IncomingMessage (connection) {
  node.EventEmitter.call(this);

  this.connection = connection;
  this.httpVersion = null;
  this.headers = [];

  // request (server) only 
  this.uri = "";
  this.method = null;

  // response (client) only
  this.statusCode = null;
  this.client = this.connection;
}
node.inherits(IncomingMessage, node.EventEmitter);

IncomingMessage.prototype.setBodyEncoding = function (enc) {
  // TODO: Find a cleaner way of doing this. 
  this.connection.setEncoding(enc);
};

IncomingMessage.prototype.pause = function () {
  this.connection.readPause();
};

IncomingMessage.prototype.resume = function () {
  this.connection.readResume();
};


function OutgoingMessage () {
  node.EventEmitter.call(this);

  this.output = [];

  this.closeOnFinish = false;
  this.chunked_encoding = false;
  this.should_keep_alive = true;
  this.use_chunked_encoding_by_default = true;

  this.finished = false;
}
node.inherits(OutgoingMessage, node.EventEmitter);

OutgoingMessage.prototype.send = function (data, encoding) {
  data.encoding = data.constructor === String ? encoding || "ascii" : "raw";
  this.output.push(data);
};

OutgoingMessage.prototype.sendHeaderLines = function (first_line, header_lines) {
  var sent_connection_header = false;
  var sent_content_length_header = false;
  var sent_transfer_encoding_header = false;

  header_lines = header_lines || [];

  // first_line in the case of request is: "GET /index.html HTTP/1.1\r\n"
  // in the case of response it is: "HTTP/1.1 200 OK\r\n"
  var header = first_line;

  for (var i = 0; i < header_lines.length; i++) {
    var field = header_lines[i][0];
    var value = header_lines[i][1];

    header += field + ": " + value + CRLF;
    
    if (connection_expression.exec(field)) {
      sent_connection_header = true;
      if (close_expression.exec(value)) this.closeOnFinish = true;

    } else if (transfer_encoding_expression.exec(field)) {
      sent_transfer_encoding_header = true;
      if (chunk_expression.exec(value)) this.chunked_encoding = true;

    } else if (content_length_expression.exec(field)) {
      sent_content_length_header = true;

    }
  }

  // keep-alive logic 
  if (sent_connection_header == false) {
    if (this.should_keep_alive) {
      header += "Connection: keep-alive\r\n";
    } else {
      this.closeOnFinish = true;
      header += "Connection: close\r\n";
    }
  }

  if (sent_content_length_header == false && sent_transfer_encoding_header == false) {
    if (this.use_chunked_encoding_by_default) {
      header += "Transfer-Encoding: chunked\r\n";
      this.chunked_encoding = true;
    }
  }

  header += CRLF;

  this.send(header);
  // wait until the first body chunk, or finish(), is sent to flush.
};

OutgoingMessage.prototype.sendBody = function (chunk, encoding) {
  if (this.chunked_encoding) {
    this.send(chunk.length.toString(16));
    this.send(CRLF);
    this.send(chunk, encoding);
    this.send(CRLF);
  } else {
    this.send(chunk, encoding);
  }

  this.flush();
};

OutgoingMessage.prototype.flush = function () {
  this.emit("flush");
};

OutgoingMessage.prototype.finish = function () {
  if (this.chunked_encoding) this.send("0\r\n\r\n"); // last chunk
  this.finished = true;
  this.flush();
};


function ServerResponse () {
  OutgoingMessage.call(this);

  this.should_keep_alive = true;
  this.use_chunked_encoding_by_default = true;
}
node.inherits(ServerResponse, OutgoingMessage);

ServerResponse.prototype.sendHeader = function (statusCode, headers) {
  var reason = node.http.STATUS_CODES[statusCode] || "unknown";
  var status_line = "HTTP/1.1 " + statusCode.toString() + " " + reason + CRLF;
  this.sendHeaderLines(status_line, headers);
};


function ClientRequest (method, uri, header_lines) {
  OutgoingMessage.call(this);

  this.should_keep_alive = false;
  if (method === "GET" || method === "HEAD") {
    this.use_chunked_encoding_by_default = false;
  } else {
    this.use_chunked_encoding_by_default = true;
  }
  this.closeOnFinish = true;

  this.sendHeaderLines(method + " " + uri + " HTTP/1.1\r\n", header_lines);
}
node.inherits(ClientRequest, OutgoingMessage);

ClientRequest.prototype.finish = function (responseListener) {
  this.addListener("response", responseListener);
  OutgoingMessage.prototype.finish.call(this);
};


function createIncomingMessageStream (connection, incoming_listener) {
  var stream = new node.EventEmitter();

  stream.addListener("incoming", incoming_listener);

  var incoming;
  var last_header_was_a_value = false;

  connection.addListener("message_begin", function () {
    incoming = new IncomingMessage(connection);
  });

  // Only servers will get URI events.
  connection.addListener("uri", function (data) {
    incoming.uri += data;
  });

  connection.addListener("header_field", function (data) {
    if (incoming.headers.length > 0 && last_header_was_a_value == false) {
      incoming.headers[incoming.headers.length-1][0] += data; 
    } else {
      incoming.headers.push([data]);
    }
    last_header_was_a_value = false;
  });

  connection.addListener("header_value", function (data) {
    var last_pair = incoming.headers[incoming.headers.length-1];
    if (last_pair.length == 1) {
      last_pair[1] = data;
    } else  {
      last_pair[1] += data;
    } 
    last_header_was_a_value = true;
  });

  connection.addListener("headers_complete", function (info) {
    incoming.httpVersion = info.httpVersion;

    if (info.method) {
      // server only
      incoming.method = info.method; 
      incoming.uri = node.http.parseUri(incoming.uri); // TODO parse the URI lazily?
    } else {
      // client only
      incoming.statusCode = info.statusCode; 
    }

    stream.emit("incoming", [incoming, info.should_keep_alive]);
  });

  connection.addListener("body", function (chunk) {
    incoming.emit("body", [chunk]);
  });

  connection.addListener("message_complete", function () {
    incoming.emit("complete");
  });

  return stream;
}

/* Returns true if the message queue is finished and the connection
 * should be closed. */
function flushMessageQueue (connection, queue) {
  if (connection.readyState !== "open" && connection.readyState !== "writeOnly") {
    return false;
  }

  while (queue[0]) {
    var message = queue[0];

    while (message.output.length > 0) {
      var out = message.output.shift();
      connection.send(out, out.encoding);
    }
  
    if (!message.finished) break;

    message.emit("sent");
    queue.shift();

    if (message.closeOnFinish) return true;
  }
  return false;
}


node.http.createServer = function (requestListener, options) {
  var server = new node.http.Server();
  //server.setOptions(options);
  server.addListener("request", requestListener);
  server.addListener("connection", connectionListener);
  return server;
};

function connectionListener (connection) {
  // An array of responses for each connection. In pipelined connections
  // we need to keep track of the order they were sent.
  var responses = [];

  // is this really needed?
  connection.addListener("eof", function () {
    if (responses.length == 0) {
      connection.close();
    } else {
      responses[responses.length-1].closeOnFinish = true;
    }
  });


  createIncomingMessageStream(connection, function (incoming, should_keep_alive) {
    var req = incoming;

    var res = new ServerResponse(connection);
    res.should_keep_alive = should_keep_alive;
    res.addListener("flush", function () {
      if(flushMessageQueue(connection, responses)) {
        connection.fullClose();
      }
    });
    responses.push(res);
    
    connection.server.emit("request", [req, res]);
  });
}


node.http.createClient = function (port, host) {
  var client = new node.http.Client();

  var requests = [];

  client._pushRequest = function (req) {
    req.addListener("flush", function () {
      if (client.readyState == "closed") {
        //node.debug("HTTP CLIENT request flush. reconnect.  readyState = " + client.readyState);
        client.connect(port, host); // reconnect
        return;
      }
      //node.debug("client flush  readyState = " + client.readyState);
      if (req == requests[0]) flushMessageQueue(client, [req]);
    });
    requests.push(req);
  };

  client.addListener("connect", function () {
    requests[0].flush();
  });

  client.addListener("eof", function () {
    //node.debug("client got eof closing. readyState = " + client.readyState);
    client.close();
  });

  client.addListener("disconnect", function (had_error) {
    if (had_error) {
      client.emit("error");
      return;
    }
     
    node.debug("HTTP CLIENT onDisconnect. readyState = " + client.readyState);

    // If there are more requests to handle, reconnect.
    if (requests.length > 0 && client.readyState != "opening") {
      node.debug("HTTP CLIENT: reconnecting readyState = " + client.readyState);
      client.connect(port, host); // reconnect
    }
  });

  createIncomingMessageStream(client, function (res) {
   //node.debug("incoming response!");

    res.addListener("complete", function ( ) {
      //node.debug("request complete disconnecting. readyState = " + client.readyState);
      client.close();
    });

    var req = requests.shift();
    req.emit("response", [res]);
  });

  return client;
};

node.http.Client.prototype.get = function (uri, headers) {
  var req = new ClientRequest("GET", uri, headers);
  this._pushRequest(req);
  return req;
};

node.http.Client.prototype.head = function (uri, headers) {
  var req = new ClientRequest("HEAD", uri, headers);
  this._pushRequest(req);
  return req;
};

node.http.Client.prototype.post = function (uri, headers) {
  var req = new ClientRequest("POST", uri, headers);
  this._pushRequest(req);
  return req;
};

node.http.Client.prototype.del = function (uri, headers) {
  var req = new ClientRequest("DELETE", uri, headers);
  this._pushRequest(req);
  return req;
};

node.http.Client.prototype.put = function (uri, headers) {
  var req = new ClientRequest("PUT", uri, headers);
  this._pushRequest(req);
  return req;
};


node.http.cat = function (url, encoding) {
  var promise = new node.Promise();

  var uri = node.http.parseUri(url);
  var client = node.http.createClient(uri.port || 80, uri.host);
  var req = client.get(uri.path || "/");

  client.addListener("error", function () {
    promise.emitError();
  });

  var content = "";

  req.finish(function (res) {
    if (res.statusCode < 200 || res.statusCode >= 300) {
      promise.emitError([res.statusCode]);
      return;
    }
    res.setBodyEncoding(encoding);
    res.addListener("body", function (chunk) { content += chunk; });
    res.addListener("complete", function () {
      promise.emitSuccess([content]);
    });
  });

  return promise;
};

})(); // anonymous namespace

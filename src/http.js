(function () {

/**
 * Inherit the prototype methods from one constructor into another.
 *
 * The Function.prototype.inherits from lang.js rewritten as a standalone
 * function (not on Function.prototype). NOTE: If this file is to be loaded
 * during bootstrapping this function needs to be revritten using some native
 * functions as prototype setup using normal JavaScript does not work as
 * expected during bootstrapping (see mirror.js in r114903).
 *
 * @param {function} ctor Constructor function which needs to inherit the
 *     prototype
 * @param {function} superCtor Constructor function to inherit prototype from
 */
function inherits(ctor, superCtor) {
  var tempCtor = function(){};
  tempCtor.prototype = superCtor.prototype;
  ctor.super_ = superCtor.prototype;
  ctor.prototype = new tempCtor();
  ctor.prototype.constructor = ctor;
}


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

function toRaw(string) {
  var a = [];
  for (var i = 0; i < string.length; i++)
    a.push(string.charCodeAt(i));
  return a;
}

function send (output, data, encoding) {
  if (data.constructor === String)
    encoding = encoding || "ascii";
  else
    encoding = "raw";

  output.push([data, encoding]);
}

/* This is a wrapper around the LowLevelServer interface. It provides
 * connection handling, overflow checking, and some data buffering.
 */
node.http.createServer = function (requestListener, options) {
  var server = new node.http.LowLevelServer();
  server.addListener("connection", connectionListener);
  server.addListener("request", requestListener);
  //server.setOptions(options);
  return server;
};



/* Abstract base class for ServerRequest and ClientResponse. */
var IncomingMessage = function (connection) {
  node.EventEmitter.call(this);

  this.connection = connection;
  this.httpVersion = null;
  this.headers = [];
  this.last_was_value = false; // TODO: remove me.
};
inherits(IncomingMessage, node.EventEmitter);

IncomingMessage.prototype.setBodyEncoding = function (enc) {
  // TODO: Find a cleaner way of doing this. 
  this.connection.setEncoding(enc);
};

IncomingMessage.prototype._emitBody = function (chunk) {
  this.emit("body", [chunk]);
};

IncomingMessage.prototype._emitComplete = function () {
  this.emit("complete");
};


var ServerRequest = function (connection) {
  IncomingMessage.call(this, connection);

  this.uri = "";
  this.method = null;
};
inherits(ServerRequest, IncomingMessage);


var ClientResponse = function (connection) {
  IncomingMessage.call(this, connection);

  this.statusCode = null;
  this.client = this.connection;
};
inherits(ClientResponse, IncomingMessage);





function connectionListener (connection) {
  // An array of responses for each connection. In pipelined connections
  // we need to keep track of the order they were sent.
  connection.responses = [];

  // is this really needed?
  connection.addListener("eof", function () {
    if (connection.responses.length == 0) {
      connection.close();
    } else {
      connection.responses[connection.responses.length-1].closeOnFinish = true;
    }
  });

  var req, res;

  connection.addListener("message_begin", function () {
    req = new ServerRequest(connection);
    res = new node.http.ServerResponse(connection);
  });

  connection.addListener("uri", function (data) {
    req.uri += data;
  });

  connection.addListener("header_field", function (data) {
    if (req.headers.length > 0 && req.last_was_value == false)
      req.headers[req.headers.length-1][0] += data; 
    else
      req.headers.push([data]);
    req.last_was_value = false;
  });

  connection.addListener("header_value", function (data) {
    var last_pair = req.headers[req.headers.length-1];
    if (last_pair.length == 1)
      last_pair[1] = data;
    else 
      last_pair[1] += data;
    req.last_was_value = true;
  });

  connection.addListener("headers_complete", function (info) {
    req.httpVersion = info.httpVersion;
    req.method = info.method;
    req.uri = node.http.parseUri(req.uri); // TODO parse the URI lazily?

    res.should_keep_alive = info.should_keep_alive;

    connection.server.emit("request", [req, res]);
  });

  connection.addListener("body", function (chunk) {
    req._emitBody(chunk);
  });

  connection.addListener("message_complete", function () {
    req._emitComplete()
  });
}

node.http.ServerResponse = function (connection) {
  var responses = connection.responses;
  responses.push(this);
  this.connection = connection;
  this.closeOnFinish = false;
  var output = [];

  var chunked_encoding = false;

  this.sendHeader = function (statusCode, headers) {
    var sent_connection_header = false;
    var sent_transfer_encoding_header = false;
    var sent_content_length_header = false;

    var reason = node.http.STATUS_CODES[statusCode] || "unknown";
    var header = "HTTP/1.1 " + statusCode.toString() + " " + reason + CRLF;

    for (var i = 0; i < headers.length; i++) {
      var field = headers[i][0];
      var value = headers[i][1];

      header += field + ": " + value + CRLF;
      
      if (connection_expression.exec(field)) {
        sent_connection_header = true;
        if (close_expression.exec(value)) this.closeOnFinish = true;
      } else if (transfer_encoding_expression.exec(field)) {
        sent_transfer_encoding_header = true;
        if (chunk_expression.exec(value)) chunked_encoding = true;
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

    if ( sent_content_length_header == false && sent_transfer_encoding_header == false ) {
      header += "Transfer-Encoding: chunked\r\n";
      chunked_encoding = true;
    }

    header += CRLF;

    send(output, header);
  };

  this.sendBody = function (chunk, encoding) {
    if (chunked_encoding) {
      send(output, chunk.length.toString(16));
      send(output, CRLF);
      send(output, chunk, encoding);
      send(output, CRLF);
    } else {
      send(output, chunk, encoding);
    }

    this.flush();
  };

  this.flush = function () {
    if (connection.readyState === "closed" || connection.readyState === "readOnly")
    {
      responses = [];
      return;
    }
    if (responses.length > 0 && responses[0] === this) {
      while (output.length > 0) {
        var out = output.shift();
        connection.send(out[0], out[1]);
      }
    }
  };

  this.finished = false;
  this.finish = function () {
    if (chunked_encoding) send(output, "0\r\n\r\n"); // last chunk

    this.finished = true;

    while (responses.length > 0 && responses[0].finished) {
      var res = responses[0];
      res.flush();
      if (res.closeOnFinish)
        connection.fullClose();
      responses.shift();
    }
  };
};


node.http.Client = node.http.LowLevelClient; // FIXME

node.http.Client.prototype.flush = function (request) {
  //p(request);
  if (this.readyState == "closed") {
    this.reconnect();
    return;
  }
  //node.debug("HTTP CLIENT flush. readyState = " + connection.readyState);
  while ( request === this.requests[0] && request.output.length > 0 && this.readyState == "open" ) {
    var out = request.output.shift();
    this.send(out[0], out[1]);
  }
};

node.http.createClient = function (port, host) {
  var client = new node.http.Client();

  client.requests = [];

  client.reconnect = function () { return client.connect(port, host); };

  client.addListener("connect", function () {
    //node.debug("HTTP CLIENT onConnect. readyState = " + client.readyState);
    //node.debug("client.requests[0].uri = '" + client.requests[0].uri + "'");
    client.flush(client.requests[0]);
  });

  client.addListener("eof", function () {
    client.close();
  });

  client.addListener("disconnect", function (had_error) {
    if (had_error) {
      client.emit("error");
      return;
    }
     
    //node.debug("HTTP CLIENT onDisconnect. readyState = " + client.readyState);
    // If there are more requests to handle, reconnect.
    if (client.requests.length > 0) {
      //node.debug("HTTP CLIENT: reconnecting");
      client.connect(port, host);
    }
  });

  var req, res;

  client.addListener("message_begin", function () {
    req = client.requests.shift();
    res = new ClientResponse(client);
  });

  client.addListener("header_field", function (data) {
    if (res.headers.length > 0 && res.last_was_value == false)
      res.headers[res.headers.length-1][0] += data; 
    else
      res.headers.push([data]);
    res.last_was_value = false;
  });

  client.addListener("header_value", function (data) {
    var last_pair = res.headers[res.headers.length-1];
    if (last_pair.length == 1) {
      last_pair[1] = data;
    } else {
      last_pair[1] += data;
    }
    res.last_was_value = true;
  });

  client.addListener("headers_complete", function (info) {
    res.statusCode = info.statusCode;
    res.httpVersion = info.httpVersion;

    req.emit("response", [res]);
  });

  client.addListener("body", function (chunk) {
    res._emitBody(chunk);
  });

  client.addListener("message_complete", function () {
    client.close();
    res._emitComplete();
  });

  return client;
};

node.http.Client.prototype.get = function (uri, headers) {
  var req = createClientRequest(this, "GET", uri, headers);
  this.requests.push(req);
  return req;
};

node.http.Client.prototype.head = function (uri, headers) {
  var req = createClientRequest(this, "HEAD", uri, headers);
  this.requests.push(req);
  return req;
};

node.http.Client.prototype.post = function (uri, headers) {
  var req = createClientRequest(this, "POST", uri, headers);
  this.requests.push(req);
  return req;
};

node.http.Client.prototype.del = function (uri, headers) {
  var req = createClientRequest(this, "DELETE", uri, headers);
  this.requests.push(req);
  return req;
};

node.http.Client.prototype.put = function (uri, headers) {
  var req = createClientRequest(this, "PUT", uri, headers);
  this.requests.push(req);
  return req;
};

function createClientRequest (connection, method, uri, header_lines) {
  var req = new node.EventEmitter;

  req.uri = uri;

  var chunked_encoding = false;
  req.closeOnFinish = false;

  var sent_connection_header = false;
  var sent_transfer_encoding_header = false;
  var sent_content_length_header = false;

  var header = method + " " + uri + " HTTP/1.1\r\n";

  header_lines = header_lines || [];
  for (var i = 0; i < header_lines.length; i++) {
    var field = header_lines[i][0];
    var value = header_lines[i][1];

    header += field + ": " + value + CRLF;
    
    if (connection_expression.exec(field)) {
      sent_connection_header = true;
      if (close_expression.exec(value)) req.closeOnFinish = true;
    } else if (transfer_encoding_expression.exec(field)) {
      sent_transfer_encoding_header = true;
      if (chunk_expression.exec(value)) chunked_encoding = true;
    } else if (content_length_expression.exec(field)) {
      sent_content_length_header = true;
    }
  }

  if (sent_connection_header == false) {
    header += "Connection: keep-alive\r\n";
  }

  header += CRLF;
   
  req.output = [];

  send(req.output, header);

  req.sendBody = function (chunk, encoding) {
    if (sent_content_length_header == false && chunked_encoding == false) {
      throw "Content-Length header (or Transfer-Encoding:chunked) not set";
    }

    if (chunked_encoding) {
      send(req.output, chunk.length.toString(16));
      send(req.output, CRLF);
      send(req.output, chunk, encoding);
      send(req.output, CRLF);
    } else {
      send(req.output, chunk, encoding);
    }

    connection.flush(req);
  };

  req.finished = false;

  req.finish = function (responseListener) {
    req.addListener("response", responseListener);

    if (chunked_encoding)
      send(req.output, "0\r\n\r\n"); // last chunk

    connection.flush(req);
  };

  return req;
}

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

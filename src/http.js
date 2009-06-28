(function () {
CRLF = "\r\n";
node.http.STATUS_CODES = { 100 : 'Continue'
                         , 101 : 'Switching Protocols' 
                         , 200 : 'OK' 
                         , 201 : 'Created' 
                         , 202 : 'Accepted' 
                         , 203 : 'Non-Authoritative Information' 
                         , 204 : 'No Content' 
                         , 205 : 'Reset Content' 
                         , 206 : 'Partial Content' 
                         , 300 : 'Multiple Choices' 
                         , 301 : 'Moved Permanently' 
                         , 302 : 'Moved Temporarily' 
                         , 303 : 'See Other' 
                         , 304 : 'Not Modified' 
                         , 305 : 'Use Proxy' 
                         , 400 : 'Bad Request' 
                         , 401 : 'Unauthorized' 
                         , 402 : 'Payment Required' 
                         , 403 : 'Forbidden' 
                         , 404 : 'Not Found' 
                         , 405 : 'Method Not Allowed' 
                         , 406 : 'Not Acceptable' 
                         , 407 : 'Proxy Authentication Required' 
                         , 408 : 'Request Time-out' 
                         , 409 : 'Conflict' 
                         , 410 : 'Gone' 
                         , 411 : 'Length Required' 
                         , 412 : 'Precondition Failed' 
                         , 413 : 'Request Entity Too Large' 
                         , 414 : 'Request-URI Too Large' 
                         , 415 : 'Unsupported Media Type' 
                         , 500 : 'Internal Server Error' 
                         , 501 : 'Not Implemented' 
                         , 502 : 'Bad Gateway' 
                         , 503 : 'Service Unavailable' 
                         , 504 : 'Gateway Time-out' 
                         , 505 : 'HTTP Version not supported'
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
  
  for (var i = o.key.length - 1; i >= 0; i--){
    if (uri[o.key[i]] == "") delete uri[o.key[i]];
  };
  
  return uri;
};

node.http.parseUri.options = {
  strictMode: false,
  key: [ "source"
       , "protocol"
       , "authority"
       , "userInfo"
       , "user"
       , "password"
       , "host"
       , "port"
       , "relative"
       , "path"
       , "directory"
       , "file"
       , "query"
       , "anchor"
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
};

/* This is a wrapper around the LowLevelServer interface. It provides
 * connection handling, overflow checking, and some data buffering.
 */
node.http.createServer = function (requestListener, options) {
  var server = new node.http.LowLevelServer();
  server.addListener("Connection", connectionListener);
  server.addListener("Request", requestListener);
  //server.setOptions(options);
  return server;
};

node.http.createServerRequest = function (connection) {
  var req = new node.EventEmitter;

  req.connection = connection;
  req.method = null;
  req.uri = "";
  req.httpVersion = null;
  req.headers = [];
  req.last_was_value = false;   // used internally XXX remove me

  req.setBodyEncoding = function (enc) {
    connection.setEncoding(enc);
  };

  return req;
};

// ^
// | 
// |  combine these two functions
// | 
// v

createClientResponse = function (connection) {
  var res = new node.EventEmitter;

  res.connection = connection;
  res.statusCode = null;
  res.httpVersion = null;
  res.headers = [];
  res.last_was_value = false;   // used internally XXX remove me

  res.setBodyEncoding = function (enc) {
    connection.setEncoding(enc);
  };

  return res;
};

function connectionListener (connection) {
  // An array of responses for each connection. In pipelined connections
  // we need to keep track of the order they were sent.
  connection.responses = [];

  // is this really needed?
  connection.addListener("EOF", function () {
    if (connection.responses.length == 0) {
      connection.close();
    } else {
      connection.responses[connection.responses.length-1].closeOnFinish = true;
    }
  });

  var req, res;

  connection.addListener("MessageBegin", function () {
    req = new node.http.createServerRequest(connection);
    res = new node.http.ServerResponse(connection);
  });

  connection.addListener("URI", function (data) {
    req.uri += data;
  });

  connection.addListener("HeaderField", function (data) {
    if (req.headers.length > 0 && req.last_was_value == false)
      req.headers[req.headers.length-1][0] += data; 
    else
      req.headers.push([data]);
    req.last_was_value = false;
  });

  connection.addListener("HeaderValue", function (data) {
    var last_pair = req.headers[req.headers.length-1];
    if (last_pair.length == 1)
      last_pair[1] = data;
    else 
      last_pair[1] += data;
    req.last_was_value = true;
  });

  connection.addListener("HeadersComplete", function (info) {
    req.httpVersion = info.httpVersion;
    req.method = info.method;
    req.uri = node.http.parseUri(req.uri); // TODO parse the URI lazily?

    res.should_keep_alive = info.should_keep_alive;

    connection.server.emit("Request", [req, res]);
  });

  connection.addListener("Body", function (chunk) {
    req.emit("Body", [chunk]);
  });

  connection.addListener("MessageComplete", function () {
    req.emit("BodyComplete");
  });
};

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
    var header = "HTTP/1.1 "
               + statusCode.toString() 
               + " " 
               + reason 
               + CRLF
               ;

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

    if ( sent_content_length_header == false 
      && sent_transfer_encoding_header == false
       ) 
    {
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
    if (responses.length > 0 && responses[0] === this)
      while (output.length > 0) {
        var out = output.shift();
        connection.send(out[0], out[1]);
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
  while ( request === this.requests[0]
       && request.output.length > 0
       && this.readyState == "open"
        )
  {
    var out = request.output.shift();
    this.send(out[0], out[1]);
  }
};

node.http.createClient = function (port, host) {
  var client = new node.http.Client();

  client.requests = [];

  client.reconnect = function () { return client.connect(port, host) };

  client.addListener("Connect", function () {
    //node.debug("HTTP CLIENT onConnect. readyState = " + client.readyState);
    //node.debug("client.requests[0].uri = '" + client.requests[0].uri + "'");
    client.flush(client.requests[0]);
  });

  client.addListener("EOF", function () {
    client.close();
  });

  client.addListener("Disconnect", function (had_error) {
    if (had_error) {
      client.emit("Error");
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

  client.addListener("MessageBegin", function () {
    req = client.requests.shift();
    res = createClientResponse(client);
  });

  client.addListener("HeaderField", function (data) {
    if (res.headers.length > 0 && res.last_was_value == false)
      res.headers[res.headers.length-1][0] += data; 
    else
      res.headers.push([data]);
    res.last_was_value = false;
  });

  client.addListener("HeaderValue", function (data) {
    var last_pair = res.headers[res.headers.length-1];
    if (last_pair.length == 1) {
      last_pair[1] = data;
    } else {
      last_pair[1] += data;
    }
    res.last_was_value = true;
  });

  client.addListener("HeadersComplete", function (info) {
    res.statusCode = info.statusCode;
    res.httpVersion = info.httpVersion;

    req.emit("Response", [res]);
  });

  client.addListener("Body", function (chunk) {
    res.emit("Body", [chunk]);
  });

  client.addListener("MessageComplete", function () {
    client.close();
    res.emit("BodyComplete");
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
      return;
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
    req.addListener("Response", responseListener);

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

  client.addListener("Error", function () {
    promise.emitError();
  });

  var content = "";

  req.finish(function (res) {
    if (res.statusCode < 200 || res.statusCode >= 300) {
      promise.emitError([res.statusCode]);
      return;
    }
    res.setBodyEncoding(encoding);
    res.addListener("Body", function (chunk) { content += chunk; });
    res.addListener("BodyComplete", function () {
      promise.emitSuccess([content]);
    });
  });

  return promise;
};

})(); // anonymous namespace

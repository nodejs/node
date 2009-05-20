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

node.http.parseUri = function (str) {
  var o   = node.http.parseUri.options,
    m   = o.parser[o.strictMode ? "strict" : "loose"].exec(str),
    uri = {},
    i   = 14;

  while (i--) uri[o.key[i]] = m[i] || "";

  uri[o.q.name] = {};
  uri[o.key[12]].replace(o.q.parser, function ($0, $1, $2) {
    if ($1) uri[o.q.name][$1] = $2;
  });
  uri.toString = function () { return str; };

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
    name:   "queryKey",
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

node.http.ServerResponse = function (connection, responses) {
  responses.push(this);
  this.connection = connection;
  this.closeOnFinish = false;
  var output = [];

  // The send method appends data onto the output array. The deal is,
  // the data is either an array of integer, representing binary or it
  // is a string in which case it's UTF8 encoded. 
  // Two things to considered:
  // - we should be able to send mixed encodings.
  // - we don't want to call connection.send("smallstring") because that
  //   is wasteful. *I think* its rather faster to concat inside of JS
  // Thus I attempt to concat as much as possible.  
  function send (data) {
    if (connection.readyState === "closed" || connection.readyState === "readOnly")
    {
      responses = [];
      return;
    }

    if (output.length == 0) {
      output.push(data);
      return;
    }

    var li = output.length-1;

    if (data.constructor == String && output[li].constructor == String) {
      output[li] += data;
      return;
    }

    if (data.constructor == Array && output[li].constructor == Array) {
      output[li] = output[li].concat(data);
      return;
    }

    // If the string is small enough, just convert it to binary
    if (data.constructor == String 
        && data.length < 128
        && output[li].constructor == Array) 
    {
      output[li] = output[li].concat(toRaw(data));
      return;
    }

    output.push(data);
  };

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
        if (close_expression.exec(value))
          this.closeOnFinish = true;
      } else if (transfer_encoding_expression.exec(field)) {
        sent_transfer_encoding_header = true;
        if (chunk_expression.exec(value))
          chunked_encoding = true;
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

    send(header);
  };

  this.sendBody = function (chunk) {
    if (chunked_encoding) {
      send(chunk.length.toString(16));
      send(CRLF);
      send(chunk);
      send(CRLF);
    } else {
      send(chunk);
    }

    this.flush();
  };

  this.flush = function () {
    if (responses.length > 0 && responses[0] === this)
      while (output.length > 0) {
        var out = output.shift();
        connection.send(out);
      }
  };

  this.finished = false;
  this.finish = function () {
    if (chunked_encoding)
      send("0\r\n\r\n"); // last chunk

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

/* This is a wrapper around the LowLevelServer interface. It provides
 * connection handling, overflow checking, and some data buffering.
 */
node.http.Server = function (RequestHandler, options) {
  if (!(this instanceof node.http.Server))
    throw Error("Constructor called as a function");
  var server = this;

  function ConnectionHandler (connection) {
    // An array of responses for each connection. In pipelined connections
    // we need to keep track of the order they were sent.
    var responses = [];

    connection.onMessage = function ( ) {
                                            // filled in ...
      var req = { method          : null    // at onHeadersComplete
                , uri             : ""      // at onURI
                , httpVersion    : null    // at onHeadersComplete
                , headers         : []      // at onHeaderField, onHeaderValue
                , onBody          : null    // by user
                , onBodyComplete  : null    // by user
                , setBodyEncoding : function (enc) {
                    connection.setEncoding(enc);
                  }
                }
      var res = new node.http.ServerResponse(connection, responses);

      this.onURI = function (data) {
        req.uri += data;
        return true
      };

      var last_was_value = false;
      var headers = req.headers;

      this.onHeaderField = function (data) {
        if (headers.length > 0 && last_was_value == false)
          headers[headers.length-1][0] += data; 
        else
          headers.push([data]);
        last_was_value = false;
        return true;
      };

      this.onHeaderValue = function (data) {
        var last_pair = headers[headers.length-1];
        if (last_pair.length == 1)
          last_pair[1] = data;
        else 
          last_pair[1] += data;
        last_was_value = true;
        return true;
      };

      this.onHeadersComplete = function () {
        req.httpVersion = this.httpVersion;
        req.method       = this.method;
        req.uri          = node.http.parseUri(req.uri); // TODO parse the URI lazily

        res.should_keep_alive = this.should_keep_alive;

        return RequestHandler.apply(server, [req, res]);
      };

      this.onBody = function (chunk) {
        if (req.onBody)
          return req.onBody(chunk);
        else
          return true;
      };

      this.onBodyComplete = function () {
        if (req.onBodyComplete)
          return req.onBodyComplete(chunk);
        else
          return true;
      };
    };

    // is this really needed?
    connection.onEOF = function () {
      if (responses.length == 0)
        connection.close();
      else
        responses[responses.length-1].closeOnFinish = true;
    };
  }

  this.__proto__.__proto__ =
    new node.http.LowLevelServer(ConnectionHandler, options);
};

node.http.Client = function (port, host) {
  var connection = new node.http.LowLevelClient();
  var requests  = [];

  function ClientRequest (method, uri, header_lines) {

    var chunked_encoding = false;
    this.closeOnFinish = false;

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
        if (close_expression.exec(value))
          this.closeOnFinish = true;
      } else if (transfer_encoding_expression.exec(field)) {
        sent_transfer_encoding_header = true;
        if (chunk_expression.exec(value))
          chunked_encoding = true;
      } else if (content_length_expression.exec(field)) {
        sent_content_length_header = true;
      }
    }

    if (sent_connection_header == false) {
      header += "Connection: keep-alive\r\n";
    }

    if (sent_content_length_header == false && sent_transfer_encoding_header == false) {
      header += "Transfer-Encoding: chunked\r\n";
      chunked_encoding = true;
    }
    header += CRLF;
     
    var output = [header];

    function send (data) {
      if (output.length == 0) {
        output.push(data);
        return;
      }

      var li = output.length-1;

      if (data.constructor == String && output[li].constructor == String) {
        output[li] += data;
        return;
      }

      if (data.constructor == Array && output[li].constructor == Array) {
        output[li] = output[li].concat(data);
        return;
      }

      // If the string is small enough, just convert it to binary
      if (data.constructor == String 
          && data.length < 128
          && output[li].constructor == Array) 
      {
        output[li] = output[li].concat(toRaw(data));
        return;
      }

      output.push(data);
    };

    this.sendBody = function (chunk) {
      if (chunked_encoding) {
        send(chunk.length.toString(16));
        send(CRLF);
        send(chunk);
        send(CRLF);
      } else {
        send(chunk);
      }

      this.flush();
    };

    this.flush = function ( ) {
      if (connection.readyState !== "open") {
        connection.connect(port, host);
        return;
      }
      while (this === requests[0] && output.length > 0) {
        connection.send(output.shift());
      }
    };

    this.finished = false;
    this.finish = function (responseHandler) {
      this.responseHandler = responseHandler;
      if (chunked_encoding)
        send("0\r\n\r\n"); // last chunk

      this.flush();
    };
  }
  
  connection.onConnect = function () {
    //node.debug("HTTP CLIENT: connected");
    requests[0].flush();
  };

  connection.onDisconnect = function () {
    //node.debug("HTTP CLIENT: disconnect");
    // If there are more requests to handle, reconnect.
    if (requests.length > 0) {
      //node.debug("HTTP CLIENT: reconnecting");
      this.connect(port, host);
    }
  };

  // On response
  connection.onMessage = function () {
    var req = requests.shift();
    var res = { statusCode      : null  // set in onHeadersComplete
              , httpVersion     : null  // set in onHeadersComplete
              , headers         : []    // set in onHeaderField/Value
              , setBodyEncoding : function (enc) {
                  connection.setEncoding(enc);
                }
              };

    var headers = res.headers;
    var last_was_value = false;

    this.onHeaderField = function (data) {
      if (headers.length > 0 && last_was_value == false)
        headers[headers.length-1][0] += data; 
      else
        headers.push([data]);
      last_was_value = false;
      return true;
    };

    this.onHeaderValue = function (data) {
      var last_pair = headers[headers.length-1];
      if (last_pair.length == 1)
        last_pair[1] = data;
      else 
        last_pair[1] += data;
      last_was_value = true;
      return true;
    };

    this.onHeadersComplete = function () {
      res.statusCode = this.statusCode;
      res.httpVersion = this.httpVersion;
      res.headers = headers;

      req.responseHandler(res);
    };

    this.onBody = function (chunk) {
      if (res.onBody)
        return res.onBody(chunk);
      else
        return true;
    };

    this.onMessageComplete = function () {
      connection.close();

      if (res.onBodyComplete)
        return res.onBodyComplete();
      else
        return true;
    };
  };

  function newRequest (method, uri, headers) {
    var req = new ClientRequest(method, uri, headers);
    requests.push(req);
    return req;
  }
  
  this.get = function (uri, headers) {
    return newRequest("GET", uri, headers);
  };

  this.head = function (uri, headers) {
    return newRequest("HEAD", uri, headers);
  };

  this.post = function (uri, headers) {
    return newRequest("POST", uri, headers);
  };

  this.del = function (uri, headers) {
    return newRequest("DELETE", uri, headers);
  };

  this.put = function (uri, headers) {
    return newRequest("PUT", uri, headers);
  };
};

})(); // anonymous namespace

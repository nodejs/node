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

var connection_expression = /Connection/i;
var transfer_encoding_expression = /Transfer-Encoding/i;
var close_expression = /close/i;
var chunk_expression = /chunk/i;
var content_length_expression = /Content-Length/i;

/* This is a wrapper around the LowLevelServer interface. It provides
 * connection handling, overflow checking, and some data buffering.
 */
node.http.Server = function (RequestHandler, options) {
  if (!(this instanceof node.http.Server))
    throw Error("Constructor called as a function");

  function ConnectionHandler (connection) {
    // An array of messages for each connection. In pipelined connections
    // we need to keep track of the order they were sent.
    var messages = [];

    function Message () {
      messages.push(this);
      /* This annoying output buisness is necessary for the case that users
       * are writing to messages out of order! HTTP requires that messages
       * are returned in the same order the requests come.
       */

      this.connection = connection;

      var output = [];

      function toRaw(string) {
        var a = [];
        for (var i = 0; i < string.length; i++)
          a.push(string.charCodeAt(i));
        return a;
      }

      // The send method appends data onto the output array. The deal is,
      // the data is either an array of integer, representing binary or it
      // is a string in which case it's UTF8 encoded. 
      // Two things to considered:
      // - we should be able to send mixed encodings.
      // - we don't want to call connection.send("smallstring") because that
      //   is wasteful. *I think* its rather faster to concat inside of JS
      // Thus I attempt to concat as much as possible.  
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

      this.flush = function () {
        if (messages.length > 0 && messages[0] === this)
          while (output.length > 0)
            connection.send(output.shift());
      };

      var chunked_encoding = false;
      var connection_close = false;

      this.sendHeader = function (status_code, headers) {
        var sent_connection_header = false;
        var sent_transfer_encoding_header = false;
        var sent_content_length_header = false;

        var reason = node.http.STATUS_CODES[status_code] || "unknown";
        var header = "HTTP/1.1 "
                   + status_code.toString() 
                   + " " 
                   + reason 
                   + "\r\n"
                   ;

        for (var i = 0; i < headers.length; i++) {
          var field = headers[i][0];
          var value = headers[i][1];

          header += field + ": " + value + "\r\n";
          
          if (connection_expression.exec(field)) {
            sent_connection_header = true;
            if (close_expression.exec(value))
              connection_close = true;
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
            connection_close = true;
            header += "Connection: close\r\n";
          }
        }

        if (sent_content_length_header == false && sent_transfer_encoding_header == false) {
          header += "Transfer-Encoding: chunked\r\n";
          chunked_encoding = true;
        }

        header += "\r\n";

        send(header);
      };

      this.sendBody = function (chunk) {
        if (chunked_encoding) {
          send(chunk.length.toString(16));
          send("\r\n");
          send(chunk);
          send("\r\n");
        } else {
          send(chunk);
        }

        this.flush();
      };

      this.finished = false;
      this.finish = function () {
        if (chunked_encoding)
          send("0\r\n\r\n"); // last chunk

        this.finished = true;

        while (messages.length > 0 && messages[0].finished) {
          var res = messages[0];
          res.flush();
          messages.shift();
        }

        if (messages.length == 0 && connection_close) {
          connection.fullClose();
        }
      };

      // abstract
      this.onBody = function () { return true; }
      this.onBodyComplete = function () { return true; }
    }

    connection.onMessage = function ( ) {
      var msg = new Message();

      msg.uri = "";
      var headers = msg.headers = [];
      
      this.onURI          = function (data) { msg.uri  += data; return true };

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
        msg.http_version = this.http_version;
        msg.method = this.method;
        msg.should_keep_alive = this.should_keep_alive;
        return RequestHandler(msg);
      };

      this.onBody = function (chunk) {
        return msg.onBody(chunk);
      };

      this.onBodyComplete = function () {
        return msg.onBodyComplete(chunk);
      };
    };

    // is this really needed?
    connection.onEOF = function () {
      connection.close();
    };
  }

  this.__proto__.__proto__ = new node.http.LowLevelServer(ConnectionHandler, options);
};

node.http.Client = function (port, host) {
  var port = port;
  var host = host;

  var pending_messages = [];
  function Message (method, uri, header_lines) {
    pending_messages.push(this);

    this.method   = method;
    this.path     = path;
    this.headers  = headers;

    var chunked_encoding = false;
    var connection_close = false;

    var sent_connection_header = false;
    var sent_transfer_encoding_header = false;
    var sent_content_length_header = false;

    var header = method + " " + uri + " HTTP/1.1\r\n";

    for (var i = 0; i < header_lines.length; i++) {
      var field = header_lines[i][0];
      var value = header_lines[i][1];

      header += field + ": " + value + "\r\n";
      
      if (connection_expression.exec(field)) {
        sent_connection_header = true;
        if (close_expression.exec(value))
          connection_close = true;
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
    header += "\r\n";
            
    var output = [header];

    this.sendBody = function (chunk) {
    };

    this.finish = function () {
    };
  }
  
  function spawn_client ( ) {
    var c = new node.http.LowLevelClient();

    c.onConnect = function () {
    };

    // On response
    c.onMessage = function () {
    };
  }
  
  this.get = function (path, headers) {
    var m = new Message("GET", path, headers);
    m.finish();
    return m;
  };

  this.head = function (path, headers) {
    var m = new Message("HEAD", path, headers);
    m.finish();
    return m;
  };

  this.post = function (path, headers) {
    return new Message("POST", path, headers);
  };

  this.del = function (path, headers) {
    return new Message("DELETE", path, headers);
  };

  this.put = function (path, headers) {
    return new Message("PUT", path, headers);
  };
};

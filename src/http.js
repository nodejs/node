/* This is a wrapper around the LowLevelServer interface. It provides
 * connection handling, overflow checking, and some data buffering.
 */
node.http.Server = function (RequestHandler, options) {
  var MAX_FIELD_SIZE = 80*1024;
  function Protocol (connection) {
    function fillField (obj, field, data) {
      obj[field] = (obj[field] || "") + data;
      if (obj[field].length > MAX_FIELD_SIZE) {
        connection.fullClose();
        return false;
      }
      return true;
    }

    var responses = [];
    function Response () {
      responses.push(this);
      /* This annoying output buisness is necessary for the case that users
       * are writing to responses out of order! HTTP requires that responses
       * are returned in the same order the requests come.
       */
      var output = "";
      this.output = output;

      function send (data) {
        if (responses[0] === this) {
          connection.send(data);
        } else {
          output += data;
        }
      };

      this.sendStatus = function (status, reason) {
        output += "HTTP/1.1 " + status.toString() + " " + reason + "\r\n";
      };

      this.sendHeader = function (field, value) {
        output += field + ": " + value.toString() + "\r\n";
      };

      var headersSent = false;

      this.sendBody = function (chunk) {
        if (headersSent === false)  {
          output += "\r\n";
        }
        output += chunk;
        if (responses[0] === this) {
          connection.send(output);
          output = "";
        }
      };

      var finished = false;
      this.finish = function () {
        this.finished = true;
        while (responses.length > 0 && responses[0].finished) {
          var res = responses.shift();
          connection.send(res.output);
        }

        if (responses.length == 0) {
          connection.fullClose();
          // TODO keep-alive logic 
          //  if http/1.0 without "Connection: keep-alive" header - yes
          //  if http/1.1 if the request was not GET or HEAD - yes
          //  if http/1.1 without "Connection: close" header - yes
        }
      };
    }

    this.onMessage = function ( ) {
      var res = new Response();
      var req = new RequestHandler(res);

      this.encoding = req.encoding || "raw";
      
      this.onPath         = function (data) { return fillField(req, "path", data); };
      this.onURI          = function (data) { return fillField(req, "uri", data); };
      this.onQueryString  = function (data) { return fillField(req, "query_string", data); };
      this.onFragment     = function (data) { return fillField(req, "fragment", data); };

      this.onHeaderField = function (data) {
        if (req.hasOwnProperty("headers")) {
          var last_pair = req.headers[req.headers.length-1];
          if (last_pair.length == 1)
            return fillField(last_pair, 0, data);
          else
            req.headers.push([data]);
        } else {
          req.headers = [[data]];
        }
        return true;
      };

      this.onHeaderValue = function (data) {
        var last_pair = req.headers[req.headers.length-1];
        if (last_pair.length == 1)
          last_pair[1] = data;
        else 
          return fillField(last_pair, 1, data);
        return true;
      };

      this.onHeadersComplete = function () {
        req.http_version = this.http_version;
        req.method = this.method;
        return req.onHeadersComplete();
      };

      this.onBody = function (chunk) { return req.onBody(chunk); };

      this.onBodyComplete = function () { return req.onBodyComplete(); };
    };
  }

  var server = new node.http.LowLevelServer(Protocol, options);
  this.listen = function (port, host) { server.listen(port, host); }
  this.close = function () { server.close(); }
};

# HTTP

    Stability: 3 - Stable

To use the HTTP server and client one must `require('http')`.

The HTTP interfaces in Node are designed to support many features
of the protocol which have been traditionally difficult to use.
In particular, large, possibly chunk-encoded, messages. The interface is
careful to never buffer entire requests or responses--the
user is able to stream data.

HTTP message headers are represented by an object like this:

    { 'content-length': '123',
      'content-type': 'text/plain',
      'connection': 'keep-alive',
      'accept': '*/*' }

Keys are lowercased. Values are not modified.

In order to support the full spectrum of possible HTTP applications, Node's
HTTP API is very low-level. It deals with stream handling and message
parsing only. It parses a message into headers and body but it does not
parse the actual headers or the body.


## http.STATUS_CODES

* {Object}

A collection of all the standard HTTP response status codes, and the
short description of each.  For example, `http.STATUS_CODES[404] === 'Not
Found'`.

## http.createServer([requestListener])

Returns a new web server object.

The `requestListener` is a function which is automatically
added to the `'request'` event.

## http.createClient([port], [host])

This function is **deprecated**; please use [http.request()][] instead.
Constructs a new HTTP client. `port` and `host` refer to the server to be
connected to.

## Class: http.Server

This is an [EventEmitter][] with the following events:

### Event: 'request'

`function (request, response) { }`

Emitted each time there is a request. Note that there may be multiple requests
per connection (in the case of keep-alive connections).
 `request` is an instance of `http.ServerRequest` and `response` is
 an instance of `http.ServerResponse`

### Event: 'connection'

`function (socket) { }`

 When a new TCP stream is established. `socket` is an object of type
 `net.Socket`. Usually users will not want to access this event. The
 `socket` can also be accessed at `request.connection`.

### Event: 'close'

`function () { }`

 Emitted when the server closes.

### Event: 'checkContinue'

`function (request, response) { }`

Emitted each time a request with an http Expect: 100-continue is received.
If this event isn't listened for, the server will automatically respond
with a 100 Continue as appropriate.

Handling this event involves calling `response.writeContinue` if the client
should continue to send the request body, or generating an appropriate HTTP
response (e.g., 400 Bad Request) if the client should not continue to send the
request body.

Note that when this event is emitted and handled, the `request` event will
not be emitted.

### Event: 'connect'

`function (request, socket, head) { }`

Emitted each time a client requests a http CONNECT method. If this event isn't
listened for, then clients requesting a CONNECT method will have their
connections closed.

* `request` is the arguments for the http request, as it is in the request
  event.
* `socket` is the network socket between the server and client.
* `head` is an instance of Buffer, the first packet of the tunneling stream,
  this may be empty.

After this event is emitted, the request's socket will not have a `data`
event listener, meaning you will need to bind to it in order to handle data
sent to the server on that socket.

### Event: 'upgrade'

`function (request, socket, head) { }`

Emitted each time a client requests a http upgrade. If this event isn't
listened for, then clients requesting an upgrade will have their connections
closed.

* `request` is the arguments for the http request, as it is in the request
  event.
* `socket` is the network socket between the server and client.
* `head` is an instance of Buffer, the first packet of the upgraded stream,
  this may be empty.

After this event is emitted, the request's socket will not have a `data`
event listener, meaning you will need to bind to it in order to handle data
sent to the server on that socket.

### Event: 'clientError'

`function (exception) { }`

If a client connection emits an 'error' event - it will forwarded here.

### server.listen(port, [hostname], [backlog], [callback])

Begin accepting connections on the specified port and hostname.  If the
hostname is omitted, the server will accept connections directed to any
IPv4 address (`INADDR_ANY`).

To listen to a unix socket, supply a filename instead of port and hostname.

Backlog is the maximum length of the queue of pending connections.
The actual length will be determined by your OS through sysctl settings such as
`tcp_max_syn_backlog` and `somaxconn` on linux. The default value of this
parameter is 511 (not 512).

This function is asynchronous. The last parameter `callback` will be added as
a listener for the ['listening'][] event.  See also [net.Server.listen(port)][].


### server.listen(path, [callback])

Start a UNIX socket server listening for connections on the given `path`.

This function is asynchronous. The last parameter `callback` will be added as
a listener for the ['listening'][] event.  See also [net.Server.listen(path)][].


### server.listen(handle, [callback])

* `handle` {Object}
* `callback` {Function}

The `handle` object can be set to either a server or socket (anything
with an underlying `_handle` member), or a `{fd: <n>}` object.

This will cause the server to accept connections on the specified
handle, but it is presumed that the file descriptor or handle has
already been bound to a port or domain socket.

Listening on a file descriptor is not supported on Windows.

This function is asynchronous. The last parameter `callback` will be added as
a listener for the ['listening'](net.html#event_listening_) event.
See also [net.Server.listen()](net.html#net_server_listen_handle_callback).

### server.close([callback])

Stops the server from accepting new connections.  See [net.Server.close()][].


### server.maxHeadersCount

Limits maximum incoming headers count, equal to 1000 by default. If set to 0 -
no limit will be applied.


## Class: http.ServerRequest

This object is created internally by a HTTP server -- not by
the user -- and passed as the first argument to a `'request'` listener.

The request implements the [Readable Stream][] interface. This is an
[EventEmitter][] with the following events:

### Event: 'data'

`function (chunk) { }`

Emitted when a piece of the message body is received. The chunk is a string if
an encoding has been set with `request.setEncoding()`, otherwise it's a
[Buffer][].

Note that the __data will be lost__ if there is no listener when a
`ServerRequest` emits a `'data'` event.

### Event: 'end'

`function () { }`

Emitted exactly once for each request. After that, no more `'data'` events
will be emitted on the request.

### Event: 'close'

`function () { }`

Indicates that the underlaying connection was terminated before
`response.end()` was called or able to flush.

Just like `'end'`, this event occurs only once per request, and no more `'data'`
events will fire afterwards.

Note: `'close'` can fire after `'end'`, but not vice versa.

### request.method

The request method as a string. Read only. Example:
`'GET'`, `'DELETE'`.


### request.url

Request URL string. This contains only the URL that is
present in the actual HTTP request. If the request is:

    GET /status?name=ryan HTTP/1.1\r\n
    Accept: text/plain\r\n
    \r\n

Then `request.url` will be:

    '/status?name=ryan'

If you would like to parse the URL into its parts, you can use
`require('url').parse(request.url)`.  Example:

    node> require('url').parse('/status?name=ryan')
    { href: '/status?name=ryan',
      search: '?name=ryan',
      query: 'name=ryan',
      pathname: '/status' }

If you would like to extract the params from the query string,
you can use the `require('querystring').parse` function, or pass
`true` as the second argument to `require('url').parse`.  Example:

    node> require('url').parse('/status?name=ryan', true)
    { href: '/status?name=ryan',
      search: '?name=ryan',
      query: { name: 'ryan' },
      pathname: '/status' }



### request.headers

Read only map of header names and values. Header names are lower-cased.
Example:

    // Prints something like:
    //
    // { 'user-agent': 'curl/7.22.0',
    //   host: '127.0.0.1:8000',
    //   accept: '*/*' }
    console.log(request.headers);


### request.trailers

Read only; HTTP trailers (if present). Only populated after the 'end' event.

### request.httpVersion

The HTTP protocol version as a string. Read only. Examples:
`'1.1'`, `'1.0'`.
Also `request.httpVersionMajor` is the first integer and
`request.httpVersionMinor` is the second.


### request.setEncoding([encoding])

Set the encoding for the request body. See [stream.setEncoding()][] for more
information.

### request.pause()

Pauses request from emitting events.  Useful to throttle back an upload.


### request.resume()

Resumes a paused request.

### request.connection

The `net.Socket` object associated with the connection.


With HTTPS support, use request.connection.verifyPeer() and
request.connection.getPeerCertificate() to obtain the client's
authentication details.



## Class: http.ServerResponse

This object is created internally by a HTTP server--not by the user. It is
passed as the second parameter to the `'request'` event.

The response implements the [Writable Stream][] interface. This is an
[EventEmitter][] with the following events:

### Event: 'close'

`function () { }`

Indicates that the underlaying connection was terminated before
`response.end()` was called or able to flush.

### response.writeContinue()

Sends a HTTP/1.1 100 Continue message to the client, indicating that
the request body should be sent. See the ['checkContinue'][] event on `Server`.

### response.writeHead(statusCode, [reasonPhrase], [headers])

Sends a response header to the request. The status code is a 3-digit HTTP
status code, like `404`. The last argument, `headers`, are the response headers.
Optionally one can give a human-readable `reasonPhrase` as the second
argument.

Example:

    var body = 'hello world';
    response.writeHead(200, {
      'Content-Length': body.length,
      'Content-Type': 'text/plain' });

This method must only be called once on a message and it must
be called before `response.end()` is called.

If you call `response.write()` or `response.end()` before calling this, the
implicit/mutable headers will be calculated and call this function for you.

Note: that Content-Length is given in bytes not characters. The above example
works because the string `'hello world'` contains only single byte characters.
If the body contains higher coded characters then `Buffer.byteLength()`
should be used to determine the number of bytes in a given encoding.
And Node does not check whether Content-Length and the length of the body
which has been transmitted are equal or not.

### response.statusCode

When using implicit headers (not calling `response.writeHead()` explicitly), this property
controls the status code that will be sent to the client when the headers get
flushed.

Example:

    response.statusCode = 404;

After response header was sent to the client, this property indicates the
status code which was sent out.

### response.setHeader(name, value)

Sets a single header value for implicit headers.  If this header already exists
in the to-be-sent headers, its value will be replaced.  Use an array of strings
here if you need to send multiple headers with the same name.

Example:

    response.setHeader("Content-Type", "text/html");

or

    response.setHeader("Set-Cookie", ["type=ninja", "language=javascript"]);

### response.sendDate

When true, the Date header will be automatically generated and sent in 
the response if it is not already present in the headers. Defaults to true.

This should only be disabled for testing; HTTP requires the Date header
in responses.

### response.getHeader(name)

Reads out a header that's already been queued but not sent to the client.  Note
that the name is case insensitive.  This can only be called before headers get
implicitly flushed.

Example:

    var contentType = response.getHeader('content-type');

### response.removeHeader(name)

Removes a header that's queued for implicit sending.

Example:

    response.removeHeader("Content-Encoding");


### response.write(chunk, [encoding])

If this method is called and `response.writeHead()` has not been called, it will
switch to implicit header mode and flush the implicit headers.

This sends a chunk of the response body. This method may
be called multiple times to provide successive parts of the body.

`chunk` can be a string or a buffer. If `chunk` is a string,
the second parameter specifies how to encode it into a byte stream.
By default the `encoding` is `'utf8'`.

**Note**: This is the raw HTTP body and has nothing to do with
higher-level multi-part body encodings that may be used.

The first time `response.write()` is called, it will send the buffered
header information and the first body to the client. The second time
`response.write()` is called, Node assumes you're going to be streaming
data, and sends that separately. That is, the response is buffered up to the
first chunk of body.

Returns `true` if the entire data was flushed successfully to the kernel
buffer. Returns `false` if all or part of the data was queued in user memory.
`'drain'` will be emitted when the buffer is again free.

### response.addTrailers(headers)

This method adds HTTP trailing headers (a header but at the end of the
message) to the response.

Trailers will **only** be emitted if chunked encoding is used for the
response; if it is not (e.g., if the request was HTTP/1.0), they will
be silently discarded.

Note that HTTP requires the `Trailer` header to be sent if you intend to
emit trailers, with a list of the header fields in its value. E.g.,

    response.writeHead(200, { 'Content-Type': 'text/plain',
                              'Trailer': 'Content-MD5' });
    response.write(fileData);
    response.addTrailers({'Content-MD5': "7895bf4b8828b55ceaf47747b4bca667"});
    response.end();


### response.end([data], [encoding])

This method signals to the server that all of the response headers and body
have been sent; that server should consider this message complete.
The method, `response.end()`, MUST be called on each
response.

If `data` is specified, it is equivalent to calling `response.write(data, encoding)`
followed by `response.end()`.


## http.request(options, callback)

Node maintains several connections per server to make HTTP requests.
This function allows one to transparently issue requests.

`options` can be an object or a string. If `options` is a string, it is
automatically parsed with [url.parse()][].

Options:

- `host`: A domain name or IP address of the server to issue the request to.
  Defaults to `'localhost'`.
- `hostname`: To support `url.parse()` `hostname` is preferred over `host`
- `port`: Port of remote server. Defaults to 80.
- `localAddress`: Local interface to bind for network connections.
- `socketPath`: Unix Domain Socket (use one of host:port or socketPath)
- `method`: A string specifying the HTTP request method. Defaults to `'GET'`.
- `path`: Request path. Defaults to `'/'`. Should include query string if any.
  E.G. `'/index.html?page=12'`
- `headers`: An object containing request headers.
- `auth`: Basic authentication i.e. `'user:password'` to compute an
  Authorization header.
- `agent`: Controls [Agent][] behavior. When an Agent is used request will
  default to `Connection: keep-alive`. Possible values:
 - `undefined` (default): use [global Agent][] for this host and port.
 - `Agent` object: explicitly use the passed in `Agent`.
 - `false`: opts out of connection pooling with an Agent, defaults request to
   `Connection: close`.

`http.request()` returns an instance of the `http.ClientRequest`
class. The `ClientRequest` instance is a writable stream. If one needs to
upload a file with a POST request, then write to the `ClientRequest` object.

Example:

    var options = {
      host: 'www.google.com',
      port: 80,
      path: '/upload',
      method: 'POST'
    };

    var req = http.request(options, function(res) {
      console.log('STATUS: ' + res.statusCode);
      console.log('HEADERS: ' + JSON.stringify(res.headers));
      res.setEncoding('utf8');
      res.on('data', function (chunk) {
        console.log('BODY: ' + chunk);
      });
    });

    req.on('error', function(e) {
      console.log('problem with request: ' + e.message);
    });

    // write data to request body
    req.write('data\n');
    req.write('data\n');
    req.end();

Note that in the example `req.end()` was called. With `http.request()` one
must always call `req.end()` to signify that you're done with the request -
even if there is no data being written to the request body.

If any error is encountered during the request (be that with DNS resolution,
TCP level errors, or actual HTTP parse errors) an `'error'` event is emitted
on the returned request object.

There are a few special headers that should be noted.

* Sending a 'Connection: keep-alive' will notify Node that the connection to
  the server should be persisted until the next request.

* Sending a 'Content-length' header will disable the default chunked encoding.

* Sending an 'Expect' header will immediately send the request headers.
  Usually, when sending 'Expect: 100-continue', you should both set a timeout
  and listen for the `continue` event. See RFC2616 Section 8.2.3 for more
  information.

* Sending an Authorization header will override using the `auth` option
  to compute basic authentication.

## http.get(options, callback)

Since most requests are GET requests without bodies, Node provides this
convenience method. The only difference between this method and `http.request()`
is that it sets the method to GET and calls `req.end()` automatically.

Example:

    http.get("http://www.google.com/index.html", function(res) {
      console.log("Got response: " + res.statusCode);
    }).on('error', function(e) {
      console.log("Got error: " + e.message);
    });


## Class: http.Agent

In node 0.5.3+ there is a new implementation of the HTTP Agent which is used
for pooling sockets used in HTTP client requests.

Previously, a single agent instance helped pool for a single host+port. The
current implementation now holds sockets for any number of hosts.

The current HTTP Agent also defaults client requests to using
Connection:keep-alive. If no pending HTTP requests are waiting on a socket
to become free the socket is closed. This means that node's pool has the
benefit of keep-alive when under load but still does not require developers
to manually close the HTTP clients using keep-alive.

Sockets are removed from the agent's pool when the socket emits either a
"close" event or a special "agentRemove" event. This means that if you intend
to keep one HTTP request open for a long time and don't want it to stay in the
pool you can do something along the lines of:

    http.get(options, function(res) {
      // Do stuff
    }).on("socket", function (socket) {
      socket.emit("agentRemove");
    });

Alternatively, you could just opt out of pooling entirely using `agent:false`:

    http.get({host:'localhost', port:80, path:'/', agent:false}, function (res) {
      // Do stuff
    })

### agent.maxSockets

By default set to 5. Determines how many concurrent sockets the agent can have 
open per host.

### agent.sockets

An object which contains arrays of sockets currently in use by the Agent. Do not 
modify.

### agent.requests

An object which contains queues of requests that have not yet been assigned to 
sockets. Do not modify.

## http.globalAgent

Global instance of Agent which is used as the default for all http client
requests.


## Class: http.ClientRequest

This object is created internally and returned from `http.request()`.  It
represents an _in-progress_ request whose header has already been queued.  The
header is still mutable using the `setHeader(name, value)`, `getHeader(name)`,
`removeHeader(name)` API.  The actual header will be sent along with the first
data chunk or when closing the connection.

To get the response, add a listener for `'response'` to the request object.
`'response'` will be emitted from the request object when the response
headers have been received.  The `'response'` event is executed with one
argument which is an instance of `http.ClientResponse`.

During the `'response'` event, one can add listeners to the
response object; particularly to listen for the `'data'` event. Note that
the `'response'` event is called before any part of the response body is received,
so there is no need to worry about racing to catch the first part of the
body. As long as a listener for `'data'` is added during the `'response'`
event, the entire body will be caught.


    // Good
    request.on('response', function (response) {
      response.on('data', function (chunk) {
        console.log('BODY: ' + chunk);
      });
    });

    // Bad - misses all or part of the body
    request.on('response', function (response) {
      setTimeout(function () {
        response.on('data', function (chunk) {
          console.log('BODY: ' + chunk);
        });
      }, 10);
    });

Note: Node does not check whether Content-Length and the length of the body
which has been transmitted are equal or not.

The request implements the [Writable Stream][] interface. This is an
[EventEmitter][] with the following events:

### Event 'response'

`function (response) { }`

Emitted when a response is received to this request. This event is emitted only
once. The `response` argument will be an instance of `http.ClientResponse`.

Options:

- `host`: A domain name or IP address of the server to issue the request to.
- `port`: Port of remote server.
- `socketPath`: Unix Domain Socket (use one of host:port or socketPath)

### Event: 'socket'

`function (socket) { }`

Emitted after a socket is assigned to this request.

### Event: 'connect'

`function (response, socket, head) { }`

Emitted each time a server responds to a request with a CONNECT method. If this
event isn't being listened for, clients receiving a CONNECT method will have
their connections closed.

A client server pair that show you how to listen for the `connect` event.

    var http = require('http');
    var net = require('net');
    var url = require('url');

    // Create an HTTP tunneling proxy
    var proxy = http.createServer(function (req, res) {
      res.writeHead(200, {'Content-Type': 'text/plain'});
      res.end('okay');
    });
    proxy.on('connect', function(req, cltSocket, head) {
      // connect to an origin server
      var srvUrl = url.parse('http://' + req.url);
      var srvSocket = net.connect(srvUrl.port, srvUrl.hostname, function() {
        cltSocket.write('HTTP/1.1 200 Connection Established\r\n' +
                        'Proxy-agent: Node-Proxy\r\n' +
                        '\r\n');
        srvSocket.write(head);
        srvSocket.pipe(cltSocket);
        cltSocket.pipe(srvSocket);
      });
    });

    // now that proxy is running
    proxy.listen(1337, '127.0.0.1', function() {

      // make a request to a tunneling proxy
      var options = {
        port: 1337,
        host: '127.0.0.1',
        method: 'CONNECT',
        path: 'www.google.com:80'
      };

      var req = http.request(options);
      req.end();

      req.on('connect', function(res, socket, head) {
        console.log('got connected!');

        // make a request over an HTTP tunnel
        socket.write('GET / HTTP/1.1\r\n' +
                     'Host: www.google.com:80\r\n' +
                     'Connection: close\r\n' +
                     '\r\n');
        socket.on('data', function(chunk) {
          console.log(chunk.toString());
        });
        socket.on('end', function() {
          proxy.close();
        });
      });
    });

### Event: 'upgrade'

`function (response, socket, head) { }`

Emitted each time a server responds to a request with an upgrade. If this
event isn't being listened for, clients receiving an upgrade header will have
their connections closed.

A client server pair that show you how to listen for the `upgrade` event.

    var http = require('http');

    // Create an HTTP server
    var srv = http.createServer(function (req, res) {
      res.writeHead(200, {'Content-Type': 'text/plain'});
      res.end('okay');
    });
    srv.on('upgrade', function(req, socket, head) {
      socket.write('HTTP/1.1 101 Web Socket Protocol Handshake\r\n' +
                   'Upgrade: WebSocket\r\n' +
                   'Connection: Upgrade\r\n' +
                   '\r\n');

      socket.pipe(socket); // echo back
    });

    // now that server is running
    srv.listen(1337, '127.0.0.1', function() {

      // make a request
      var options = {
        port: 1337,
        host: '127.0.0.1',
        headers: {
          'Connection': 'Upgrade',
          'Upgrade': 'websocket'
        }
      };

      var req = http.request(options);
      req.end();

      req.on('upgrade', function(res, socket, upgradeHead) {
        console.log('got upgraded!');
        socket.end();
        process.exit(0);
      });
    });

### Event: 'continue'

`function () { }`

Emitted when the server sends a '100 Continue' HTTP response, usually because
the request contained 'Expect: 100-continue'. This is an instruction that
the client should send the request body.

### request.write(chunk, [encoding])

Sends a chunk of the body.  By calling this method
many times, the user can stream a request body to a
server--in that case it is suggested to use the
`['Transfer-Encoding', 'chunked']` header line when
creating the request.

The `chunk` argument should be a [Buffer][] or a string.

The `encoding` argument is optional and only applies when `chunk` is a string.
Defaults to `'utf8'`.


### request.end([data], [encoding])

Finishes sending the request. If any parts of the body are
unsent, it will flush them to the stream. If the request is
chunked, this will send the terminating `'0\r\n\r\n'`.

If `data` is specified, it is equivalent to calling
`request.write(data, encoding)` followed by `request.end()`.

### request.abort()

Aborts a request.  (New since v0.3.8.)

### request.setTimeout(timeout, [callback])

Once a socket is assigned to this request and is connected
[socket.setTimeout()][] will be called.

### request.setNoDelay([noDelay])

Once a socket is assigned to this request and is connected
[socket.setNoDelay()][] will be called.

### request.setSocketKeepAlive([enable], [initialDelay])

Once a socket is assigned to this request and is connected
[socket.setKeepAlive()][] will be called.

## http.ClientResponse

This object is created when making a request with `http.request()`. It is
passed to the `'response'` event of the request object.

The response implements the [Readable Stream][] interface. This is an
[EventEmitter][] with the following events:


### Event: 'data'

`function (chunk) { }`

Emitted when a piece of the message body is received.

Note that the __data will be lost__ if there is no listener when a
`ClientResponse` emits a `'data'` event.


### Event: 'end'

`function () { }`

Emitted exactly once for each message. No arguments. After
emitted no other events will be emitted on the response.

### Event: 'close'

`function (err) { }`

Indicates that the underlaying connection was terminated before
`end` event was emitted.
See [http.ServerRequest][]'s `'close'` event for more information.

### response.statusCode

The 3-digit HTTP response status code. E.G. `404`.

### response.httpVersion

The HTTP version of the connected-to server. Probably either
`'1.1'` or `'1.0'`.
Also `response.httpVersionMajor` is the first integer and
`response.httpVersionMinor` is the second.

### response.headers

The response headers object.

### response.trailers

The response trailers object. Only populated after the 'end' event.

### response.setEncoding([encoding])

Set the encoding for the response body. See [stream.setEncoding()][] for more
information.

### response.pause()

Pauses response from emitting events.  Useful to throttle back a download.

### response.resume()

Resumes a paused response.

[Agent]: #http_class_http_agent
['checkContinue']: #http_event_checkcontinue
[Buffer]: buffer.html#buffer_buffer
[EventEmitter]: events.html#events_class_events_eventemitter
[global Agent]: #http_http_globalagent
[http.request()]: #http_http_request_options_callback
[http.ServerRequest]: #http_class_http_serverrequest
['listening']: net.html#net_event_listening
[net.Server.close()]: net.html#net_server_close_callback
[net.Server.listen(path)]: net.html#net_server_listen_path_callback
[net.Server.listen(port)]: net.html#net_server_listen_port_host_backlog_callback
[Readable Stream]: stream.html#stream_readable_stream
[socket.setKeepAlive()]: net.html#net_socket_setkeepalive_enable_initialdelay
[socket.setNoDelay()]: net.html#net_socket_setnodelay_nodelay
[socket.setTimeout()]: net.html#net_socket_settimeout_timeout_callback
[stream.setEncoding()]: stream.html#stream_stream_setencoding_encoding
[url.parse()]: url.html#url_url_parse_urlstr_parsequerystring_slashesdenotehost
[Writable Stream]: stream.html#stream_writable_stream

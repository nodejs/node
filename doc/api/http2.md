# HTTP2

> Stability: 1 - Experimental

The `http2` module provides an implementation of the [HTTP/2][] protocol. It
can be accessed using:

```js
const http2 = require('http2');
```

## Core API

The Core API provides a low-level interface designed specifically around
support for HTTP/2 protocol features. It is specifically *not* designed for
compatibility with the existing [HTTP/1][] module API. However,
the [Compatibility API][] is.

The `http2` Core API is much more symmetric between client and server than the
`http` API. For instance, most events, like `error` and `socketError`, can be
emitted either by client-side code or server-side code.

### Server-side example

The following illustrates a simple, plain-text HTTP/2 server using the
Core API:

```js
const http2 = require('http2');
const fs = require('fs');

const server = http2.createSecureServer({
  key: fs.readFileSync('localhost-privkey.pem'),
  cert: fs.readFileSync('localhost-cert.pem')
});
server.on('error', (err) => console.error(err));
server.on('socketError', (err) => console.error(err));

server.on('stream', (stream, headers) => {
  // stream is a Duplex
  stream.respond({
    'content-type': 'text/html',
    ':status': 200
  });
  stream.end('<h1>Hello World</h1>');
});

server.listen(8443);
```

To generate the certificate and key for this example, run:

```bash
openssl req -x509 -newkey rsa:2048 -nodes -sha256 -subj '/CN=localhost' \
  -keyout localhost-privkey.pem -out localhost-cert.pem
```

### Client-side example

The following illustrates an HTTP/2 client:

```js
const http2 = require('http2');
const fs = require('fs');
const client = http2.connect('https://localhost:8443', {
  ca: fs.readFileSync('localhost-cert.pem')
});
client.on('socketError', (err) => console.error(err));
client.on('error', (err) => console.error(err));

const req = client.request({ ':path': '/' });

req.on('response', (headers, flags) => {
  for (const name in headers) {
    console.log(`${name}: ${headers[name]}`);
  }
});

req.setEncoding('utf8');
let data = '';
req.on('data', (chunk) => { data += chunk; });
req.on('end', () => {
  console.log(`\n${data}`);
  client.destroy();
});
req.end();
```

### Class: Http2Session
<!-- YAML
added: v8.4.0
-->

* Extends: {EventEmitter}

Instances of the `http2.Http2Session` class represent an active communications
session between an HTTP/2 client and server. Instances of this class are *not*
intended to be constructed directly by user code.

Each `Http2Session` instance will exhibit slightly different behaviors
depending on whether it is operating as a server or a client. The
`http2session.type` property can be used to determine the mode in which an
`Http2Session` is operating. On the server side, user code should rarely
have occasion to work with the `Http2Session` object directly, with most
actions typically taken through interactions with either the `Http2Server` or
`Http2Stream` objects.

#### Http2Session and Sockets

Every `Http2Session` instance is associated with exactly one [`net.Socket`][] or
[`tls.TLSSocket`][] when it is created. When either the `Socket` or the
`Http2Session` are destroyed, both will be destroyed.

Because the of the specific serialization and processing requirements imposed
by the HTTP/2 protocol, it is not recommended for user code to read data from
or write data to a `Socket` instance bound to a `Http2Session`. Doing so can
put the HTTP/2 session into an indeterminate state causing the session and
the socket to become unusable.

Once a `Socket` has been bound to an `Http2Session`, user code should rely
solely on the API of the `Http2Session`.

#### Event: 'close'
<!-- YAML
added: v8.4.0
-->

The `'close'` event is emitted once the `Http2Session` has been terminated.

#### Event: 'connect'
<!-- YAML
added: v8.4.0
-->

The `'connect'` event is emitted once the `Http2Session` has been successfully
connected to the remote peer and communication may begin.

*Note*: User code will typically not listen for this event directly.

#### Event: 'error'
<!-- YAML
added: v8.4.0
-->

The `'error'` event is emitted when an error occurs during the processing of
an `Http2Session`.

#### Event: 'frameError'
<!-- YAML
added: v8.4.0
-->

The `'frameError'` event is emitted when an error occurs while attempting to
send a frame on the session. If the frame that could not be sent is associated
with a specific `Http2Stream`, an attempt to emit `'frameError'` event on the
`Http2Stream` is made.

When invoked, the handler function will receive three arguments:

* An integer identifying the frame type.
* An integer identifying the error code.
* An integer identifying the stream (or 0 if the frame is not associated with
  a stream).

If the `'frameError'` event is associated with a stream, the stream will be
closed and destroyed immediately following the `'frameError'` event. If the
event is not associated with a stream, the `Http2Session` will be shutdown
immediately following the `'frameError'` event.

#### Event: 'goaway'
<!-- YAML
added: v8.4.0
-->

The `'goaway'` event is emitted when a GOAWAY frame is received. When invoked,
the handler function will receive three arguments:

* `errorCode` {number} The HTTP/2 error code specified in the GOAWAY frame.
* `lastStreamID` {number} The ID of the last stream the remote peer successfully
  processed (or `0` if no ID is specified).
* `opaqueData` {Buffer} If additional opaque data was included in the GOAWAY
  frame, a `Buffer` instance will be passed containing that data.

*Note*: The `Http2Session` instance will be shutdown automatically when the
`'goaway'` event is emitted.

#### Event: 'localSettings'
<!-- YAML
added: v8.4.0
-->

The `'localSettings'` event is emitted when an acknowledgement SETTINGS frame
has been received. When invoked, the handler function will receive a copy of
the local settings.

*Note*: When using `http2session.settings()` to submit new settings, the
modified settings do not take effect until the `'localSettings'` event is
emitted.

```js
session.settings({ enablePush: false });

session.on('localSettings', (settings) => {
  /** use the new settings **/
});
```

#### Event: 'remoteSettings'
<!-- YAML
added: v8.4.0
-->

The `'remoteSettings'` event is emitted when a new SETTINGS frame is received
from the connected peer. When invoked, the handler function will receive a copy
of the remote settings.

```js
session.on('remoteSettings', (settings) => {
  /** use the new settings **/
});
```

#### Event: 'stream'
<!-- YAML
added: v8.4.0
-->

The `'stream'` event is emitted when a new `Http2Stream` is created. When
invoked, the handler function will receive a reference to the `Http2Stream`
object, a [Headers Object][], and numeric flags associated with the creation
of the stream.

```js
const http2 = require('http2');
const {
  HTTP2_HEADER_METHOD,
  HTTP2_HEADER_PATH,
  HTTP2_HEADER_STATUS,
  HTTP2_HEADER_CONTENT_TYPE
} = http2.constants;
session.on('stream', (stream, headers, flags) => {
  const method = headers[HTTP2_HEADER_METHOD];
  const path = headers[HTTP2_HEADER_PATH];
  // ...
  stream.respond({
    [HTTP2_HEADER_STATUS]: 200,
    [HTTP2_HEADER_CONTENT_TYPE]: 'text/plain'
  });
  stream.write('hello ');
  stream.end('world');
});
```

On the server side, user code will typically not listen for this event directly,
and would instead register a handler for the `'stream'` event emitted by the
`net.Server` or `tls.Server` instances returned by `http2.createServer()` and
`http2.createSecureServer()`, respectively, as in the example below:

```js
const http2 = require('http2');

// Create a plain-text HTTP/2 server
const server = http2.createServer();

server.on('stream', (stream, headers) => {
  stream.respond({
    'content-type': 'text/html',
    ':status': 200
  });
  stream.end('<h1>Hello World</h1>');
});

server.listen(80);
```

#### Event: 'socketError'
<!-- YAML
added: v8.4.0
-->

The `'socketError'` event is emitted when an `'error'` is emitted on the
`Socket` instance bound to the `Http2Session`. If this event is not handled,
the `'error'` event will be re-emitted on the `Socket`.

For `ServerHttp2Session` instances, a `'socketError'` event listener is always
registered that will, by default, forward the event on to the owning
`Http2Server` instance if no additional handlers are registered.

#### Event: 'timeout'
<!-- YAML
added: v8.4.0
-->

After the `http2session.setTimeout()` method is used to set the timeout period
for this `Http2Session`, the `'timeout'` event is emitted if there is no
activity on the `Http2Session` after the configured number of milliseconds.

```js
session.setTimeout(2000);
session.on('timeout', () => { /** .. **/ });
```

#### http2session.destroy()
<!-- YAML
added: v8.4.0
-->

* Returns: {undefined}

Immediately terminates the `Http2Session` and the associated `net.Socket` or
`tls.TLSSocket`.

#### http2session.destroyed
<!-- YAML
added: v8.4.0
-->

* Value: {boolean}

Will be `true` if this `Http2Session` instance has been destroyed and must no
longer be used, otherwise `false`.

#### http2session.localSettings
<!-- YAML
added: v8.4.0
-->

* Value: {[Settings Object][]}

A prototype-less object describing the current local settings of this
`Http2Session`. The local settings are local to *this* `Http2Session` instance.

#### http2session.pendingSettingsAck
<!-- YAML
added: v8.4.0
-->

* Value: {boolean}

Indicates whether or not the `Http2Session` is currently waiting for an
acknowledgement for a sent SETTINGS frame. Will be `true` after calling the
`http2session.settings()` method. Will be `false` once all sent SETTINGS
frames have been acknowledged.

#### http2session.remoteSettings
<!-- YAML
added: v8.4.0
-->

* Value: {[Settings Object][]}

A prototype-less object describing the current remote settings of this
`Http2Session`. The remote settings are set by the *connected* HTTP/2 peer.

#### http2session.request(headers[, options])
<!-- YAML
added: v8.4.0
-->

* `headers` {[Headers Object][]}
* `options` {Object}
  * `endStream` {boolean} `true` if the `Http2Stream` *writable* side should
    be closed initially, such as when sending a `GET` request that should not
    expect a payload body.
  * `exclusive` {boolean} When `true` and `parent` identifies a parent Stream,
    the created stream is made the sole direct dependency of the parent, with
    all other existing dependents made a dependent of the newly created stream.
    Defaults to `false`.
  * `parent` {number} Specifies the numeric identifier of a stream the newly
    created stream is dependent on.
  * `weight` {number} Specifies the relative dependency of a stream in relation
    to other streams with the same `parent`. The value is a number between `1`
    and `256` (inclusive).
  * `getTrailers` {Function} Callback function invoked to collect trailer
    headers.

* Returns: {ClientHttp2Stream}

For HTTP/2 Client `Http2Session` instances only, the `http2session.request()`
creates and returns an `Http2Stream` instance that can be used to send an
HTTP/2 request to the connected server.

This method is only available if `http2session.type` is equal to
`http2.constants.NGHTTP2_SESSION_CLIENT`.

```js
const http2 = require('http2');
const clientSession = http2.connect('https://localhost:1234');
const {
  HTTP2_HEADER_PATH,
  HTTP2_HEADER_STATUS
} = http2.constants;

const req = clientSession.request({ [HTTP2_HEADER_PATH]: '/' });
req.on('response', (headers) => {
  console.log(headers[HTTP2_HEADER_STATUS]);
  req.on('data', (chunk) => { /** .. **/ });
  req.on('end', () => { /** .. **/ });
});
```

When set, the `options.getTrailers()` function is called immediately after
queuing the last chunk of payload data to be sent. The callback is passed a
single object (with a `null` prototype) that the listener may used to specify
the trailing header fields to send to the peer.

*Note*: The HTTP/1 specification forbids trailers from containing HTTP/2
"pseudo-header" fields (e.g. `':method'`, `':path'`, etc). An `'error'` event
will be emitted if the `getTrailers` callback attempts to set such header
fields.

#### http2session.rstStream(stream, code)
<!-- YAML
added: v8.4.0
-->

* stream {Http2Stream}
* code {number} Unsigned 32-bit integer identifying the error code. Defaults to
  `http2.constant.NGHTTP2_NO_ERROR` (`0x00`)
* Returns: {undefined}

Sends an `RST_STREAM` frame to the connected HTTP/2 peer, causing the given
`Http2Stream` to be closed on both sides using [error code][] `code`.

#### http2session.setTimeout(msecs, callback)
<!-- YAML
added: v8.4.0
-->

* `msecs` {number}
* `callback` {Function}
* Returns: {undefined}

Used to set a callback function that is called when there is no activity on
the `Http2Session` after `msecs` milliseconds. The given `callback` is
registered as a listener on the `'timeout'` event.

#### http2session.shutdown(options[, callback])
<!-- YAML
added: v8.4.0
-->

* `options` {Object}
  * `graceful` {boolean} `true` to attempt a polite shutdown of the
    `Http2Session`.
  * `errorCode` {number} The HTTP/2 [error code][] to return. Note that this is
    *not* the same thing as an HTTP Response Status Code. Defaults to `0x00`
    (No Error).
  * `lastStreamID` {number} The Stream ID of the last successfully processed
    `Http2Stream` on this `Http2Session`.
  * `opaqueData` {Buffer|Uint8Array} A `Buffer` or `Uint8Array` instance
    containing arbitrary additional data to send to the peer upon disconnection.
    This is used, typically, to provide additional data for debugging failures,
    if necessary.
* `callback` {Function} A callback that is invoked after the session shutdown
  has been completed.
* Returns: {undefined}

Attempts to shutdown this `Http2Session` using HTTP/2 defined procedures.
If specified, the given `callback` function will be invoked once the shutdown
process has completed.

Note that calling `http2session.shutdown()` does *not* destroy the session or
tear down the `Socket` connection. It merely prompts both sessions to begin
preparing to cease activity.

During a "graceful" shutdown, the session will first send a `GOAWAY` frame to
the connected peer identifying the last processed stream as 2<sup>32</sup>-1.
Then, on the next tick of the event loop, a second `GOAWAY` frame identifying
the most recently processed stream identifier is sent. This process allows the
remote peer to begin preparing for the connection to be terminated.

```js
session.shutdown({
  graceful: true,
  opaqueData: Buffer.from('add some debugging data here')
}, () => session.destroy());
```

#### http2session.socket
<!-- YAML
added: v8.4.0
-->

* Value: {net.Socket|tls.TLSSocket}

Returns a Proxy object that acts as a `net.Socket` (or `tls.TLSSocket`) but
limits available methods to ones safe to use with HTTP/2.

`destroy`, `emit`, `end`, `pause`, `read`, `resume`, and `write` will throw
an error with code `ERR_HTTP2_NO_SOCKET_MANIPULATION`. See
[Http2Session and Sockets][] for more information.

`setTimeout` method will be called on this `Http2Session`.

All other interactions will be routed directly to the socket.

#### http2session.state
<!-- YAML
added: v8.4.0
-->

* Value: {Object}
  * `effectiveLocalWindowSize` {number}
  * `effectiveRecvDataLength` {number}
  * `nextStreamID` {number}
  * `localWindowSize` {number}
  * `lastProcStreamID` {number}
  * `remoteWindowSize` {number}
  * `outboundQueueSize` {number}
  * `deflateDynamicTableSize` {number}
  * `inflateDynamicTableSize` {number}

An object describing the current status of this `Http2Session`.

#### http2session.priority(stream, options)
<!-- YAML
added: v8.4.0
-->

* `stream` {Http2Stream}
* `options` {Object}
  * `exclusive` {boolean} When `true` and `parent` identifies a parent Stream,
    the given stream is made the sole direct dependency of the parent, with
    all other existing dependents made a dependent of the given stream. Defaults
    to `false`.
  * `parent` {number} Specifies the numeric identifier of a stream the given
    stream is dependent on.
  * `weight` {number} Specifies the relative dependency of a stream in relation
    to other streams with the same `parent`. The value is a number between `1`
    and `256` (inclusive).
  * `silent` {boolean} When `true`, changes the priority locally without
    sending a `PRIORITY` frame to the connected peer.
* Returns: {undefined}

Updates the priority for the given `Http2Stream` instance.

#### http2session.settings(settings)
<!-- YAML
added: v8.4.0
-->

* `settings` {[Settings Object][]}
* Returns {undefined}

Updates the current local settings for this `Http2Session` and sends a new
`SETTINGS` frame to the connected HTTP/2 peer.

Once called, the `http2session.pendingSettingsAck` property will be `true`
while the session is waiting for the remote peer to acknowledge the new
settings.

*Note*: The new settings will not become effective until the SETTINGS
acknowledgement is received and the `'localSettings'` event is emitted. It
is possible to send multiple SETTINGS frames while acknowledgement is still
pending.

#### http2session.type
<!-- YAML
added: v8.4.0
-->

* Value: {number}

The `http2session.type` will be equal to
`http2.constants.NGHTTP2_SESSION_SERVER` if this `Http2Session` instance is a
server, and `http2.constants.NGHTTP2_SESSION_CLIENT` if the instance is a
client.

### Class: Http2Stream
<!-- YAML
added: v8.4.0
-->

* Extends: {Duplex}

Each instance of the `Http2Stream` class represents a bidirectional HTTP/2
communications stream over an `Http2Session` instance. Any single `Http2Session`
may have up to 2<sup>31</sup>-1 `Http2Stream` instances over its lifetime.

User code will not construct `Http2Stream` instances directly. Rather, these
are created, managed, and provided to user code through the `Http2Session`
instance. On the server, `Http2Stream` instances are created either in response
to an incoming HTTP request (and handed off to user code via the `'stream'`
event), or in response to a call to the `http2stream.pushStream()` method.
On the client, `Http2Stream` instances are created and returned when either the
`http2session.request()` method is called, or in response to an incoming
`'push'` event.

*Note*: The `Http2Stream` class is a base for the [`ServerHttp2Stream`][] and
[`ClientHttp2Stream`][] classes, each of which are used specifically by either
the Server or Client side, respectively.

All `Http2Stream` instances are [`Duplex`][] streams. The `Writable` side of the
`Duplex` is used to send data to the connected peer, while the `Readable` side
is used to receive data sent by the connected peer.

#### Http2Stream Lifecycle

##### Creation

On the server side, instances of [`ServerHttp2Stream`][] are created either
when:

* A new HTTP/2 `HEADERS` frame with a previously unused stream ID is received;
* The `http2stream.pushStream()` method is called.

On the client side, instances of [`ClientHttp2Stream`][] are created when the
`http2session.request()` method is called.

*Note*: On the client, the `Http2Stream` instance returned by
`http2session.request()` may not be immediately ready for use if the parent
`Http2Session` has not yet been fully established. In such cases, operations
called on the `Http2Stream` will be buffered until the `'ready'` event is
emitted. User code should rarely, if ever, have need to handle the `'ready'`
event directly. The ready status of an `Http2Stream` can be determined by
checking the value of `http2stream.id`. If the value is `undefined`, the stream
is not yet ready for use.

##### Destruction

All [`Http2Stream`][] instances are destroyed either when:

* An `RST_STREAM` frame for the stream is received by the connected peer.
* The `http2stream.rstStream()` or `http2session.rstStream()` methods are
  called.
* The `http2stream.destroy()` or `http2session.destroy()` methods are called.

When an `Http2Stream` instance is destroyed, an attempt will be made to send an
`RST_STREAM` frame will be sent to the connected peer.

Once the `Http2Stream` instance is destroyed, the `'streamClosed'` event will
be emitted. Because `Http2Stream` is an instance of `stream.Duplex`, the
`'end'` event will also be emitted if the stream data is currently flowing.
The `'error'` event may also be emitted if `http2stream.destroy()` was called
with an `Error` passed as the first argument.

After the `Http2Stream` has been destroyed, the `http2stream.destroyed`
property will be `true` and the `http2stream.rstCode` property will specify the
`RST_STREAM` error code. The `Http2Stream` instance is no longer usable once
destroyed.

#### Event: 'aborted'
<!-- YAML
added: v8.4.0
-->

The `'aborted'` event is emitted whenever a `Http2Stream` instance is
abnormally aborted in mid-communication.

*Note*: The `'aborted'` event will only be emitted if the `Http2Stream`
writable side has not been ended.

#### Event: 'error'
<!-- YAML
added: v8.4.0
-->

The `'error'` event is emitted when an error occurs during the processing of
an `Http2Stream`.

#### Event: 'frameError'
<!-- YAML
added: v8.4.0
-->

The `'frameError'` event is emitted when an error occurs while attempting to
send a frame. When invoked, the handler function will receive an integer
argument identifying the frame type, and an integer argument identifying the
error code. The `Http2Stream` instance will be destroyed immediately after the
`'frameError'` event is emitted.

#### Event: 'streamClosed'
<!-- YAML
added: v8.4.0
-->

The `'streamClosed'` event is emitted when the `Http2Stream` is destroyed. Once
this event is emitted, the `Http2Stream` instance is no longer usable.

The listener callback is passed a single argument specifying the HTTP/2 error
code specified when closing the stream. If the code is any value other than
`NGHTTP2_NO_ERROR` (`0`), an `'error'` event will also be emitted.

#### Event: 'timeout'
<!-- YAML
added: v8.4.0
-->

The `'timeout'` event is emitted after no activity is received for this
`'Http2Stream'` within the number of millseconds set using
`http2stream.setTimeout()`.

#### Event: 'trailers'
<!-- YAML
added: v8.4.0
-->

The `'trailers'` event is emitted when a block of headers associated with
trailing header fields is received. The listener callback is passed the
[Headers Object][] and flags associated with the headers.

```js
stream.on('trailers', (headers, flags) => {
  console.log(headers);
});
```

#### http2stream.aborted
<!-- YAML
added: v8.4.0
-->

* Value: {boolean}

Set to `true` if the `Http2Stream` instance was aborted abnormally. When set,
the `'aborted'` event will have been emitted.

#### http2stream.destroyed
<!-- YAML
added: v8.4.0
-->

* Value: {boolean}

Set to `true` if the `Http2Stream` instance has been destroyed and is no longer
usable.

#### http2stream.priority(options)
<!-- YAML
added: v8.4.0
-->

* `options` {Object}
  * `exclusive` {boolean} When `true` and `parent` identifies a parent Stream,
    this stream is made the sole direct dependency of the parent, with
    all other existing dependents made a dependent of this stream. Defaults
    to `false`.
  * `parent` {number} Specifies the numeric identifier of a stream this stream
    is dependent on.
  * `weight` {number} Specifies the relative dependency of a stream in relation
    to other streams with the same `parent`. The value is a number between `1`
    and `256` (inclusive).
  * `silent` {boolean} When `true`, changes the priority locally without
    sending a `PRIORITY` frame to the connected peer.
* Returns: {undefined}

Updates the priority for this `Http2Stream` instance.

#### http2stream.rstCode
<!-- YAML
added: v8.4.0
-->

* Value: {number}

Set to the `RST_STREAM` [error code][] reported when the `Http2Stream` is
destroyed after either receiving an `RST_STREAM` frame from the connected peer,
calling `http2stream.rstStream()`, or `http2stream.destroy()`. Will be
`undefined` if the `Http2Stream` has not been closed.

#### http2stream.rstStream(code)
<!-- YAML
added: v8.4.0
-->

* code {number} Unsigned 32-bit integer identifying the error code. Defaults to
  `http2.constant.NGHTTP2_NO_ERROR` (`0x00`)
* Returns: {undefined}

Sends an `RST_STREAM` frame to the connected HTTP/2 peer, causing this
`Http2Stream` to be closed on both sides using [error code][] `code`.

#### http2stream.rstWithNoError()
<!-- YAML
added: v8.4.0
-->

* Returns: {undefined}

Shortcut for `http2stream.rstStream()` using error code `0x00` (No Error).

#### http2stream.rstWithProtocolError()
<!-- YAML
added: v8.4.0
-->

* Returns: {undefined}

Shortcut for `http2stream.rstStream()` using error code `0x01` (Protocol Error).

#### http2stream.rstWithCancel()
<!-- YAML
added: v8.4.0
-->

* Returns: {undefined}

Shortcut for `http2stream.rstStream()` using error code `0x08` (Cancel).

#### http2stream.rstWithRefuse()
<!-- YAML
added: v8.4.0
-->

* Returns: {undefined}

Shortcut for `http2stream.rstStream()` using error code `0x07` (Refused Stream).

#### http2stream.rstWithInternalError()
<!-- YAML
added: v8.4.0
-->

* Returns: {undefined}

Shortcut for `http2stream.rstStream()` using error code `0x02` (Internal Error).

#### http2stream.session
<!-- YAML
added: v8.4.0
-->

* Value: {Http2Sesssion}

A reference to the `Http2Session` instance that owns this `Http2Stream`. The
value will be `undefined` after the `Http2Stream` instance is destroyed.

#### http2stream.setTimeout(msecs, callback)
<!-- YAML
added: v8.4.0
-->

* `msecs` {number}
* `callback` {Function}
* Returns: {undefined}

```js
const http2 = require('http2');
const client = http2.connect('http://example.org:8000');

const req = client.request({ ':path': '/' });

// Cancel the stream if there's no activity after 5 seconds
req.setTimeout(5000, () => req.rstWithCancel());
```

#### http2stream.state
<!-- YAML
added: v8.4.0
-->

* Value: {Object}
  * `localWindowSize` {number}
  * `state` {number}
  * `localClose` {number}
  * `remoteClose` {number}
  * `sumDependencyWeight` {number}
  * `weight` {number}

A current state of this `Http2Stream`.

### Class: ClientHttp2Stream
<!-- YAML
added: v8.4.0
-->

* Extends {Http2Stream}

The `ClientHttp2Stream` class is an extension of `Http2Stream` that is
used exclusively on HTTP/2 Clients. `Http2Stream` instances on the client
provide events such as `'response'` and `'push'` that are only relevant on
the client.

#### Event: 'continue'
<!-- YAML
added: v8.5.0
-->

Emitted when the server sends a `100 Continue` status, usually because
the request contained `Expect: 100-continue`. This is an instruction that
the client should send the request body.

#### Event: 'headers'
<!-- YAML
added: v8.4.0
-->

The `'headers'` event is emitted when an additional block of headers is received
for a stream, such as when a block of `1xx` informational headers are received.
The listener callback is passed the [Headers Object][] and flags associated with
the headers.

```js
stream.on('headers', (headers, flags) => {
  console.log(headers);
});
```

#### Event: 'push'
<!-- YAML
added: v8.4.0
-->

The `'push'` event is emitted when response headers for a Server Push stream
are received. The listener callback is passed the [Headers Object][] and flags
associated with the headers.

```js
stream.on('push', (headers, flags) => {
  console.log(headers);
});
```

#### Event: 'response'
<!-- YAML
added: v8.4.0
-->

The `'response'` event is emitted when a response `HEADERS` frame has been
received for this stream from the connected HTTP/2 server. The listener is
invoked with two arguments: an Object containing the received
[Headers Object][], and flags associated with the headers.

For example:

```js
const http2 = require('http2');
const client = http2.connect('https://localhost');
const req = client.request({ ':path': '/' });
req.on('response', (headers, flags) => {
  console.log(headers[':status']);
});
```

### Class: ServerHttp2Stream
<!-- YAML
added: v8.4.0
-->

* Extends: {Http2Stream}

The `ServerHttp2Stream` class is an extension of [`Http2Stream`][] that is
used exclusively on HTTP/2 Servers. `Http2Stream` instances on the server
provide additional methods such as `http2stream.pushStream()` and
`http2stream.respond()` that are only relevant on the server.

#### http2stream.additionalHeaders(headers)
<!-- YAML
added: v8.4.0
-->

* `headers` {[Headers Object][]}
* Returns: {undefined}

Sends an additional informational `HEADERS` frame to the connected HTTP/2 peer.

#### http2stream.headersSent
<!-- YAML
added: v8.4.0
-->

* Value: {boolean}

Boolean (read-only). True if headers were sent, false otherwise.

#### http2stream.pushAllowed
<!-- YAML
added: v8.4.0
-->

* Value: {boolean}

Read-only property mapped to the `SETTINGS_ENABLE_PUSH` flag of the remote
client's most recent `SETTINGS` frame. Will be `true` if the remote peer
accepts push streams, `false` otherwise. Settings are the same for every
`Http2Stream` in the same `Http2Session`.

#### http2stream.pushStream(headers[, options], callback)
<!-- YAML
added: v8.4.0
-->

* `headers` {[Headers Object][]}
* `options` {Object}
  * `exclusive` {boolean} When `true` and `parent` identifies a parent Stream,
    the created stream is made the sole direct dependency of the parent, with
    all other existing dependents made a dependent of the newly created stream.
    Defaults to `false`.
  * `parent` {number} Specifies the numeric identifier of a stream the newly
    created stream is dependent on.
* `callback` {Function} Callback that is called once the push stream has been
  initiated.
* Returns: {undefined}

Initiates a push stream. The callback is invoked with the new `Http2Stream`
instance created for the push stream.

```js
const http2 = require('http2');
const server = http2.createServer();
server.on('stream', (stream) => {
  stream.respond({ ':status': 200 });
  stream.pushStream({ ':path': '/' }, (pushStream) => {
    pushStream.respond({ ':status': 200 });
    pushStream.end('some pushed data');
  });
  stream.end('some data');
});
```

Setting the weight of a push stream is not allowed in the `HEADERS` frame. Pass
a `weight` value to `http2stream.priority` with the `silent` option set to
`true` to enable server-side bandwidth balancing between concurrent streams.

#### http2stream.respond([headers[, options]])
<!-- YAML
added: v8.4.0
-->

* `headers` {[Headers Object][]}
* `options` {Object}
  * `endStream` {boolean} Set to `true` to indicate that the response will not
    include payload data.
  * `getTrailers` {function} Callback function invoked to collect trailer
    headers.
* Returns: {undefined}

```js
const http2 = require('http2');
const server = http2.createServer();
server.on('stream', (stream) => {
  stream.respond({ ':status': 200 });
  stream.end('some data');
});
```

When set, the `options.getTrailers()` function is called immediately after
queuing the last chunk of payload data to be sent. The callback is passed a
single object (with a `null` prototype) that the listener may used to specify
the trailing header fields to send to the peer.

```js
const http2 = require('http2');
const server = http2.createServer();
server.on('stream', (stream) => {
  stream.respond({ ':status': 200 }, {
    getTrailers(trailers) {
      trailers['ABC'] = 'some value to send';
    }
  });
  stream.end('some data');
});
```

*Note*: The HTTP/1 specification forbids trailers from containing HTTP/2
"pseudo-header" fields (e.g. `':status'`, `':path'`, etc). An `'error'` event
will be emitted if the `getTrailers` callback attempts to set such header
fields.

#### http2stream.respondWithFD(fd[, headers[, options]])
<!-- YAML
added: v8.4.0
-->

* `fd` {number} A readable file descriptor
* `headers` {[Headers Object][]}
* `options` {Object}
  * `statCheck` {Function}
  * `getTrailers` {Function} Callback function invoked to collect trailer
    headers.
  * `offset` {number} The offset position at which to begin reading
  * `length` {number} The amount of data from the fd to send

Initiates a response whose data is read from the given file descriptor. No
validation is performed on the given file descriptor. If an error occurs while
attempting to read data using the file descriptor, the `Http2Stream` will be
closed using an `RST_STREAM` frame using the standard `INTERNAL_ERROR` code.

When used, the `Http2Stream` object's Duplex interface will be closed
automatically.

```js
const http2 = require('http2');
const fs = require('fs');

const fd = fs.openSync('/some/file', 'r');

const server = http2.createServer();
server.on('stream', (stream) => {
  const stat = fs.fstatSync(fd);
  const headers = {
    'content-length': stat.size,
    'last-modified': stat.mtime.toUTCString(),
    'content-type': 'text/plain'
  };
  stream.respondWithFD(fd, headers);
});
server.on('close', () => fs.closeSync(fd));
```

The optional `options.statCheck` function may be specified to give user code
an opportunity to set additional content headers based on the `fs.Stat` details
of the given fd. If the `statCheck` function is provided, the
`http2stream.respondWithFD()` method will perform an `fs.fstat()` call to
collect details on the provided file descriptor.

The `offset` and `length` options may be used to limit the response to a
specific range subset. This can be used, for instance, to support HTTP Range
requests.

When set, the `options.getTrailers()` function is called immediately after
queuing the last chunk of payload data to be sent. The callback is passed a
single object (with a `null` prototype) that the listener may used to specify
the trailing header fields to send to the peer.

```js
const http2 = require('http2');
const fs = require('fs');

const fd = fs.openSync('/some/file', 'r');

const server = http2.createServer();
server.on('stream', (stream) => {
  const stat = fs.fstatSync(fd);
  const headers = {
    'content-length': stat.size,
    'last-modified': stat.mtime.toUTCString(),
    'content-type': 'text/plain'
  };
  stream.respondWithFD(fd, headers, {
    getTrailers(trailers) {
      trailers['ABC'] = 'some value to send';
    }
  });
});
server.on('close', () => fs.closeSync(fd));
```

*Note*: The HTTP/1 specification forbids trailers from containing HTTP/2
"pseudo-header" fields (e.g. `':status'`, `':path'`, etc). An `'error'` event
will be emitted if the `getTrailers` callback attempts to set such header
fields.

#### http2stream.respondWithFile(path[, headers[, options]])
<!-- YAML
added: v8.4.0
-->

* `path` {string|Buffer|URL}
* `headers` {[Headers Object][]}
* `options` {Object}
  * `statCheck` {Function}
  * `onError` {Function} Callback function invoked in the case of an
    Error before send
  * `getTrailers` {Function} Callback function invoked to collect trailer
    headers.
  * `offset` {number} The offset position at which to begin reading
  * `length` {number} The amount of data from the fd to send

Sends a regular file as the response. The `path` must specify a regular file
or an `'error'` event will be emitted on the `Http2Stream` object.

When used, the `Http2Stream` object's Duplex interface will be closed
automatically.

The optional `options.statCheck` function may be specified to give user code
an opportunity to set additional content headers based on the `fs.Stat` details
of the given file:

If an error occurs while attempting to read the file data, the `Http2Stream`
will be closed using an `RST_STREAM` frame using the standard `INTERNAL_ERROR`
code. If the `onError` callback is defined it will be called, otherwise
the stream will be destroyed.

Example using a file path:

```js
const http2 = require('http2');
const server = http2.createServer();
server.on('stream', (stream) => {
  function statCheck(stat, headers) {
    headers['last-modified'] = stat.mtime.toUTCString();
  }

  function onError(err) {
    if (err.code === 'ENOENT') {
      stream.respond({ ':status': 404 });
    } else {
      stream.respond({ ':status': 500 });
    }
    stream.end();
  }

  stream.respondWithFile('/some/file',
                         { 'content-type': 'text/plain' },
                         { statCheck, onError });
});
```

The `options.statCheck` function may also be used to cancel the send operation
by returning `false`. For instance, a conditional request may check the stat
results to determine if the file has been modified to return an appropriate
`304` response:

```js
const http2 = require('http2');
const server = http2.createServer();
server.on('stream', (stream) => {
  function statCheck(stat, headers) {
    // Check the stat here...
    stream.respond({ ':status': 304 });
    return false; // Cancel the send operation
  }
  stream.respondWithFile('/some/file',
                         { 'content-type': 'text/plain' },
                         { statCheck });
});
```

The `content-length` header field will be automatically set.

The `offset` and `length` options may be used to limit the response to a
specific range subset. This can be used, for instance, to support HTTP Range
requests.

The `options.onError` function may also be used to handle all the errors
that could happen before the delivery of the file is initiated. The
default behavior is to destroy the stream.

When set, the `options.getTrailers()` function is called immediately after
queuing the last chunk of payload data to be sent. The callback is passed a
single object (with a `null` prototype) that the listener may used to specify
the trailing header fields to send to the peer.

```js
const http2 = require('http2');
const server = http2.createServer();
server.on('stream', (stream) => {
  function getTrailers(trailers) {
    trailers['ABC'] = 'some value to send';
  }
  stream.respondWithFile('/some/file',
                         { 'content-type': 'text/plain' },
                         { getTrailers });
});
```

*Note*: The HTTP/1 specification forbids trailers from containing HTTP/2
"pseudo-header" fields (e.g. `':status'`, `':path'`, etc). An `'error'` event
will be emitted if the `getTrailers` callback attempts to set such header
fields.

### Class: Http2Server
<!-- YAML
added: v8.4.0
-->

* Extends: {net.Server}

In `Http2Server`, there is no `'clientError'` event as there is in
HTTP1. However, there are `'socketError'`, `'sessionError'`,  and
`'streamError'`, for error happened on the socket, session or stream
respectively.

#### Event: 'socketError'
<!-- YAML
added: v8.4.0
-->

The `'socketError'` event is emitted when a `'socketError'` event is emitted by
an `Http2Session` associated with the server.

#### Event: 'sessionError'
<!-- YAML
added: v8.4.0
-->

The `'sessionError'` event is emitted when an `'error'` event is emitted by
an `Http2Session` object. If no listener is registered for this event, an
`'error'` event is emitted.

#### Event: 'streamError'
<!-- YAML
added: v8.5.0
-->

* `socket` {http2.ServerHttp2Stream}

If an `ServerHttp2Stream` emits an `'error'` event, it will be forwarded here.
The stream will already be destroyed when this event is triggered.

#### Event: 'stream'
<!-- YAML
added: v8.4.0
-->

The `'stream'` event is emitted when a `'stream'` event has been emitted by
an `Http2Session` associated with the server.

```js
const http2 = require('http2');
const {
  HTTP2_HEADER_METHOD,
  HTTP2_HEADER_PATH,
  HTTP2_HEADER_STATUS,
  HTTP2_HEADER_CONTENT_TYPE
} = http2.constants;

const server = http2.createServer();
server.on('stream', (stream, headers, flags) => {
  const method = headers[HTTP2_HEADER_METHOD];
  const path = headers[HTTP2_HEADER_PATH];
  // ...
  stream.respond({
    [HTTP2_HEADER_STATUS]: 200,
    [HTTP2_HEADER_CONTENT_TYPE]: 'text/plain'
  });
  stream.write('hello ');
  stream.end('world');
});
```

#### Event: 'request'
<!-- YAML
added: v8.4.0
-->

* `request` {http2.Http2ServerRequest}
* `response` {http2.Http2ServerResponse}

Emitted each time there is a request. Note that there may be multiple requests
per session. See the [Compatibility API][].

#### Event: 'timeout'
<!-- YAML
added: v8.4.0
-->

The `'timeout'` event is emitted when there is no activity on the Server for
a given number of milliseconds set using `http2server.setTimeout()`.

#### Event: 'checkContinue'
<!-- YAML
added: v8.5.0
-->

* `request` {http2.Http2ServerRequest}
* `response` {http2.Http2ServerResponse}

If a [`'request'`][] listener is registered or [`http2.createServer()`][] is
supplied a callback function, the `'checkContinue'` event is emitted each time
a request with an HTTP `Expect: 100-continue` is received. If this event is
not listened for, the server will automatically respond with a status
`100 Continue` as appropriate.

Handling this event involves calling [`response.writeContinue()`][] if the client
should continue to send the request body, or generating an appropriate HTTP
response (e.g. 400 Bad Request) if the client should not continue to send the
request body.

Note that when this event is emitted and handled, the [`'request'`][] event will
not be emitted.

### Class: Http2SecureServer
<!-- YAML
added: v8.4.0
-->

* Extends: {tls.Server}

#### Event: 'sessionError'
<!-- YAML
added: v8.4.0
-->

The `'sessionError'` event is emitted when an `'error'` event is emitted by
an `Http2Session` object. If no listener is registered for this event, an
`'error'` event is emitted on the `Http2Session` instance instead.

#### Event: 'socketError'
<!-- YAML
added: v8.4.0
-->

The `'socketError'` event is emitted when a `'socketError'` event is emitted by
an `Http2Session` associated with the server.

#### Event: 'unknownProtocol'
<!-- YAML
added: v8.4.0
-->

The `'unknownProtocol'` event is emitted when a connecting client fails to
negotiate an allowed protocol (i.e. HTTP/2 or HTTP/1.1). The event handler
receives the socket for handling. If no listener is registered for this event,
the connection is terminated. See the [Compatibility API][].

#### Event: 'stream'
<!-- YAML
added: v8.4.0
-->

The `'stream'` event is emitted when a `'stream'` event has been emitted by
an `Http2Session` associated with the server.

```js
const http2 = require('http2');
const {
  HTTP2_HEADER_METHOD,
  HTTP2_HEADER_PATH,
  HTTP2_HEADER_STATUS,
  HTTP2_HEADER_CONTENT_TYPE
} = http2.constants;

const options = getOptionsSomehow();

const server = http2.createSecureServer(options);
server.on('stream', (stream, headers, flags) => {
  const method = headers[HTTP2_HEADER_METHOD];
  const path = headers[HTTP2_HEADER_PATH];
  // ...
  stream.respond({
    [HTTP2_HEADER_STATUS]: 200,
    [HTTP2_HEADER_CONTENT_TYPE]: 'text/plain'
  });
  stream.write('hello ');
  stream.end('world');
});
```

#### Event: 'request'
<!-- YAML
added: v8.4.0
-->

* `request` {http2.Http2ServerRequest}
* `response` {http2.Http2ServerResponse}

Emitted each time there is a request. Note that there may be multiple requests
per session. See the [Compatibility API][].

#### Event: 'timeout'
<!-- YAML
added: v8.4.0
-->

#### Event: 'checkContinue'
<!-- YAML
added: v8.5.0
-->

* `request` {http2.Http2ServerRequest}
* `response` {http2.Http2ServerResponse}

If a [`'request'`][] listener is registered or [`http2.createSecureServer()`][]
is supplied a callback function, the `'checkContinue'` event is emitted each
time a request with an HTTP `Expect: 100-continue` is received. If this event
is not listened for, the server will automatically respond with a status
`100 Continue` as appropriate.

Handling this event involves calling [`response.writeContinue()`][] if the client
should continue to send the request body, or generating an appropriate HTTP
response (e.g. 400 Bad Request) if the client should not continue to send the
request body.

Note that when this event is emitted and handled, the [`'request'`][] event will
not be emitted.

### http2.createServer(options[, onRequestHandler])
<!-- YAML
added: v8.4.0
-->

* `options` {Object}
  * `maxDeflateDynamicTableSize` {number} Sets the maximum dynamic table size
    for deflating header fields. Defaults to 4Kib.
  * `maxSendHeaderBlockLength` {number} Sets the maximum allowed size for a
    serialized, compressed block of headers. Attempts to send headers that
    exceed this limit will result in a `'frameError'` event being emitted
    and the stream being closed and destroyed.
  * `paddingStrategy` {number} Identifies the strategy used for determining the
     amount of padding to use for HEADERS and DATA frames. Defaults to
     `http2.constants.PADDING_STRATEGY_NONE`. Value may be one of:
     * `http2.constants.PADDING_STRATEGY_NONE` - Specifies that no padding is
       to be applied.
     * `http2.constants.PADDING_STRATEGY_MAX` - Specifies that the maximum
       amount of padding, as determined by the internal implementation, is to
       be applied.
     * `http2.constants.PADDING_STRATEGY_CALLBACK` - Specifies that the user
       provided `options.selectPadding` callback is to be used to determine the
       amount of padding.
  * `peerMaxConcurrentStreams` {number} Sets the maximum number of concurrent
    streams for the remote peer as if a SETTINGS frame had been received. Will
    be overridden if the remote peer sets its own value for
    `maxConcurrentStreams`. Defaults to 100.
  * `selectPadding` {Function} When `options.paddingStrategy` is equal to
    `http2.constants.PADDING_STRATEGY_CALLBACK`, provides the callback function
    used to determine the padding. See [Using options.selectPadding][].
  * `settings` {[Settings Object][]} The initial settings to send to the
    remote peer upon connection.
* `onRequestHandler` {Function} See [Compatibility API][]
* Returns: {Http2Server}

Returns a `net.Server` instance that creates and manages `Http2Session`
instances.

```js
const http2 = require('http2');

// Create a plain-text HTTP/2 server
const server = http2.createServer();

server.on('stream', (stream, headers) => {
  stream.respond({
    'content-type': 'text/html',
    ':status': 200
  });
  stream.end('<h1>Hello World</h1>');
});

server.listen(80);
```

### http2.createSecureServer(options[, onRequestHandler])
<!-- YAML
added: v8.4.0
-->

* `options` {Object}
  * `allowHTTP1` {boolean} Incoming client connections that do not support
    HTTP/2 will be downgraded to HTTP/1.x when set to `true`. The default value
    is `false`. See the [`'unknownProtocol'`][] event. See [ALPN negotiation][].
  * `maxDeflateDynamicTableSize` {number} Sets the maximum dynamic table size
    for deflating header fields. Defaults to 4Kib.
  * `maxSendHeaderBlockLength` {number} Sets the maximum allowed size for a
    serialized, compressed block of headers. Attempts to send headers that
    exceed this limit will result in a `'frameError'` event being emitted
    and the stream being closed and destroyed.
  * `paddingStrategy` {number} Identifies the strategy used for determining the
     amount of padding to use for HEADERS and DATA frames. Defaults to
     `http2.constants.PADDING_STRATEGY_NONE`. Value may be one of:
     * `http2.constants.PADDING_STRATEGY_NONE` - Specifies that no padding is
       to be applied.
     * `http2.constants.PADDING_STRATEGY_MAX` - Specifies that the maximum
       amount of padding, as determined by the internal implementation, is to
       be applied.
     * `http2.constants.PADDING_STRATEGY_CALLBACK` - Specifies that the user
       provided `options.selectPadding` callback is to be used to determine the
       amount of padding.
  * `peerMaxConcurrentStreams` {number} Sets the maximum number of concurrent
    streams for the remote peer as if a SETTINGS frame had been received. Will
    be overridden if the remote peer sets its own value for
    `maxConcurrentStreams`. Defaults to 100.
  * `selectPadding` {Function} When `options.paddingStrategy` is equal to
    `http2.constants.PADDING_STRATEGY_CALLBACK`, provides the callback function
    used to determine the padding. See [Using options.selectPadding][].
  * `settings` {[Settings Object][]} The initial settings to send to the
    remote peer upon connection.
  * ...: Any [`tls.createServer()`][] options can be provided. For
    servers, the identity options (`pfx` or `key`/`cert`) are usually required.
* `onRequestHandler` {Function} See [Compatibility API][]
* Returns {Http2SecureServer}

Returns a `tls.Server` instance that creates and manages `Http2Session`
instances.

```js
const http2 = require('http2');

const options = {
  key: fs.readFileSync('server-key.pem'),
  cert: fs.readFileSync('server-cert.pem')
};

// Create a secure HTTP/2 server
const server = http2.createSecureServer(options);

server.on('stream', (stream, headers) => {
  stream.respond({
    'content-type': 'text/html',
    ':status': 200
  });
  stream.end('<h1>Hello World</h1>');
});

server.listen(80);
```

### http2.connect(authority[, options][, listener])
<!-- YAML
added: v8.4.0
-->

* `authority` {string|URL}
* `options` {Object}
  * `maxDeflateDynamicTableSize` {number} Sets the maximum dynamic table size
    for deflating header fields. Defaults to 4Kib.
  * `maxReservedRemoteStreams` {number} Sets the maximum number of reserved push
    streams the client will accept at any given time. Once the current number of
    currently reserved push streams exceeds reaches this limit, new push streams
    sent by the server will be automatically rejected.
  * `maxSendHeaderBlockLength` {number} Sets the maximum allowed size for a
    serialized, compressed block of headers. Attempts to send headers that
    exceed this limit will result in a `'frameError'` event being emitted
    and the stream being closed and destroyed.
  * `paddingStrategy` {number} Identifies the strategy used for determining the
     amount of padding to use for HEADERS and DATA frames. Defaults to
     `http2.constants.PADDING_STRATEGY_NONE`. Value may be one of:
     * `http2.constants.PADDING_STRATEGY_NONE` - Specifies that no padding is
       to be applied.
     * `http2.constants.PADDING_STRATEGY_MAX` - Specifies that the maximum
       amount of padding, as determined by the internal implementation, is to
       be applied.
     * `http2.constants.PADDING_STRATEGY_CALLBACK` - Specifies that the user
       provided `options.selectPadding` callback is to be used to determine the
       amount of padding.
  * `peerMaxConcurrentStreams` {number} Sets the maximum number of concurrent
    streams for the remote peer as if a SETTINGS frame had been received. Will
    be overridden if the remote peer sets its own value for
    `maxConcurrentStreams`. Defaults to 100.
  * `selectPadding` {Function} When `options.paddingStrategy` is equal to
    `http2.constants.PADDING_STRATEGY_CALLBACK`, provides the callback function
    used to determine the padding. See [Using options.selectPadding][].
  * `settings` {[Settings Object][]} The initial settings to send to the
    remote peer upon connection.
  * `createConnection` {Function} An optional callback that receives the `URL`
    instance passed to `connect` and the `options` object, and returns any
    [`Duplex`][] stream that is to be used as the connection for this session.
* `listener` {Function}
* Returns {Http2Session}

Returns a HTTP/2 client `Http2Session` instance.

```js
const http2 = require('http2');
const client = http2.connect('https://localhost:1234');

/** use the client **/

client.destroy();
```

### http2.constants
<!-- YAML
added: v8.4.0
-->

#### Error Codes for RST_STREAM and GOAWAY
<a id="error_codes"></a>

| Value | Name                | Constant                                      |
|-------|---------------------|-----------------------------------------------|
| 0x00  | No Error            | `http2.constants.NGHTTP2_NO_ERROR`            |
| 0x01  | Protocol Error      | `http2.constants.NGHTTP2_PROTOCOL_ERROR`      |
| 0x02  | Internal Error      | `http2.constants.NGHTTP2_INTERNAL_ERROR`      |
| 0x03  | Flow Control Error  | `http2.constants.NGHTTP2_FLOW_CONTROL_ERROR`  |
| 0x04  | Settings Timeout    | `http2.constants.NGHTTP2_SETTINGS_TIMEOUT`    |
| 0x05  | Stream Closed       | `http2.constants.NGHTTP2_STREAM_CLOSED`       |
| 0x06  | Frame Size Error    | `http2.constants.NGHTTP2_FRAME_SIZE_ERROR`    |
| 0x07  | Refused Stream      | `http2.constants.NGHTTP2_REFUSED_STREAM`      |
| 0x08  | Cancel              | `http2.constants.NGHTTP2_CANCEL`              |
| 0x09  | Compression Error   | `http2.constants.NGHTTP2_COMPRESSION_ERROR`   |
| 0x0a  | Connect Error       | `http2.constants.NGHTTP2_CONNECT_ERROR`       |
| 0x0b  | Enhance Your Calm   | `http2.constants.NGHTTP2_ENHANCE_YOUR_CALM`   |
| 0x0c  | Inadequate Security | `http2.constants.NGHTTP2_INADEQUATE_SECURITY` |
| 0x0d  | HTTP/1.1 Required   | `http2.constants.NGHTTP2_HTTP_1_1_REQUIRED`   |

The `'timeout'` event is emitted when there is no activity on the Server for
a given number of milliseconds set using `http2server.setTimeout()`.

### http2.getDefaultSettings()
<!-- YAML
added: v8.4.0
-->

* Returns: {[Settings Object][]}

Returns an object containing the default settings for an `Http2Session`
instance. This method returns a new object instance every time it is called
so instances returned may be safely modified for use.

### http2.getPackedSettings(settings)
<!-- YAML
added: v8.4.0
-->

* `settings` {[Settings Object][]}
* Returns: {Buffer}

Returns a `Buffer` instance containing serialized representation of the given
HTTP/2 settings as specified in the [HTTP/2][] specification. This is intended
for use with the `HTTP2-Settings` header field.

```js
const http2 = require('http2');

const packed = http2.getPackedSettings({ enablePush: false });

console.log(packed.toString('base64'));
// Prints: AAIAAAAA
```

### http2.getUnpackedSettings(buf)
<!-- YAML
added: v8.4.0
-->

* `buf` {Buffer|Uint8Array} The packed settings
* Returns: {[Settings Object][]}

Returns a [Settings Object][] containing the deserialized settings from the
given `Buffer` as generated by `http2.getPackedSettings()`.

### Headers Object

Headers are represented as own-properties on JavaScript objects. The property
keys will be serialized to lower-case. Property values should be strings (if
they are not they will be coerced to strings) or an Array of strings (in order
to send more than one value per header field).

For example:

```js
const headers = {
  ':status': '200',
  'content-type': 'text-plain',
  'ABC': ['has', 'more', 'than', 'one', 'value']
};

stream.respond(headers);
```

*Note*: Header objects passed to callback functions will have a `null`
prototype. This means that normal JavaScript object methods such as
`Object.prototype.toString()` and `Object.prototype.hasOwnProperty()` will
not work.

```js
const http2 = require('http2');
const server = http2.createServer();
server.on('stream', (stream, headers) => {
  console.log(headers[':path']);
  console.log(headers.ABC);
});
```

### Settings Object

The `http2.getDefaultSettings()`, `http2.getPackedSettings()`,
`http2.createServer()`, `http2.createSecureServer()`,
`http2session.settings()`, `http2session.localSettings`, and
`http2session.remoteSettings` APIs either return or receive as input an
object that defines configuration settings for an `Http2Session` object.
These objects are ordinary JavaScript objects containing the following
properties.

* `headerTableSize` {number} Specifies the maximum number of bytes used for
  header compression. The default value is 4,096 octets. The minimum allowed
  value is 0. The maximum allowed value is 2<sup>32</sup>-1.
* `enablePush` {boolean} Specifies `true` if HTTP/2 Push Streams are to be
  permitted on the `Http2Session` instances.
* `initialWindowSize` {number} Specifies the *senders* initial window size
  for stream-level flow control. The default value is 65,535 bytes. The minimum
  allowed value is 0. The maximum allowed value is 2<sup>32</sup>-1.
* `maxFrameSize` {number} Specifies the size of the largest frame payload.
  The default and the minimum allowed value is 16,384 bytes. The maximum
  allowed value is 2<sup>24</sup>-1.
* `maxConcurrentStreams` {number} Specifies the maximum number of concurrent
  streams permitted on an `Http2Session`. There is no default value which
  implies, at least theoretically, 2<sup>31</sup>-1 streams may be open
  concurrently at any given time in an `Http2Session`. The minimum value is
  0. The maximum allowed value is 2<sup>31</sup>-1.
* `maxHeaderListSize` {number} Specifies the maximum size (uncompressed octets)
  of header list that will be accepted. There is no default value. The minimum
  allowed value is 0. The maximum allowed value is 2<sup>32</sup>-1.

All additional properties on the settings object are ignored.

### Using `options.selectPadding`

When `options.paddingStrategy` is equal to
`http2.constants.PADDING_STRATEGY_CALLBACK`, the the HTTP/2 implementation will
consult the `options.selectPadding` callback function, if provided, to determine
the specific amount of padding to use per HEADERS and DATA frame.

The `options.selectPadding` function receives two numeric arguments,
`frameLen` and `maxFrameLen` and must return a number `N` such that
`frameLen <= N <= maxFrameLen`.

```js
const http2 = require('http2');
const server = http2.createServer({
  paddingStrategy: http2.constants.PADDING_STRATEGY_CALLBACK,
  selectPadding(frameLen, maxFrameLen) {
    return maxFrameLen;
  }
});
```

*Note*: The `options.selectPadding` function is invoked once for *every*
HEADERS and DATA frame. This has a definite noticeable impact on
performance.

### Error Handling

There are several types of error conditions that may arise when using the
`http2` module:

Validation Errors occur when an incorrect argument, option or setting value is
passed in. These will always be reported by a synchronous `throw`.

State Errors occur when an action is attempted at an incorrect time (for
instance, attempting to send data on a stream after it has closed). These will
be reported using either a synchronous `throw` or via an `'error'` event on
the `Http2Stream`, `Http2Session` or HTTP/2 Server objects, depending on where
and when the error occurs.

Internal Errors occur when an HTTP/2 session fails unexpectedly. These will be
reported via an `'error'` event on the `Http2Session` or HTTP/2 Server objects.

Protocol Errors occur when various HTTP/2 protocol constraints are violated.
These will be reported using either a synchronous `throw` or via an `'error'`
event on the `Http2Stream`, `Http2Session` or HTTP/2 Server objects, depending
on where and when the error occurs.

### Invalid character handling in header names and values

The HTTP/2 implementation applies stricter handling of invalid characters in
HTTP header names and values than the HTTP/1 implementation.

Header field names are *case-insensitive* and are transmitted over the wire
strictly as lower-case strings. The API provided by Node.js allows header
names to be set as mixed-case strings (e.g. `Content-Type`) but will convert
those to lower-case (e.g. `content-type`) upon transmission.

Header field-names *must only* contain one or more of the following ASCII
characters: `a`-`z`, `A`-`Z`, `0`-`9`, `!`, `#`, `$`, `%`, `&`, `'`, `*`, `+`,
`-`, `.`, `^`, `_`, `` ` `` (backtick), `|`, and `~`.

Using invalid characters within an HTTP header field name will cause the
stream to be closed with a protocol error being reported.

Header field values are handled with more leniency but *should* not contain
new-line or carriage return characters and *should* be limited to US-ASCII
characters, per the requirements of the HTTP specification.

### Push streams on the client

To receive pushed streams on the client, set a listener for the `'stream'`
event on the `ClientHttp2Session`:

```js
const http2 = require('http2');

const client = http2.connect('http://localhost');

client.on('stream', (pushedStream, requestHeaders) => {
  pushedStream.on('push', (responseHeaders) => {
    // process response headers
  });
  pushedStream.on('data', (chunk) => { /* handle pushed data */ });
});

const req = client.request({ ':path': '/' });
```

### Supporting the CONNECT method

The `CONNECT` method is used to allow an HTTP/2 server to be used as a proxy
for TCP/IP connections.

A simple TCP Server:
```js
const net = require('net');

const server = net.createServer((socket) => {
  let name = '';
  socket.setEncoding('utf8');
  socket.on('data', (chunk) => name += chunk);
  socket.on('end', () => socket.end(`hello ${name}`));
});

server.listen(8000);
```

An HTTP/2 CONNECT proxy:

```js
const http2 = require('http2');
const net = require('net');
const { URL } = require('url');

const proxy = http2.createServer();
proxy.on('stream', (stream, headers) => {
  if (headers[':method'] !== 'CONNECT') {
    // Only accept CONNECT requests
    stream.rstWithRefused();
    return;
  }
  const auth = new URL(`tcp://${headers[':authority']}`);
  // It's a very good idea to verify that hostname and port are
  // things this proxy should be connecting to.
  const socket = net.connect(auth.port, auth.hostname, () => {
    stream.respond();
    socket.pipe(stream);
    stream.pipe(socket);
  });
  socket.on('error', (error) => {
    stream.rstStream(http2.constants.NGHTTP2_CONNECT_ERROR);
  });
});

proxy.listen(8001);
```

An HTTP/2 CONNECT client:

```js
const http2 = require('http2');

const client = http2.connect('http://localhost:8001');

// Must not specify the ':path' and ':scheme' headers
// for CONNECT requests or an error will be thrown.
const req = client.request({
  ':method': 'CONNECT',
  ':authority': `localhost:${port}`
});

req.on('response', (headers) => {
  console.log(headers[http2.constants.HTTP2_HEADER_STATUS]);
});
let data = '';
req.setEncoding('utf8');
req.on('data', (chunk) => data += chunk);
req.on('end', () => {
  console.log(`The server says: ${data}`);
  client.destroy();
});
req.end('Jane');
```

## Compatibility API

The Compatibility API has the goal of providing a similar developer experience
of HTTP/1 when using HTTP/2, making it possible to develop applications
that supports both [HTTP/1][] and HTTP/2. This API targets only the
**public API** of the [HTTP/1][], however many modules uses internal
methods or state, and those _are not supported_ as it is a completely
different implementation.

The following example creates an HTTP/2 server using the compatibility
API:

```js
const http2 = require('http2');
const server = http2.createServer((req, res) => {
  res.setHeader('Content-Type', 'text/html');
  res.setHeader('X-Foo', 'bar');
  res.writeHead(200, { 'Content-Type': 'text/plain' });
  res.end('ok');
});
```

In order to create a mixed [HTTPS][] and HTTP/2 server, refer to the
[ALPN negotiation][] section.
Upgrading from non-tls HTTP/1 servers is not supported.

The HTTP2 compatibility API is composed of [`Http2ServerRequest`]() and
[`Http2ServerResponse`](). They aim at API compatibility with HTTP/1, but
they do not hide the differences between the protocols. As an example,
the status message for HTTP codes is ignored.

### ALPN negotiation

ALPN negotiation allows to support both [HTTPS][] and HTTP/2 over
the same socket. The `req` and `res` objects can be either HTTP/1 or
HTTP/2, and an application **must** restrict itself to the public API of
[HTTP/1][], and detect if it is possible to use the more advanced
features of HTTP/2.

The following example creates a server that supports both protocols:

```js
const { createSecureServer } = require('http2');
const { readFileSync } = require('fs');

const cert = readFileSync('./cert.pem');
const key = readFileSync('./key.pem');

const server = createSecureServer(
  { cert, key, allowHTTP1: true },
  onRequest
).listen(4443);

function onRequest(req, res) {
  // detects if it is a HTTPS request or HTTP/2
  const { socket: { alpnProtocol } } = req.httpVersion === '2.0' ?
    req.stream.session : req;
  res.writeHead(200, { 'content-type': 'application/json' });
  res.end(JSON.stringify({
    alpnProtocol,
    httpVersion: req.httpVersion
  }));
}
```

The `'request'` event works identically on both [HTTPS][] and
HTTP/2.

### Class: http2.Http2ServerRequest
<!-- YAML
added: v8.4.0
-->

A `Http2ServerRequest` object is created by [`http2.Server`][] or
[`http2.SecureServer`][] and passed as the first argument to the
[`'request'`][] event. It may be used to access a request status, headers and
data.

It implements the [Readable Stream][] interface, as well as the
following additional events, methods, and properties.

#### Event: 'aborted'
<!-- YAML
added: v8.4.0
-->

The `'aborted'` event is emitted whenever a `Http2ServerRequest` instance is
abnormally aborted in mid-communication.

*Note*: The `'aborted'` event will only be emitted if the
`Http2ServerRequest` writable side has not been ended.

#### Event: 'close'
<!-- YAML
added: v8.4.0
-->

Indicates that the underlying [`Http2Stream`][] was closed.
Just like `'end'`, this event occurs only once per response.

#### request.destroy([error])
<!-- YAML
added: v8.4.0
-->

* `error` {Error}

Calls `destroy()` on the [`Http2Stream`][] that received
the [`Http2ServerRequest`][]. If `error` is provided, an `'error'` event
is emitted and `error` is passed as an argument to any listeners on the event.

It does nothing if the stream was already destroyed.

#### request.headers
<!-- YAML
added: v8.4.0
-->

* {Object}

The request/response headers object.

Key-value pairs of header names and values. Header names are lower-cased.
Example:

```js
// Prints something like:
//
// { 'user-agent': 'curl/7.22.0',
//   host: '127.0.0.1:8000',
//   accept: '*/*' }
console.log(request.headers);
```

See [Headers Object][].

#### request.httpVersion
<!-- YAML
added: v8.4.0
-->

* {string}

In case of server request, the HTTP version sent by the client. In the case of
client response, the HTTP version of the connected-to server. Returns
`'2.0'`.

Also `message.httpVersionMajor` is the first integer and
`message.httpVersionMinor` is the second.

#### request.method
<!-- YAML
added: v8.4.0
-->

* {string}

The request method as a string. Read only. Example:
`'GET'`, `'DELETE'`.

#### request.rawHeaders
<!-- YAML
added: v8.4.0
-->

* {Array}

The raw request/response headers list exactly as they were received.

Note that the keys and values are in the same list.  It is *not* a
list of tuples.  So, the even-numbered offsets are key values, and the
odd-numbered offsets are the associated values.

Header names are not lowercased, and duplicates are not merged.

```js
// Prints something like:
//
// [ 'user-agent',
//   'this is invalid because there can be only one',
//   'User-Agent',
//   'curl/7.22.0',
//   'Host',
//   '127.0.0.1:8000',
//   'ACCEPT',
//   '*/*' ]
console.log(request.rawHeaders);
```

#### request.rawTrailers
<!-- YAML
added: v8.4.0
-->

* {Array}

The raw request/response trailer keys and values exactly as they were
received.  Only populated at the `'end'` event.

#### request.setTimeout(msecs, callback)
<!-- YAML
added: v8.4.0
-->

* `msecs` {number}
* `callback` {Function}

Sets the [`Http2Stream`]()'s timeout value to `msecs`.  If a callback is
provided, then it is added as a listener on the `'timeout'` event on
the response object.

If no `'timeout'` listener is added to the request, the response, or
the server, then [`Http2Stream`]()s are destroyed when they time out. If a
handler is assigned to the request, the response, or the server's `'timeout'`
events, timed out sockets must be handled explicitly.

Returns `request`.

#### request.socket
<!-- YAML
added: v8.4.0
-->

* {net.Socket|tls.TLSSocket}

Returns a Proxy object that acts as a `net.Socket` (or `tls.TLSSocket`) but
applies getters, setters and methods based on HTTP/2 logic.

`destroyed`, `readable`, and `writable` properties will be retrieved from and
set on `request.stream`.

`destroy`, `emit`, `end`, `on` and `once` methods will be called on
`request.stream`.

`setTimeout` method will be called on `request.stream.session`.

`pause`, `read`, `resume`, and `write` will throw an error with code
`ERR_HTTP2_NO_SOCKET_MANIPULATION`. See [Http2Session and Sockets][] for
more information.

All other interactions will be routed directly to the socket. With TLS support,
use [`request.socket.getPeerCertificate()`][] to obtain the client's
authentication details.

#### request.stream
<!-- YAML
added: v8.4.0
-->

* {http2.Http2Stream}

The [`Http2Stream`][] object backing the request.

#### request.trailers
<!-- YAML
added: v8.4.0
-->

* {Object}

The request/response trailers object. Only populated at the `'end'` event.

#### request.url
<!-- YAML
added: v8.4.0
-->

* {string}

Request URL string. This contains only the URL that is
present in the actual HTTP request. If the request is:

```txt
GET /status?name=ryan HTTP/1.1\r\n
Accept: text/plain\r\n
\r\n
```

Then `request.url` will be:

<!-- eslint-disable semi -->
```js
'/status?name=ryan'
```

To parse the url into its parts `require('url').parse(request.url)`
can be used.  Example:

```txt
$ node
> require('url').parse('/status?name=ryan')
Url {
  protocol: null,
  slashes: null,
  auth: null,
  host: null,
  port: null,
  hostname: null,
  hash: null,
  search: '?name=ryan',
  query: 'name=ryan',
  pathname: '/status',
  path: '/status?name=ryan',
  href: '/status?name=ryan' }
```

To extract the parameters from the query string, the
`require('querystring').parse` function can be used, or
`true` can be passed as the second argument to `require('url').parse`.
Example:

```txt
$ node
> require('url').parse('/status?name=ryan', true)
Url {
  protocol: null,
  slashes: null,
  auth: null,
  host: null,
  port: null,
  hostname: null,
  hash: null,
  search: '?name=ryan',
  query: { name: 'ryan' },
  pathname: '/status',
  path: '/status?name=ryan',
  href: '/status?name=ryan' }
```

### Class: http2.Http2ServerResponse
<!-- YAML
added: v8.4.0
-->

This object is created internally by an HTTP server--not by the user. It is
passed as the second parameter to the [`'request'`][] event.

The response implements, but does not inherit from, the [Writable Stream][]
interface. This is an [`EventEmitter`][] with the following events:

#### Event: 'close'
<!-- YAML
added: v8.4.0
-->

Indicates that the underlying [`Http2Stream`]() was terminated before
[`response.end()`][] was called or able to flush.

#### Event: 'finish'
<!-- YAML
added: v8.4.0
-->

Emitted when the response has been sent. More specifically, this event is
emitted when the last segment of the response headers and body have been
handed off to the HTTP/2 multiplexing for transmission over the network. It
does not imply that the client has received anything yet.

After this event, no more events will be emitted on the response object.

#### response.addTrailers(headers)
<!-- YAML
added: v8.4.0
-->

* `headers` {Object}

This method adds HTTP trailing headers (a header but at the end of the
message) to the response.

Attempting to set a header field name or value that contains invalid characters
will result in a [`TypeError`][] being thrown.

#### response.connection
<!-- YAML
added: v8.4.0
-->

* {net.Socket|tls.TLSSocket}

See [`response.socket`][].

#### response.end([data][, encoding][, callback])
<!-- YAML
added: v8.4.0
-->

* `data` {string|Buffer}
* `encoding` {string}
* `callback` {Function}

This method signals to the server that all of the response headers and body
have been sent; that server should consider this message complete.
The method, `response.end()`, MUST be called on each response.

If `data` is specified, it is equivalent to calling
[`response.write(data, encoding)`][] followed by `response.end(callback)`.

If `callback` is specified, it will be called when the response stream
is finished.

#### response.finished
<!-- YAML
added: v8.4.0
-->

* {boolean}

Boolean value that indicates whether the response has completed. Starts
as `false`. After [`response.end()`][] executes, the value will be `true`.

#### response.getHeader(name)
<!-- YAML
added: v8.4.0
-->

* `name` {string}
* Returns: {string}

Reads out a header that has already been queued but not sent to the client.
Note that the name is case insensitive.

Example:

```js
const contentType = response.getHeader('content-type');
```

#### response.getHeaderNames()
<!-- YAML
added: v8.4.0
-->

* Returns: {Array}

Returns an array containing the unique names of the current outgoing headers.
All header names are lowercase.

Example:

```js
response.setHeader('Foo', 'bar');
response.setHeader('Set-Cookie', ['foo=bar', 'bar=baz']);

const headerNames = response.getHeaderNames();
// headerNames === ['foo', 'set-cookie']
```

#### response.getHeaders()
<!-- YAML
added: v8.4.0
-->

* Returns: {Object}

Returns a shallow copy of the current outgoing headers. Since a shallow copy
is used, array values may be mutated without additional calls to various
header-related http module methods. The keys of the returned object are the
header names and the values are the respective header values. All header names
are lowercase.

*Note*: The object returned by the `response.getHeaders()` method _does not_
prototypically inherit from the JavaScript `Object`. This means that typical
`Object` methods such as `obj.toString()`, `obj.hasOwnProperty()`, and others
are not defined and *will not work*.

Example:

```js
response.setHeader('Foo', 'bar');
response.setHeader('Set-Cookie', ['foo=bar', 'bar=baz']);

const headers = response.getHeaders();
// headers === { foo: 'bar', 'set-cookie': ['foo=bar', 'bar=baz'] }
```

#### response.hasHeader(name)
<!-- YAML
added: v8.4.0
-->

* `name` {string}
* Returns: {boolean}

Returns `true` if the header identified by `name` is currently set in the
outgoing headers. Note that the header name matching is case-insensitive.

Example:

```js
const hasContentType = response.hasHeader('content-type');
```

#### response.headersSent
<!-- YAML
added: v8.4.0
-->

* {boolean}

Boolean (read-only). True if headers were sent, false otherwise.

#### response.removeHeader(name)
<!-- YAML
added: v8.4.0
-->

* `name` {string}

Removes a header that has been queued for implicit sending.

Example:

```js
response.removeHeader('Content-Encoding');
```

#### response.sendDate
<!-- YAML
added: v8.4.0
-->

* {boolean}

When true, the Date header will be automatically generated and sent in
the response if it is not already present in the headers. Defaults to true.

This should only be disabled for testing; HTTP requires the Date header
in responses.

#### response.setHeader(name, value)
<!-- YAML
added: v8.4.0
-->

* `name` {string}
* `value` {string | string[]}

Sets a single header value for implicit headers.  If this header already exists
in the to-be-sent headers, its value will be replaced.  Use an array of strings
here to send multiple headers with the same name.

Example:

```js
response.setHeader('Content-Type', 'text/html');
```

or

```js
response.setHeader('Set-Cookie', ['type=ninja', 'language=javascript']);
```

Attempting to set a header field name or value that contains invalid characters
will result in a [`TypeError`][] being thrown.

When headers have been set with [`response.setHeader()`][], they will be merged
with any headers passed to [`response.writeHead()`][], with the headers passed
to [`response.writeHead()`][] given precedence.

```js
// returns content-type = text/plain
const server = http2.createServer((req, res) => {
  res.setHeader('Content-Type', 'text/html');
  res.setHeader('X-Foo', 'bar');
  res.writeHead(200, { 'Content-Type': 'text/plain' });
  res.end('ok');
});
```

#### response.setTimeout(msecs[, callback])
<!-- YAML
added: v8.4.0
-->

* `msecs` {number}
* `callback` {Function}

Sets the [`Http2Stream`]()'s timeout value to `msecs`.  If a callback is
provided, then it is added as a listener on the `'timeout'` event on
the response object.

If no `'timeout'` listener is added to the request, the response, or
the server, then [`Http2Stream`]()s are destroyed when they time out. If a
handler is assigned to the request, the response, or the server's `'timeout'`
events, timed out sockets must be handled explicitly.

Returns `response`.

#### response.socket
<!-- YAML
added: v8.4.0
-->

* {net.Socket|tls.TLSSocket}

Returns a Proxy object that acts as a `net.Socket` (or `tls.TLSSocket`) but
applies getters, setters and methods based on HTTP/2 logic.

`destroyed`, `readable`, and `writable` properties will be retrieved from and
set on `response.stream`.

`destroy`, `emit`, `end`, `on` and `once` methods will be called on
`response.stream`.

`setTimeout` method will be called on `response.stream.session`.

`pause`, `read`, `resume`, and `write` will throw an error with code
`ERR_HTTP2_NO_SOCKET_MANIPULATION`. See [Http2Session and Sockets][] for
more information.

All other interactions will be routed directly to the socket.

Example:

```js
const http2 = require('http2');
const server = http2.createServer((req, res) => {
  const ip = req.socket.remoteAddress;
  const port = req.socket.remotePort;
  res.end(`Your IP address is ${ip} and your source port is ${port}.`);
}).listen(3000);
```

#### response.statusCode
<!-- YAML
added: v8.4.0
-->

* {number}

When using implicit headers (not calling [`response.writeHead()`][] explicitly),
this property controls the status code that will be sent to the client when
the headers get flushed.

Example:

```js
response.statusCode = 404;
```

After response header was sent to the client, this property indicates the
status code which was sent out.

#### response.statusMessage
<!-- YAML
added: v8.4.0
-->

* {string}

Status message is not supported by HTTP/2 (RFC7540 8.1.2.4). It returns
an empty string.

#### response.stream
<!-- YAML
added: v8.4.0
-->

* {http2.Http2Stream}

The [`Http2Stream`][] object backing the response.

#### response.write(chunk[, encoding][, callback])
<!-- YAML
added: v8.4.0
-->

* `chunk` {string|Buffer}
* `encoding` {string}
* `callback` {Function}
* Returns: {boolean}

If this method is called and [`response.writeHead()`][] has not been called,
it will switch to implicit header mode and flush the implicit headers.

This sends a chunk of the response body. This method may
be called multiple times to provide successive parts of the body.

Note that in the `http` module, the response body is omitted when the
request is a HEAD request. Similarly, the `204` and `304` responses
_must not_ include a message body.

`chunk` can be a string or a buffer. If `chunk` is a string,
the second parameter specifies how to encode it into a byte stream.
By default the `encoding` is `'utf8'`. `callback` will be called when this chunk
of data is flushed.

*Note*: This is the raw HTTP body and has nothing to do with
higher-level multi-part body encodings that may be used.

The first time [`response.write()`][] is called, it will send the buffered
header information and the first chunk of the body to the client. The second
time [`response.write()`][] is called, Node.js assumes data will be streamed,
and sends the new data separately. That is, the response is buffered up to the
first chunk of the body.

Returns `true` if the entire data was flushed successfully to the kernel
buffer. Returns `false` if all or part of the data was queued in user memory.
`'drain'` will be emitted when the buffer is free again.

#### response.writeContinue()
<!-- YAML
added: v8.4.0
-->

Sends a status `100 Continue` to the client, indicating that the request body
should be sent. See the [`'checkContinue'`][] event on `Http2Server` and
`Http2SecureServer`.

#### response.writeHead(statusCode[, statusMessage][, headers])
<!-- YAML
added: v8.4.0
-->

* `statusCode` {number}
* `statusMessage` {string}
* `headers` {Object}

Sends a response header to the request. The status code is a 3-digit HTTP
status code, like `404`. The last argument, `headers`, are the response headers.

For compatibility with [HTTP/1][], a human-readable `statusMessage` may be
passed as the second argument. However, because the `statusMessage` has no
meaning within HTTP/2, the argument will have no effect and a process warning
will be emitted.

Example:

```js
const body = 'hello world';
response.writeHead(200, {
  'Content-Length': Buffer.byteLength(body),
  'Content-Type': 'text/plain' });
```

Note that Content-Length is given in bytes not characters. The
`Buffer.byteLength()` API  may be used to determine the number of bytes in a
given encoding. On outbound messages, Node.js does not check if Content-Length
and the length of the body being transmitted are equal or not. However, when
receiving messages, Node.js will automatically reject messages when the
Content-Length does not match the actual payload size.

This method may be called at most one time on a message before
[`response.end()`][] is called.

If [`response.write()`][] or [`response.end()`][] are called before calling
this, the implicit/mutable headers will be calculated and call this function.

When headers have been set with [`response.setHeader()`][], they will be merged
with any headers passed to [`response.writeHead()`][], with the headers passed
to [`response.writeHead()`][] given precedence.

```js
// returns content-type = text/plain
const server = http2.createServer((req, res) => {
  res.setHeader('Content-Type', 'text/html');
  res.setHeader('X-Foo', 'bar');
  res.writeHead(200, { 'Content-Type': 'text/plain' });
  res.end('ok');
});
```

Attempting to set a header field name or value that contains invalid characters
will result in a [`TypeError`][] being thrown.

#### response.createPushResponse(headers, callback)
<!-- YAML
added: v8.4.0
-->

Call [`http2stream.pushStream()`][] with the given headers, and wraps the
given newly created [`Http2Stream`] on `Http2ServerRespose`.

The callback will be called with an error with code `ERR_HTTP2_STREAM_CLOSED`
if the stream is closed.

[ALPN negotiation]: #http2_alpn_negotiation
[Compatibility API]: #http2_compatibility_api
[HTTP/1]: http.html
[HTTP/2]: https://tools.ietf.org/html/rfc7540
[HTTPS]: https.html
[Headers Object]: #http2_headers_object
[Http2Session and Sockets]: #http2_http2session_and_sockets
[Readable Stream]: stream.html#stream_class_stream_readable
[Settings Object]: #http2_settings_object
[Using options.selectPadding]: #http2_using_options_selectpadding
[Writable Stream]: stream.html#stream_writable_streams
[`'checkContinue'`]: #http2_event_checkcontinue
[`'request'`]: #http2_event_request
[`'unknownProtocol'`]: #http2_event_unknownprotocol
[`ClientHttp2Stream`]: #http2_class_clienthttp2stream
[`Duplex`]: stream.html#stream_class_stream_duplex
[`EventEmitter`]: events.html#events_class_eventemitter
[`Http2ServerRequest`]: #http2_class_http2_http2serverrequest
[`Http2Stream`]: #http2_class_http2stream
[`ServerHttp2Stream`]: #http2_class_serverhttp2stream
[`TypeError`]: errors.html#errors_class_typeerror
[`http2.SecureServer`]: #http2_class_http2secureserver
[`http2.createSecureServer()`]: #http2_http2_createsecureserver_options_onrequesthandler
[`http2.Server`]: #http2_class_http2server
[`http2.createServer()`]: #http2_http2_createserver_options_onrequesthandler
[`http2stream.pushStream()`]: #http2_http2stream_pushstream_headers_options_callback
[`net.Socket`]: net.html#net_class_net_socket
[`request.socket.getPeerCertificate()`]: tls.html#tls_tlssocket_getpeercertificate_detailed
[`response.end()`]: #http2_response_end_data_encoding_callback
[`response.setHeader()`]: #http2_response_setheader_name_value
[`response.socket`]: #http2_response_socket
[`response.write()`]: #http2_response_write_chunk_encoding_callback
[`response.write(data, encoding)`]: http.html#http_response_write_chunk_encoding_callback
[`response.writeContinue()`]: #http2_response_writecontinue
[`response.writeHead()`]: #http2_response_writehead_statuscode_statusmessage_headers
[`tls.TLSSocket`]: tls.html#tls_class_tls_tlssocket
[`tls.createServer()`]: tls.html#tls_tls_createserver_options_secureconnectionlistener
[error code]: #error_codes

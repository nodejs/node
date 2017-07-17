# HTTP2

> Stability: 1 - Experimental

The `http2` module provides an implementation of the [HTTP/2][] protocol. It
can be accessed using:

```js
const http2 = require('http2');
```

*Note*: Node.js must be launched with the `--expose-http2` command line flag
in order to use the `'http2'` module.

## Core API

The Core API provides a low-level interface designed specifically around
support for HTTP/2 protocol features. It is specifically *not* designed for
compatibility with the existing [HTTP/1][] module API.

The following illustrates a simple, plain-text HTTP/2 server:

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

The following illustrates an HTTP/2 client:

```js
const http2 = require('http2');

const client = http2.connect('http://localhost:80');

const req = client.request({ ':path': '/' });

req.on('response', (headers) => {
  console.log(headers[':status']);
  console.log(headers['date']);
});

let data = '';
req.setEncoding('utf8');
req.on('data', (d) => data += d);
req.on('end', () => client.destroy());
req.end();
```

### Class: Http2Session
<!-- YAML
added: REPLACEME
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
added: REPLACEME
-->

The `'close'` event is emitted once the `Http2Session` has been terminated.

#### Event: 'connect'
<!-- YAML
added: REPLACEME
-->

The `'connect'` event is emitted once the `Http2Session` has been successfully
connected to the remote peer and communication may begin.

*Note*: User code will typically not listen for this event directly.

#### Event: 'error'
<!-- YAML
added: REPLACEME
-->

The `'error'` event is emitted when an error occurs during the processing of
an `Http2Session`.

#### Event: 'frameError'
<!-- YAML
added: REPLACEME
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
added: REPLACEME
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
added: REPLACEME
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
added: REPLACEME
-->

The `'remoteSettings'` event is emitted when a new SETTINGS frame is received
from the connected peer. When invoked, the handle function will receive a copy
of the remote settings.

```js
session.on('remoteSettings', (settings) => {
  /** use the new settings **/
});
```

#### Event: 'stream'
<!-- YAML
added: REPLACEME
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
added: REPLACEME
-->

The `'socketError'` event is emitted when an `'error'` is emitted on the
`Socket` instance bound to the `Http2Session`. If this event is not handled,
the `'error'` event will be re-emitted on the `Socket`.

Likewise, when an `'error'` event is emitted on the `Http2Session`, a
`'sessionError'` event will be emitted on the `Socket`. If that event is
not handled, the `'error'` event will be re-emitted on the `Http2Session`.

#### Event: 'timeout'
<!-- YAML
added: REPLACEME
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
added: REPLACEME
-->

* Returns: {undefined}

Immediately terminates the `Http2Session` and the associated `net.Socket` or
`tls.TLSSocket`.

#### http2session.destroyed
<!-- YAML
added: REPLACEME
-->

* Value: {boolean}

Will be `true` if this `Http2Session` instance has been destroyed and must no
longer be used, otherwise `false`.

#### http2session.localSettings
<!-- YAML
added: REPLACEME
-->

* Value: {[Settings Object][]}

A prototype-less object describing the current local settings of this
`Http2Session`. The local settings are local to *this* `Http2Session` instance.

#### http2session.pendingSettingsAck
<!-- YAML
added: REPLACEME
-->

* Value: {boolean}

Indicates whether or not the `Http2Session` is currently waiting for an
acknowledgement for a sent SETTINGS frame. Will be `true` after calling the
`http2session.settings()` method. Will be `false` once all sent SETTINGS
frames have been acknowledged.

#### http2session.remoteSettings
<!-- YAML
added: REPLACEME
-->

* Value: {[Settings Object][]}

A prototype-less object describing the current remote settings of this
`Http2Session`. The remote settings are set by the *connected* HTTP/2 peer.

#### http2session.request(headers[, options])
<!-- YAML
added: REPLACEME
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
  console.log(HTTP2_HEADER_STATUS);
  req.on('data', (chunk) => { /** .. **/ });
  req.on('end', () => { /** .. **/ });
});
```

#### http2session.rstStream(stream, code)
<!-- YAML
added: REPLACEME
-->

* stream {Http2Stream}
* code {number} Unsigned 32-bit integer identifying the error code. Defaults to
  `http2.constant.NGHTTP2_NO_ERROR` (`0x00`)
* Returns: {undefined}

Sends an `RST_STREAM` frame to the connected HTTP/2 peer, causing the given
`Http2Stream` to be closed on both sides using [error code][] `code`.

#### http2session.setTimeout(msecs, callback)
<!-- YAML
added: REPLACEME
-->

* `msecs` {number}
* `callback` {Function}
* Returns: {undefined}

Used to set a callback function that is called when there is no activity on
the `Http2Session` after `msecs` milliseconds. The given `callback` is
registered as a listener on the `'timeout'` event.

#### http2session.shutdown(options[, callback])
<!-- YAML
added: REPLACEME
-->

* `options` {Object}
  * `graceful` {boolean} `true` to attempt a polite shutdown of the
    `Http2Session`.
  * `errorCode` {number} The HTTP/2 [error code][] to return. Note that this is
    *not* the same thing as an HTTP Response Status Code. Defaults to `0x00`
    (No Error).
  * `lastStreamID` {number} The Stream ID of the last successfully processed
    `Http2Stream` on this `Http2Session`.
  * `opaqueData` {Buffer} A `Buffer` instance containing arbitrary additional
    data to send to the peer upon disconnection. This is used, typically, to
    provide additional data for debugging failures, if necessary.
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
added: REPLACEME
-->

* Value: {net.Socket|tls.TLSSocket}

A reference to the [`net.Socket`][] or [`tls.TLSSocket`][] to which this
`Http2Session` instance is bound.

*Note*: It is not recommended for user code to interact directly with a
`Socket` bound to an `Http2Session`. See [Http2Session and Sockets][] for
details.

#### http2session.state
<!-- YAML
added: REPLACEME
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
added: REPLACEME
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
added: REPLACEME
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
added: REPLACEME
-->

* Value: {number}

The `http2session.type` will be equal to
`http2.constants.NGHTTP2_SESSION_SERVER` if this `Http2Session` instance is a
server, and `http2.constants.NGHTTP2_SESSION_CLIENT` if the instance is a
client.

### Class: Http2Stream
<!-- YAML
added: REPLACEME
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
added: REPLACEME
-->

The `'aborted'` event is emitted whenever a `Http2Stream` instance is
abnormally aborted in mid-communication.

*Note*: The `'aborted'` event will only be emitted if the `Http2Stream`
writable side has not been ended.

#### Event: 'error'
<!-- YAML
added: REPLACEME
-->

The `'error'` event is emitted when an error occurs during the processing of
an `Http2Stream`.

#### Event: 'fetchTrailers'
<!-- YAML
added: REPLACEME
-->

The `'fetchTrailers'` event is emitted by the `Http2Stream` immediately after
queuing the last chunk of payload data to be sent. The listener callback is
passed a single object (with a `null` prototype) that the listener may used
to specify the trailing header fields to send to the peer.

```js
stream.on('fetchTrailers', (trailers) => {
  trailers['ABC'] = 'some value to send';
});
```

*Note*: The HTTP/1 specification forbids trailers from containing HTTP/2
"pseudo-header" fields (e.g. `':status'`, `':path'`, etc). An `'error'` event
will be emitted if the `'fetchTrailers'` event handler attempts to set such
header fields.

#### Event: 'frameError'
<!-- YAML
added: REPLACEME
-->

The `'frameError'` event is emitted when an error occurs while attempting to
send a frame. When invoked, the handler function will receive an integer
argument identifying the frame type, and an integer argument identifying the
error code. The `Http2Stream` instance will be destroyed immediately after the
`'frameError'` event is emitted.

#### Event: 'streamClosed'
<!-- YAML
added: REPLACEME
-->

The `'streamClosed'` event is emitted when the `Http2Stream` is destroyed. Once
this event is emitted, the `Http2Stream` instance is no longer usable.

The listener callback is passed a single argument specifying the HTTP/2 error
code specified when closing the stream. If the code is any value other than
`NGHTTP2_NO_ERROR` (`0`), an `'error'` event will also be emitted.

#### Event: 'timeout'
<!-- YAML
added: REPLACEME
-->

The `'timeout'` event is emitted after no activity is received for this
`'Http2Stream'` within the number of millseconds set using
`http2stream.setTimeout()`.

#### Event: 'trailers'
<!-- YAML
added: REPLACEME
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
added: REPLACEME
-->

* Value: {boolean}

Set to `true` if the `Http2Stream` instance was aborted abnormally. When set,
the `'aborted'` event will have been emitted.

#### http2stream.destroyed
<!-- YAML
added: REPLACEME
-->

* Value: {boolean}

Set to `true` if the `Http2Stream` instance has been destroyed and is no longer
usable.

#### http2stream.priority(options)
<!-- YAML
added: REPLACEME
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
added: REPLACEME
-->

* Value: {number}

Set to the `RST_STREAM` [error code][] reported when the `Http2Stream` is
destroyed after either receiving an `RST_STREAM` frame from the connected peer,
calling `http2stream.rstStream()`, or `http2stream.destroy()`. Will be
`undefined` if the `Http2Stream` has not been closed.

#### http2stream.rstStream(code)
<!-- YAML
added: REPLACEME
-->

* code {number} Unsigned 32-bit integer identifying the error code. Defaults to
  `http2.constant.NGHTTP2_NO_ERROR` (`0x00`)
* Returns: {undefined}

Sends an `RST_STREAM` frame to the connected HTTP/2 peer, causing this
`Http2Stream` to be closed on both sides using [error code][] `code`.

#### http2stream.rstWithNoError()
<!-- YAML
added: REPLACEME
-->

* Returns: {undefined}

Shortcut for `http2stream.rstStream()` using error code `0x00` (No Error).

#### http2stream.rstWithProtocolError() {
<!-- YAML
added: REPLACEME
-->

* Returns: {undefined}

Shortcut for `http2stream.rstStream()` using error code `0x01` (Protocol Error).

#### http2stream.rstWithCancel() {
<!-- YAML
added: REPLACEME
-->

* Returns: {undefined}

Shortcut for `http2stream.rstStream()` using error code `0x08` (Cancel).

#### http2stream.rstWithRefuse() {
<!-- YAML
added: REPLACEME
-->

* Returns: {undefined}

Shortcut for `http2stream.rstStream()` using error code `0x07` (Refused Stream).

#### http2stream.rstWithInternalError() {
<!-- YAML
added: REPLACEME
-->

* Returns: {undefined}

Shortcut for `http2stream.rstStream()` using error code `0x02` (Internal Error).

#### http2stream.session
<!-- YAML
added: REPLACEME
-->

* Value: {Http2Sesssion}

A reference to the `Http2Session` instance that owns this `Http2Stream`. The
value will be `undefined` after the `Http2Stream` instance is destroyed.

#### http2stream.setTimeout(msecs, callback)
<!-- YAML
added: REPLACEME
-->

* `msecs` {number}
* `callback` {Function}
* Returns: {undefined}

```js
const http2 = require('http2');
const client = http2.connect('http://example.org:8000');

const req = client.request({ ':path': '/' });

// Cancel the stream if there's no activity after 5 seconds
req.setTimeout(5000, () => req.rstStreamWithCancel());
```

#### http2stream.state
<!-- YAML
added: REPLACEME
-->

* Value: {Object}
  * `localWindowSize` {number}
  * `state` {number}
  * `streamLocalClose` {number}
  * `streamRemoteClose` {number}
  * `sumDependencyWeight` {number}
  * `weight` {number}

A current state of this `Http2Stream`.

### Class: ClientHttp2Stream
<!-- YAML
added: REPLACEME
-->

* Extends {Http2Stream}

The `ClientHttp2Stream` class is an extension of `Http2Stream` that is
used exclusively on HTTP/2 Clients. `Http2Stream` instances on the client
provide events such as `'response'` and `'push'` that are only relevant on
the client.

#### Event: 'headers'
<!-- YAML
added: REPLACEME
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
added: REPLACEME
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
added: REPLACEME
-->

The `'response'` event is emitted when a response `HEADERS` frame has been
received for this stream from the connected HTTP/2 server. The listener is
invoked with two arguments: an Object containing the received
[Headers Object][], and flags associated with the headers.

For example:

```js
const http2 = require('http');
const client = http2.connect('https://localhost');
const req = client.request({ ':path': '/' });
req.on('response', (headers, flags) => {
  console.log(headers[':status']);
});
```

### Class: ServerHttp2Stream
<!-- YAML
added: REPLACEME
-->

* Extends: {Http2Stream}

The `ServerHttp2Stream` class is an extension of [`Http2Stream`][] that is
used exclusively on HTTP/2 Servers. `Http2Stream` instances on the server
provide additional methods such as `http2stream.pushStream()` and
`http2stream.respond()` that are only relevant on the server.

#### http2stream.additionalHeaders(headers)
<!-- YAML
added: REPLACEME
-->

* `headers` {[Headers Object][]}
* Returns: {undefined}

Sends an additional informational `HEADERS` frame to the connected HTTP/2 peer.

#### http2stream.headersSent
<!-- YAML
added: REPLACEME
-->

* Value: {boolean}

Boolean (read-only). True if headers were sent, false otherwise.

#### http2stream.pushAllowed
<!-- YAML
added: REPLACEME
-->

* Value: {boolean}

Read-only property mapped to the `SETTINGS_ENABLE_PUSH` flag of the remote
client's most recent `SETTINGS` frame. Will be `true` if the remote peer
accepts push streams, `false` otherwise. Settings are the same for every
`Http2Stream` in the same `Http2Session`.

#### http2stream.pushStream(headers[, options], callback)
<!-- YAML
added: REPLACEME
-->

* `headers` {[Headers Object][]}
* `options` {Object}
  * `exclusive` {boolean} When `true` and `parent` identifies a parent Stream,
    the created stream is made the sole direct dependency of the parent, with
    all other existing dependents made a dependent of the newly created stream.
    Defaults to `false`.
  * `parent` {number} Specifies the numeric identifier of a stream the newly
    created stream is dependent on.
  * `weight` {number} Specifies the relative dependency of a stream in relation
    to other streams with the same `parent`. The value is a number between `1`
    and `256` (inclusive).
* `callback` {Function} Callback that is called once the push stream has been
  initiated.
* Returns: {undefined}

Initiates a push stream. The callback is invoked with the new `Htt2Stream`
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

#### http2stream.respond([headers[, options]])
<!-- YAML
added: REPLACEME
-->

* `headers` {[Headers Object][]}
* `options` {Object}
  * `endStream` {boolean} Set to `true` to indicate that the response will not
    include payload data.
* Returns: {undefined}

```js
const http2 = require('http2');
const server = http2.createServer();
server.on('stream', (stream) => {
  stream.respond({ ':status': 200 });
  stream.end('some data');
});
```

#### http2stream.respondWithFD(fd[, headers])
<!-- YAML
added: REPLACEME
-->

* `fd` {number} A readable file descriptor
* `headers` {[Headers Object][]}

Initiates a response whose data is read from the given file descriptor. No
validation is performed on the given file descriptor. If an error occurs while
attempting to read data using the file descriptor, the `Http2Stream` will be
closed using an `RST_STREAM` frame using the standard `INTERNAL_ERROR` code.

When used, the `Http2Stream` object's Duplex interface will be closed
automatically. HTTP trailer fields cannot be sent. The `'fetchTrailers'` event
will *not* be emitted.

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

#### http2stream.respondWithFile(path[, headers[, options]])
<!-- YAML
added: REPLACEME
-->

* `path` {string|Buffer|URL}
* `headers` {[Headers Object][]}
* `options` {Object}
  * `statCheck` {Function}

Sends a regular file as the response. The `path` must specify a regular file
or an `'error'` event will be emitted on the `Http2Stream` object.

When used, the `Http2Stream` object's Duplex interface will be closed
automatically. HTTP trailer fields cannot be sent. The `'fetchTrailers'` event
will *not* be emitted.

The optional `options.statCheck` function may be specified to give user code
an opportunity to set additional content headers based on the `fs.Stat` details
of the given file:

If an error occurs while attempting to read the file data, the `Http2Stream`
will be closed using an `RST_STREAM` frame using the standard `INTERNAL_ERROR`
code.

Example using a file path:

```js
const http2 = require('http2');
const server = http2.createServer();
server.on('stream', (stream) => {
  function statCheck(stat, headers) {
    headers['last-modified'] = stat.mtime.toUTCString();
  }
  stream.respondWithFile('/some/file',
                         { 'content-type': 'text/plain' },
                         { statCheck });
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

### Class: Http2Server
<!-- YAML
added: REPLACEME
-->

* Extends: {net.Server}

#### Event: 'sessionError'
<!-- YAML
added: REPLACEME
-->

The `'sessionError'` event is emitted when an `'error'` event is emitted by
an `Http2Session` object. If no listener is registered for this event, an
`'error'` event is emitted.

#### Event: 'socketError'
<!-- YAML
added: REPLACEME
-->

The `'socketError'` event is emitted when an `'error'` event is emitted by
a `Socket` associated with the server. If no listener is registered for this
event, an `'error'` event is emitted.

#### Event: 'stream'
<!-- YAML
added: REPLACEME
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

const server = http.createServer();
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

#### Event: 'timeout'
<!-- YAML
added: REPLACEME
-->

The `'timeout'` event is emitted when there is no activity on the Server for
a given number of milliseconds set using `http2server.setTimeout()`.

### Class: Http2SecureServer
<!-- YAML
added: REPLACEME
-->

* Extends: {tls.Server}

#### Event: 'sessionError'
<!-- YAML
added: REPLACEME
-->

The `'sessionError'` event is emitted when an `'error'` event is emitted by
an `Http2Session` object. If no listener is registered for this event, an
`'error'` event is emitted on the `Http2Session` instance instead.

#### Event: 'socketError'
<!-- YAML
added: REPLACEME
-->

The `'socketError'` event is emitted when an `'error'` event is emitted by
a `Socket` associated with the server. If no listener is registered for this
event, an `'error'` event is emitted on the `Socket` instance instead.

#### Event: 'unknownProtocol'
<!-- YAML
added: REPLACEME
-->

The `'unknownProtocol'` event is emitted when a connecting client fails to
negotiate an allowed protocol (i.e. HTTP/2 or HTTP/1.1). The event handler
receives the socket for handling. If no listener is registered for this event,
the connection is terminated. See the

#### Event: 'stream'
<!-- YAML
added: REPLACEME
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

const server = http.createSecureServer(options);
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

#### Event: 'timeout'
<!-- YAML
added: REPLACEME
-->

### http2.createServer(options[, onRequestHandler])
<!-- YAML
added: REPLACEME
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
added: REPLACEME
-->

* `options` {Object}
  * `allowHTTP1` {boolean} Incoming client connections that do not support
    HTTP/2 will be downgraded to HTTP/1.x when set to `true`. The default value
    is `false`. See the [`'unknownProtocol'`][] event.
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

// Create a plain-text HTTP/2 server
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
added: REPLACEME
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
added: REPLACEME
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
added: REPLACEME
-->

* Returns: {[Settings Object][]}

Returns an object containing the default settings for an `Http2Session`
instance. This method returns a new object instance every time it is called
so instances returned may be safely modified for use.

### http2.getPackedSettings(settings)
<!-- YAML
added: REPLACEME
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
added: REPLACEME
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
be repoorted using either a synchronous `throw` or via an `'error'` event on
the `Http2Stream`, `Http2Session` or HTTP/2 Server objects, depending on where
and when the error occurs.

Internal Errors occur when an HTTP/2 session fails unexpectedly. These will be
reported via an `'error'` event on the `Http2Session` or HTTP/2 Server objects.

Protocol Errors occur when various HTTP/2 protocol constraints are violated.
These will be reported using either a synchronous `throw` or via an `'error'`
event on the `Http2Stream`, `Http2Session` or HTTP/2 Server objects, depending
on where and when the error occurs.

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

req.on('response', common.mustCall());
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

TBD


[HTTP/2]: https://tools.ietf.org/html/rfc7540
[HTTP/1]: http.html
[`net.Socket`]: net.html
[`tls.TLSSocket`]: tls.html
[`tls.createServer()`]: tls.html#tls_tls_createserver_options_secureconnectionlistener
[`ClientHttp2Stream`]: #http2_class_clienthttp2stream
[Compatibility API]: #http2_compatibility_api
[`Duplex`]: stream.html#stream_class_stream_duplex
[Headers Object]: #http2_headers_object
[`Http2Stream`]: #http2_class_http2stream
[Http2Session and Sockets]: #http2_http2sesion_and_sockets
[`ServerHttp2Stream`]: #http2_class_serverhttp2stream
[Settings Object]: #http2_settings_object
[Using options.selectPadding]: #http2_using_options_selectpadding
[error code]: #error_codes
[`'unknownProtocol'`]: #http2_event_unknownprotocol

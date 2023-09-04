# WebSocket

<!--introduced_in=REPLACEME-->

> Stability: 1 - Experimental

<!--name=websocket-->

<!-- source_link=lib/websocket.js -->

The `node:websocket` library allows for the creation of RFC 6455 conformant
WebSocket client sockets and servers both secure and insecure.

This module supports, as currently written, asynchronous execution with
callbacks only.

## Callback example

### Client TLS Socket example
```javascript
const options = {
  authentication: 'auth-string',
  callbackOpen: function(err, socket) {
    console.log("client callbackOpen");
    if (err === null) {
      socket.messageSend("hello from client");
    }
  },
  masking: false,
  id: 'id-string',
  messageHandler: function(message){
    console.log("message received at client");
    console.log(message.toString());
  },
  'proxy-authorization': 'proxy-auth',
  subProtocol: 'sub-proto',
  type: 'type-string',
  secure: true,
  socketOptions: {
    host: host,
    port: port,
    rejectUnauthorized: false
  }
};
websocket.clientConnect(options);
```

### Client Net Socket example
```javascript
const options = {
  authentication: 'auth-string',
  callbackOpen: function(err, socket) {
    console.log("client callbackOpen");
    if (err === null) {
      socket.messageSend("hello from client");
    }
  },
  masking: false,
  id: 'id-string',
  messageHandler: function(message){
    console.log("message received at client");
    console.log(message.toString());
  },
  'proxy-authorization': 'proxy-auth',
  subProtocol: 'sub-proto',
  type: 'type-string',
  secure: false,
  socketOptions: {
    host: host,
    port: port
  }
};
websocket.clientConnect(options);
```

### Server TLS example
```javascript
const options = {
  callbackConnect: function(headerValues, socket, ready) {
    console.log(headerValues);
    ready();
  },
  callbackListener: function(server) {
      console.log("server callbackListener");
  },
  callbackOpen: function(err, socket) {
      console.log("server callbackOpen");
  },
  messageHandler: function(message) {
      console.log("message received at server");
      console.log(message.toString());
  },
  listenerOptions: {
      host: host,
      port: port
  },
  secure: true,
  serverOptions: {
    ca: caCertificate,
    cert: domainCertificate,
    key: domainKey
  }
};
const server = ws.server(options);
```

### Server Net example
```javascript
const options = {
  callbackConnect: function(headerValues, socket, ready) {
    console.log(headerValues);
    ready();
  },
  callbackListener: function(server) {
      console.log("server callbackListener");
  },
  callbackOpen: function(err, socket) {
      console.log("server callbackOpen");
  },
  messageHandler: function(message) {
      console.log("message received at server");
      console.log(message.toString());
  },
  listenerOptions: {
      host: host,
      port: port
  },
  secure: false,
  serverOptions: {}
};
const server = ws.server(options);
```

## API

<!-- YAML
added: REPLACEME
-->

The websocket module exists to provide primitive WebSocket protocol support as
defined by [RFC 6455](https://www.rfc-editor.org/rfc/rfc6455).

### Class: `clientConnect`

<!-- YAML
added: REPLACEME
-->

The `clientConnect` class extends class
[net.Socket](https://nodejs.org/dist/latest-v20.x/docs/api/net.html#class-netsocket)
or
[tls.TLSSocket](https://nodejs.org/dist/latest-v20.x/docs/api/tls.html#class-tlstlssocket)
to create a WebSocket socket and connects it to a WebSocket server.

* `options` {Object}
  * `callbackOpen` {Function} If present will execute upon completion of
    WebSocket connection handshake. Receives two arguments: `err`
    {NodeJS.ErrnoException} and `socket` {websocketClient}.
    **Default:** `undefined`
  * `extensions` {string} Any additional instructions, identifiers, or custom
    descriptive data.
  * `masking` {boolean} Whether to apply RFC 6455 message masking.
    **Default:** `true`
  * `messageHandler` {Function} Received messages are passed into this function
    for custom processing. When this function is absent received messages are
    discarded. Receives one argument: `message` {Buffer}.
  * `proxy-authorization` {string} Any identifier required by proxy
    authorization mechanism.
  * `secure` {boolean} Whether to create a TLS based socket or Net based
    socket. **Default:** `true`
  * `socketOptions` {Object} See
    [net.connect](https://nodejs.org/dist/latest-v20.x/docs/api/net.html#netconnect)
    or
    [tls.connect](https://nodejs.org/dist/latest-v20.x/docs/api/tls.html#tlsconnectoptions-callback).
  * `subProtocol` {string} Any one or more RFC 6455 identified sub-protocols. 
* Returns {websocketClient}

### Class: `getAddress`

<!-- YAML
added: REPLACEME
-->

A convenience utility for attaining addressing data from any network socket.

* `socket` {websocketClient}
* Returns {Object} of form:
  * `local`
    * `address` {string} An IP address.
    * `port` {number} A port.
  * `remote`
    * `address` {string} An IP address.
    * `port` {number} A port.

### `magicString`

<!-- YAML
added: REPLACEME
-->

* {string} A static string required by RFC 6455 to internally create and prove
  the connection handshake.

### Class: `server`

<!-- YAML
added: REPLACEME
-->

The `server` class extends class
[net.Server](https://nodejs.org/dist/latest-v20.x/docs/api/net.html#class-netserver)
or
[tls.Server](https://nodejs.org/dist/latest-v20.x/docs/api/tls.html#class-tlsserver)
to create a WebSocket server listening for connecting sockets. Any socket that
fails to complete the handshake within 5 seconds of connecting to the server
will be destroyed.

* `options` {Object}
  * `callbackConnect` {Function} A callback to execute when a socket connects
    to the server, but before the handshake completes. This function provides a
    means to apply authentication or additional description before completing
    the handshake and allowing messaging. Receives 3 arguments: `headerValues`
    {Object}, `socket` {websocketClient}, `ready` {Function}. The third
    argument must be called by the callbackConnect function in order for the
    handshake to complete.
  * `callbackListener` {Function} A callback that executes once the server
    starts listening for incoming socket connections. Provides 1 argument:
    `server` {Server}.
  * `callbackOpen` {Function} If present will execute upon completion of
    WebSocket connection handshake. Receives two arguments: `err`
    {NodeJS.ErrnoException} and `socket` {websocketClient}.
  * `messageHandler` {Function} Received messages are passed into this function
    for custom processing. When this function is absent received messages are
    discarded. Receives one argument: `message` {Buffer}.
  * `listenerOptions` {Object} See
    [net.server.listen](https://nodejs.org/dist/latest-v20.x/docs/api/net.html#serverlistenoptions-callback).
  * `secure` Whether to create a net.Server or a tls.TLSServer. **Default:**
    true
  * `serverOptions` {Object} See
    [net.createServer](https://nodejs.org/dist/latest-v20.x/docs/api/net.html#netcreateserveroptions-connectionlistener)
    or
    [tls.createServer](https://nodejs.org/dist/latest-v20.x/docs/api/tls.html#tlscreateserveroptions-secureconnectionlistener).

## Common Objects

### websocketClient

<!-- YAML
added: REPLACEME
-->

The `websocketClient` object inherits from either
[net.Socket](https://nodejs.org/dist/latest-v20.x/docs/api/net.html#class-netsocket)
or
[tls.TLSSocket](https://nodejs.org/dist/latest-v20.x/docs/api/tls.html#class-tlstlssocket)
with these additional object properties:

* `ping` {Function} Performs an arbitrary connection test that a user may call
  at their liberty.
* `websocket` {Object}
  * `callbackOpen` {Function} If present will execute upon completion of
    WebSocket connection handshake. Receives two arguments: `err`
    {NodeJS.ErrnoException} and `socket` {websocketClient}.
  * `extensions` {string} Any additional instructions, identifiers, or custom
    descriptive data.
  * `fragment` {Buffer} Stores complete message frames sufficient to assemble a
    complete message.
  * `frame` {Buffer} Stores pieces of a frame sufficient to assemble a complete
    frame.
  * `frameExtended` {number} Stores the extended length value of the current
    message.
  * `masking` {boolean} Determines whether to mask messages before sending them.
    Defaults to `true` for client roles and `false` for server roles, but default
    behavior can be changes with options.
  * `messageHandler` {Function} Received messages are passed into this function
    for custom processing. When this function is absent received messages are
    discarded. Receives one argument: `message` {Buffer}.
  * `pong` Stores an object with metadata sufficient to test connectivity
    initiated as a ping from the remote end.
  * `proxy-authorization` Any identifier required by proxy authorization
    mechanism.
  * `queue` {Buffer[]} Stores messages in order so that they are sent one at a
    time exactly in the order sent.
  * `role` {'client'|'server'} Whether the socket is instantiated as a client
    or server connection.
  * `status` {'closed'|'open'|'pending'} Current transfer status of the socket.
    * `closed` - Socket is not destroyed but is no longer receiving or
      transmitting.
    * `open` - Socket is available to send and receive messages.
    * `pending` - Socket can receive messages, but is halted from sending
      messages. This typically occurs because the socket is writing a message and
      others are stacked up in queue.
  * `subprotocol`: {string} Any sub-protocols defined by the client.
  * `userAgent` {string} User agent identifier populated by the client.
* `write` {Function} Sends WebSocket messages.
  * `message` {Buffer|string} The message to send.
  * `opcode` {Integer} an RFC 6455 message code. **Default:** 1 if the message is
    text or 2 if the message is a Buffer.
  * `fragmentSize` Determines the size of message fragmentation. **Default:** 0,
    which means no message fragmentation.
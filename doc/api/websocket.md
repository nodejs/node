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

* `options` {object}
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
  * `socketOptions` {object} See
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
* Returns {object} of form:
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

* `options` {object}
  * `callbackConnect` {Function} A callback to execute when a socket connects
    to the server, but before the handshake completes. This function provides a
    means to apply authentication or additional description before completing
    the handshake and allowing messaging. Receives 3 arguments: `headerValues`
    {object}, `socket` {websocketClient}, `ready` {Function}. The third
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
  * `listenerOptions` {object} See
    [net.server.listen](https://nodejs.org/dist/latest-v20.x/docs/api/net.html#serverlistenoptions-callback).
  * `secure` Whether to create a net.Server or a tls.TLSServer. **Default:**
    true
  * `serverOptions` {object} See
    [net.createServer](https://nodejs.org/dist/latest-v20.x/docs/api/net.html#netcreateserveroptions-connectionlistener)
    or
    [tls.createServer](https://nodejs.org/dist/latest-v20.x/docs/api/tls.html#tlscreateserveroptions-secureconnectionlistener).
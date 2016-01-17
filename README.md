# ws: a node.js websocket library

[![Build Status](https://travis-ci.org/websockets/ws.svg?branch=master)](https://travis-ci.org/websockets/ws)

`ws` is a simple to use WebSocket implementation, up-to-date against RFC-6455,
and [probably the fastest WebSocket library for node.js][archive].

Passes the quite extensive Autobahn test suite. See http://websockets.github.com/ws
for the full reports.

## Protocol support

* **Hixie draft 76** (Old and deprecated, but still in use by Safari and Opera.
  Added to ws version 0.4.2, but server only. Can be disabled by setting the
  `disableHixie` option to true.)
* **HyBi drafts 07-12** (Use the option `protocolVersion: 8`)
* **HyBi drafts 13-17** (Current default, alternatively option `protocolVersion: 13`)

### Installing

```
npm install --save ws
```

### Opt-in for performance

There are 2 optional modules that can be installed along side with the `ws`
module. These modules are binary addons which improve certain operations, but as
they are binary addons they require compilation which can fail if no c++
compiler is installed on the host system.

- `npm install --save bufferutil`: Improves internal buffer operations which
  allows for faster processing of masked WebSocket frames and general buffer
  operations. 
- `npm install --save utf-8-validate`: The specification requires validation of
  invalid UTF-8 chars, some of these validations could not be done in JavaScript
  hence the need for a binary addon. In most cases you will already be
  validating the input that you receive for security purposes leading to double
  validation. But if you want to be 100% spec conform and fast validation of UTF-8
  then this module is a must.

### Sending and receiving text data

```js
var WebSocket = require('ws');
var ws = new WebSocket('ws://www.host.com/path');

ws.on('open', function open() {
  ws.send('something');
});

ws.on('message', function(data, flags) {
  // flags.binary will be set if a binary data is received.
  // flags.masked will be set if the data was masked.
});
```

### Sending binary data

```js
var WebSocket = require('ws');
var ws = new WebSocket('ws://www.host.com/path');

ws.on('open', function open() {
  var array = new Float32Array(5);

  for (var i = 0; i < array.length; ++i) {
    array[i] = i / 2;
  }

  ws.send(array, { binary: true, mask: true });
});
```

Setting `mask`, as done for the send options above, will cause the data to be
masked according to the WebSocket protocol. The same option applies for text
data.

### Server example

```js
var WebSocketServer = require('ws').Server
  , wss = new WebSocketServer({ port: 8080 });

wss.on('connection', function connection(ws) {
  ws.on('message', function incoming(message) {
    console.log('received: %s', message);
  });

  ws.send('something');
});
```

### ExpressJS example

```js
var server = require('http').createServer()
  , url = require('url')
  , WebSocketServer = require('ws').Server
  , wss = new WebSocketServer({ server: server })
  , express = require('express')
  , app = express()
  , port = 4080;

app.use(function (req, res) {
  res.send({ msg: "hello" });
});

wss.on('connection', function connection(ws) {
  var location = url.parse(ws.upgradeReq.url, true);
  // you might use location.query.access_token to authenticate or share sessions
  // or ws.upgradeReq.headers.cookie (see http://stackoverflow.com/a/16395220/151312)
  
  ws.on('message', function incoming(message) {
    console.log('received: %s', message);
  });

  ws.send('something');
});

server.on('request', app);
server.listen(port, function () { console.log('Listening on ' + server.address().port) });
```

### Server sending broadcast data

```js
var WebSocketServer = require('ws').Server
  , wss = new WebSocketServer({ port: 8080 });

wss.broadcast = function broadcast(data) {
  wss.clients.forEach(function each(client) {
    client.send(data);
  });
};
```

### Error handling best practices

```js
// If the WebSocket is closed before the following send is attempted
ws.send('something');

// Errors (both immediate and async write errors) can be detected in an optional
// callback. The callback is also the only way of being notified that data has
// actually been sent.
ws.send('something', function ack(error) {
  // if error is not defined, the send has been completed,
  // otherwise the error object will indicate what failed.
});

// Immediate errors can also be handled with try/catch-blocks, but **note** that
// since sends are inherently asynchronous, socket write failures will *not* be
// captured when this technique is used.
try { ws.send('something'); }
catch (e) { /* handle error */ }
```

### echo.websocket.org demo

```js
var WebSocket = require('ws');
var ws = new WebSocket('ws://echo.websocket.org/', {
  protocolVersion: 8, 
  origin: 'http://websocket.org'
});

ws.on('open', function open() {
  console.log('connected');
  ws.send(Date.now().toString(), {mask: true});
});

ws.on('close', function close() {
  console.log('disconnected');
});

ws.on('message', function message(data, flags) {
  console.log('Roundtrip time: ' + (Date.now() - parseInt(data)) + 'ms', flags);

  setTimeout(function timeout() {
    ws.send(Date.now().toString(), {mask: true});
  }, 500);
});
```

### Browserify users
When including ws via a browserify bundle, ws returns global.WebSocket which has slightly different API. 
You should use the standard WebSockets API instead.

https://developer.mozilla.org/en-US/docs/WebSockets/Writing_WebSocket_client_applications#Availability_of_WebSockets


### Other examples

For a full example with a browser client communicating with a ws server, see the
examples folder.

Note that the usage together with Express 3.0 is quite different from Express
2.x. The difference is expressed in the two different serverstats-examples.

Otherwise, see the test cases.

### Running the tests

```
make test
```

## API Docs

See [`/doc/ws.md`](https://github.com/websockets/ws/blob/master/doc/ws.md) for Node.js-like docs for the ws classes.

## Changelog

We're using the GitHub [`releases`](https://github.com/websockets/ws/releases) for changelog entries.

## License

(The MIT License)

Copyright (c) 2011 Einar Otto Stangvik &lt;einaros@gmail.com&gt;

Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the
'Software'), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED 'AS IS', WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

[archive]: http://web.archive.org/web/20130314230536/http://hobbycoding.posterous.com/the-fastest-websocket-module-for-nodejs

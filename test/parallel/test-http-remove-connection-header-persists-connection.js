// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

'use strict';
require('../common');
const assert = require('assert');

const net = require('net');
const http = require('http');

const server = http.createServer(function(request, response) {
  // When the connection header is removed, for HTTP/1.1 the connection should still persist.
  // For HTTP/1.0, the connection should be closed after the response automatically.
  response.removeHeader('connection');

  response.end('beep boop\n');
});

const agent = new http.Agent({ keepAlive: true });

function makeHttp11Request(cb) {
  http.get({
    port: server.address().port,
    agent
  }, function(res) {
    const socket = res.socket;

    assert.strictEqual(res.statusCode, 200);
    assert.strictEqual(res.headers.connection, undefined);

    res.setEncoding('ascii');
    let response = '';
    res.on('data', function(chunk) {
      response += chunk;
    });
    res.on('end', function() {
      assert.strictEqual(response, 'beep boop\n');

      // Wait till next tick to ensure that the socket is returned to the agent before
      // we continue to the next request
      process.nextTick(function() {
        cb(socket);
      });
    });
  });
}

function makeHttp10Request(cb) {
  // We have to manually make HTTP/1.0 requests since Node does not allow sending them:
  const socket = net.connect({ port: server.address().port }, function() {
    socket.write('GET / HTTP/1.0\r\n' +
               'Host: localhost:' + server.address().port + '\r\n' +
                '\r\n');
    socket.resume(); // Ignore the response itself

    setTimeout(function() {
      cb(socket);
    }, 10);
  });
}

server.listen(0, function() {
  makeHttp11Request(function(firstSocket) {
    makeHttp11Request(function(secondSocket) {
      // Both HTTP/1.1 requests should have used the same socket:
      assert.strictEqual(firstSocket, secondSocket);

      makeHttp10Request(function(socket) {
        // The server should have immediately closed the HTTP/1.0 socket:
        assert.strictEqual(socket.closed, true);
        server.close();
      });
    });
  });
});

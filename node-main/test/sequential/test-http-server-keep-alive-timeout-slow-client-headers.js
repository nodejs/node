'use strict';

const common = require('../common');
const assert = require('assert');
const http = require('http');
const net = require('net');

const server = http.createServer(common.mustCall((req, res) => {
  res.end();
}, 2));

server.keepAliveTimeout = common.platformTimeout(100);

server.listen(0, common.mustCall(() => {
  const port = server.address().port;
  const socket = net.connect({ port }, common.mustCall(() => {
    request(common.mustCall(() => {
      // Make a second request on the same socket, after the keep-alive timeout
      // has been set on the server side.
      request(common.mustCall());
    }));
  }));

  server.on('timeout', common.mustCall(() => {
    socket.end();
    server.close();
  }));

  function request(callback) {
    socket.setEncoding('utf8');
    socket.on('data', onData);
    let response = '';

    // Simulate a client that sends headers slowly (with a period of inactivity
    // that is longer than the keep-alive timeout).
    socket.write('GET / HTTP/1.1\r\n' +
                 `Host: localhost:${port}\r\n`);
    setTimeout(() => {
      socket.write('Connection: keep-alive\r\n' +
                   '\r\n');
    }, common.platformTimeout(300));

    function onData(chunk) {
      response += chunk;
      if (chunk.includes('\r\n')) {
        socket.removeListener('data', onData);
        onHeaders();
      }
    }

    function onHeaders() {
      assert.ok(response.includes('HTTP/1.1 200 OK\r\n'));
      assert.ok(response.includes('Connection: keep-alive\r\n'));
      callback();
    }
  }
}));

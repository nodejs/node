'use strict';

const common = require('../common');
const http = require('http');
const net = require('net');
const assert = require('assert');

function request(socket) {
  socket.write('GET / HTTP/1.1\r\n');
  socket.write('Connection: keep-alive\r\n');
  socket.write('Host: localhost\r\n');
  socket.write('\r\n\r\n');
}

const server = http.createServer(common.mustCall((req, res) => {
  res.end('ok');
}));

server.on('dropRequest', common.mustCall((request, socket) => {
  assert.strictEqual(request instanceof http.IncomingMessage, true);
  assert.strictEqual(socket instanceof net.Socket, true);
  server.close();
}));

server.listen(0, common.mustCall(() => {
  const socket = net.connect(server.address().port);
  let response = '';
  socket.on('connect', common.mustCall(() => {
    request(socket);
    request(socket);
  }));
  socket.on('data', common.mustCallAtLeast((chunk) => {
    response += chunk;
    if (response.includes('HTTP/1.1 503 Service Unavailable'))
      socket.end();
  }));
  socket.on('close', common.mustCall(() => {
    assert.match(response, /HTTP\/1\.1 200 OK/);
    assert.match(response, /HTTP\/1\.1 503 Service Unavailable/);
  }));
}));

server.maxRequestsPerSocket = 1;

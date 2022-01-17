'use strict';

const common = require('../common');
const http = require('http');
const net = require('net');
const assert = require('assert');

// Verify that invalid chunk extensions cannot be used to perform HTTP request
// smuggling attacks.

const server = http.createServer(common.mustCall((request, response) => {
  assert.notStrictEqual(request.url, '/admin');
  response.end('hello world');
}), 1);

server.listen(0, common.mustCall(start));

function start() {
  const sock = net.connect(server.address().port);

  sock.write('' +
    'GET / HTTP/1.1\r\n' +
    'Host: localhost:8080\r\n' +
    'Transfer-Encoding: chunked\r\n' +
    '\r\n' +
    '2;\n' +
    'xx\r\n' +
    '4c\r\n' +
    '0\r\n' +
    '\r\n' +
    'GET /admin HTTP/1.1\r\n' +
    'Host: localhost:8080\r\n' +
    'Transfer-Encoding: chunked\r\n' +
    '\r\n' +
    '0\r\n' +
    '\r\n'
  );

  sock.resume();
  sock.on('end', common.mustCall(function() {
    server.close();
  }));
}

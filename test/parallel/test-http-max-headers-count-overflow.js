'use strict';
const common = require('../common');
const assert = require('assert');
const http = require('http');
const net = require('net');

const server = http.createServer(common.mustNotCall());

server.maxHeadersCount = 2;

server.on('clientError', common.mustCall((err, socket) => {
  assert.strictEqual(err.code, 'HPE_HEADER_OVERFLOW');
  socket.end('HTTP/1.1 431 Request Header Fields Too Large\r\n\r\n');
}));

server.listen(0, common.mustCall(() => {
  const port = server.address().port;
  const req = 'POST / HTTP/1.1\r\n' +
              'Host: localhost\r\n' +
              'X-A: b\r\n' +
              'Content-Length: 3\r\n' +
              '\r\nabc';

  net.createConnection(port, 'localhost', common.mustCall(function() {
    let response = '';
    this.setEncoding('latin1');
    this.end(req);
    this.on('data', (chunk) => response += chunk);
    this.on('end', common.mustCall(() => {
      assert.match(response, /^HTTP\/1\.1 431 /);
      server.close();
    }));
  }));
}));

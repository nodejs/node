'use strict';
const common = require('../common');
const assert = require('assert');
const http = require('http');
const net = require('net');

const requestServer = http.createServer(common.mustNotCall());

requestServer.maxHeadersCount = 2;

requestServer.on('clientError', common.mustCall((err, socket) => {
  assert.strictEqual(err.code, 'HPE_HEADER_OVERFLOW');
  socket.end('HTTP/1.1 431 Request Header Fields Too Large\r\n\r\n');
}));

requestServer.listen(0, common.mustCall(() => {
  const port = requestServer.address().port;
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
      requestServer.close();
    }));
  }));
}));

const responseServer = net.createServer(common.mustCall((socket) => {
  socket.once('data', common.mustCall(() => {
    socket.end('HTTP/1.1 200 OK\r\n' +
               'X-A: a\r\n' +
               'X-B: b\r\n' +
               'Content-Length: 0\r\n\r\n');
  }));
}));

responseServer.listen(0, common.mustCall(() => {
  const req = http.request({
    port: responseServer.address().port,
  }, common.mustNotCall());
  req.maxHeadersCount = 2;
  req.on('error', common.mustCall((err) => {
    assert.strictEqual(err.code, 'HPE_HEADER_OVERFLOW');
    responseServer.close();
  }));
  req.end();
}));

const defaultResponseServer = net.createServer(common.mustCall((socket) => {
  socket.once('data', common.mustCall(() => {
    socket.end('HTTP/1.1 200 OK\r\n' +
               'X: a\r\n'.repeat(1000) +
               'Content-Length: 0\r\n\r\n');
  }));
}));

defaultResponseServer.listen(0, common.mustCall(() => {
  const req = http.request({
    port: defaultResponseServer.address().port,
  }, common.mustNotCall());
  req.on('error', common.mustCall((err) => {
    assert.strictEqual(err.code, 'HPE_HEADER_OVERFLOW');
    defaultResponseServer.close();
  }));
  req.end();
}));

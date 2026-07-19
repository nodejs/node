'use strict';

const common = require('../common');
const assert = require('assert');
const http = require('http');
const net = require('net');

// Regression test: sending a __proto__ header must not crash the server
// when accessing req.headersDistinct or req.trailersDistinct.

const server = http.createServer(common.mustCall((req, res) => {
  const headers = req.headersDistinct;
  assert.strictEqual(Object.getPrototypeOf(headers), null);
  assert.deepStrictEqual(Object.getOwnPropertyDescriptor(headers, '__proto__').value, ['test']);
  res.end();
}));

server.listen(0, common.mustCall(() => {
  const port = server.address().port;

  const client = net.connect(port, common.mustCall(() => {
    client.write(
      'GET / HTTP/1.1\r\n' +
      'Host: localhost\r\n' +
      '__proto__: test\r\n' +
      'Connection: close\r\n' +
      '\r\n',
    );
  }));

  client.on('end', common.mustCall(() => {
    server.close();
  }));

  client.resume();
}));

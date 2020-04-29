'use strict';

const common = require('../common');
const assert = require('assert');
const { createServer } = require('http');
const { connect } = require('net');

// This test verifies that it is possible to disable
// headersTimeout by setting it to zero.

const server = createServer(common.mustCall((req, res) => {
  res.writeHead(200);
  res.end('OK');
}));

server.headersTimeout = 0;

server.once('timeout', common.mustNotCall((socket) => {
  socket.destroy();
}));

server.listen(0, common.mustCall(() => {
  const client = connect(server.address().port);
  let response = '';

  client.resume();
  client.write('GET / HTTP/1.1\r\nConnection: close\r\n');

  // All the timeouts below must be greater than a second, otherwise
  // headersTimeout won't be triggered anyway as the current date is cached
  // for a second in HTTP internals.
  setTimeout(() => {
    client.write('X-Crash: Ab: 456\r\n');
  }, common.platformTimeout(1100)).unref();

  setTimeout(() => {
    client.write('\r\n');
  }, common.platformTimeout(1200)).unref();

  client.on('data', (chunk) => {
    response += chunk.toString('utf-8');
  });

  client.on('end', common.mustCall(() => {
    assert.strictEqual(response.split('\r\n').shift(), 'HTTP/1.1 200 OK');
    client.end();
    server.close();
  }));
}));

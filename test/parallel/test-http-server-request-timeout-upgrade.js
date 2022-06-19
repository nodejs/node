'use strict';

const common = require('../common');
const assert = require('assert');
const { createServer } = require('http');
const { connect } = require('net');

// This test validates that the requestTimeoout
// is disabled after the connection is upgraded.
let sendDelayedRequestHeaders;
const requestTimeout = common.platformTimeout(2000);
const server = createServer({
  headersTimeout: 0,
  requestTimeout,
  keepAliveTimeout: 0,
  connectionsCheckingInterval: requestTimeout / 4
}, common.mustNotCall());
server.on('connection', common.mustCall(() => {
  assert.strictEqual(typeof sendDelayedRequestHeaders, 'function');
  sendDelayedRequestHeaders();
}));

assert.strictEqual(server.requestTimeout, requestTimeout);

server.on('upgrade', common.mustCall((req, socket, head) => {
  socket.write('HTTP/1.1 101 Web Socket Protocol Handshake\r\n');
  socket.write('Upgrade: WebSocket\r\n');
  socket.write('Connection: Upgrade\r\n\r\n');
  socket.pipe(socket);
}));

server.listen(0, common.mustCall(() => {
  const client = connect(server.address().port);
  let response = '';

  client.on('data', common.mustCallAtLeast((chunk) => {
    response += chunk.toString('utf-8');
  }, 1));

  client.on('end', common.mustCall(() => {
    assert.strictEqual(
      response,
      'HTTP/1.1 101 Web Socket Protocol Handshake\r\n' +
      'Upgrade: WebSocket\r\n' +
      'Connection: Upgrade\r\n\r\n' +
      '12345678901234567890'
    );

    server.close();
  }));

  client.resume();
  client.write('GET / HTTP/1.1\r\n');
  client.write('Upgrade: WebSocket\r\n');
  client.write('Connection: Upgrade\r\n\r\n');

  sendDelayedRequestHeaders = common.mustCall(() => {
    setTimeout(() => {
      client.write('12345678901234567890');
      client.end();
    }, requestTimeout * 2).unref();
  });
}));

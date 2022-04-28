'use strict';

const common = require('../common');
const assert = require('assert');
const { createServer } = require('http');
const { connect } = require('net');

// This test validates that the server returns 408
// after server.headersTimeout if the client
// pauses sending in the middle of a header.

let sendDelayedRequestHeaders;
const headersTimeout = common.platformTimeout(2000);
const server = createServer({
  headersTimeout,
  requestTimeout: 0,
  keepAliveTimeout: 0,
  connectionsCheckingInterval: headersTimeout / 4,
}, common.mustNotCall());
server.on('connection', common.mustCall(() => {
  assert.strictEqual(typeof sendDelayedRequestHeaders, 'function');
  sendDelayedRequestHeaders();
}));

assert.strictEqual(server.headersTimeout, headersTimeout);

// Check that timeout event is not triggered
server.once('timeout', common.mustNotCall((socket) => {
  socket.destroy();
}));

server.listen(0, common.mustCall(() => {
  const client = connect(server.address().port);
  let response = '';

  client.on('data', common.mustCall((chunk) => {
    response += chunk.toString('utf-8');
  }));

  const errOrEnd = common.mustSucceed(function(err) {
    assert.strictEqual(
      response,
      'HTTP/1.1 408 Request Timeout\r\nConnection: close\r\n\r\n'
    );
    server.close();
  });

  client.on('end', errOrEnd);
  client.on('error', errOrEnd);

  client.resume();
  client.write('GET / HTTP/1.1\r\n');
  client.write('Connection: close\r\n');
  client.write('X-CRASH: ');

  sendDelayedRequestHeaders = common.mustCall(() => {
    setTimeout(() => {
      client.write('1234567890\r\n\r\n');
    }, common.platformTimeout(headersTimeout * 2)).unref();
  });
}));

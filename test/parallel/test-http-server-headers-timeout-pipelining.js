'use strict';

const common = require('../common');
const assert = require('assert');
const { createServer } = require('http');
const { connect } = require('net');

// This test validates that the server returns 408
// after server.requestTimeout if the client
// does not complete a request when using pipelining.

const headersTimeout = common.platformTimeout(2000);
const server = createServer({
  headersTimeout,
  requestTimeout: 0,
  keepAliveTimeout: 0,
  connectionsCheckingInterval: headersTimeout / 4,
}, common.mustCallAtLeast((req, res) => {
  res.writeHead(200, { 'Content-Type': 'text/plain' });
  res.end();
}));

// 0 seconds is the default
assert.strictEqual(server.headersTimeout, headersTimeout);

// Make sure requestTimeout and keepAliveTimeout
// are big enough for the headersTimeout.
server.requestTimeout = 0;
server.keepAliveTimeout = 0;

server.listen(0, common.mustCall(() => {
  const client = connect(server.address().port);
  let second = false;
  let response = '';

  client.on('data', common.mustCallAtLeast((chunk) => {
    response += chunk.toString('utf-8');

    // First response has ended
    if (!second && response.endsWith('\r\n\r\n')) {
      assert.strictEqual(
        response.split('\r\n')[0],
        'HTTP/1.1 200 OK'
      );

      response = '';
      second = true;
    }
  }, 1));

  const errOrEnd = common.mustCall(function(err) {
    if (!second) {
      return;
    }

    assert.strictEqual(
      response,
      // Empty because of https://github.com/nodejs/node/commit/e8d7fedf7cad6e612e4f2e0456e359af57608ac7
      // 'HTTP/1.1 408 Request Timeout\r\nConnection: close\r\n\r\n'
      ''
    );
    server.close();
  });

  client.on('error', errOrEnd);
  client.on('end', errOrEnd);

  // Send two requests using pipelining. Delay before finishing the second one
  client.resume();
  client.write('GET / HTTP/1.1\r\nConnection: keep-alive\r\n\r\nGET / HTTP/1.1\r\nConnection: ');

  // Complete the request
  setTimeout(() => {
    client.write('close\r\n\r\n');
  }, headersTimeout * 1.5).unref();
}));

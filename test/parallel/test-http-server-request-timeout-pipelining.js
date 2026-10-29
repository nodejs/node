'use strict';

const common = require('../common');
const assert = require('assert');
const { createServer } = require('http');
const { connect } = require('net');

// This test validates that the server returns 408
// after server.requestTimeout if the client
// does not complete a request when using pipelining.

const requestTimeout = common.platformTimeout(2000);
const server = createServer({
  headersTimeout: 0,
  requestTimeout,
  keepAliveTimeout: 0,
  connectionsCheckingInterval: requestTimeout / 4
}, common.mustCallAtLeast((req, res) => {
  res.writeHead(200, { 'Content-Type': 'text/plain' });
  res.end();
}));

assert.strictEqual(server.requestTimeout, requestTimeout);

server.listen(0, common.mustCall(() => {
  const client = connect(server.address().port);
  let second = false;
  let response = '';

  client.setEncoding('utf8');
  client.on('data', common.mustCallAtLeast((chunk) => {
    response += chunk;

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
      'HTTP/1.1 408 Request Timeout\r\nConnection: close\r\n\r\n'
    );
    server.close();
  });

  client.on('error', errOrEnd);
  client.on('end', errOrEnd);

  // Send two requests using pipelining. Delay before finishing the second one
  client.resume();
  client.write('GET / HTTP/1.1\r\nHost: localhost\r\nConnection: keep-alive\r\n\r\n' +
   'GET / HTTP/1.1\r\nHost: localhost\r\nConnection: ');

  // Complete the request
  setTimeout(() => {
    client.write('close\r\n\r\n');
  }, requestTimeout * 2).unref();
}));

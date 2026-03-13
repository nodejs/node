'use strict';

const common = require('../common');
const assert = require('assert');
const { createServer } = require('http');
const { connect } = require('net');

// This test validates that the server returns 408
// after server.requestTimeout if the client
// does not complete a request, and that keep alive
// works properly.

function performRequestWithDelay(client, firstDelay, secondDelay, closeAfter) {
  client.resume();
  client.write('GET / HTTP/1.1\r\nHost: example.com\r\n');

  setTimeout(() => {
    client.write('Connection: ');
  }, firstDelay).unref();

  // Complete the request
  setTimeout(() => {
    client.write(`${closeAfter ? 'close' : 'keep-alive'}\r\n\r\n`);
  }, firstDelay + secondDelay).unref();
}

const requestTimeout = common.platformTimeout(5000);
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

// Make sure keepAliveTimeout is big enough for the requestTimeout.
server.keepAliveTimeout = 0;

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

      const defer = requestTimeout * 1.5;

      // Wait some time to make sure requestTimeout
      // does not interfere with keep alive
      setTimeout(() => {
        response = '';
        second = true;

        // Perform a second request expected to finish after requestTimeout
        performRequestWithDelay(
          client,
          requestTimeout / 5,
          requestTimeout * 2,
          true
        );
      }, defer).unref();
    }
  }, 1));

  const errOrEnd = common.mustCall(function(err) {
    assert.strictEqual(second, true);
    assert.strictEqual(
      response,
      'HTTP/1.1 408 Request Timeout\r\nConnection: close\r\n\r\n'
    );
    server.close();
  });

  client.on('error', errOrEnd);
  client.on('end', errOrEnd);

  // Perform a first request which is completed immediately
  performRequestWithDelay(
    client,
    requestTimeout / 5,
    requestTimeout / 5,
    false
  );
}));

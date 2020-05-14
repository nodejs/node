'use strict';

const common = require('../common');
const assert = require('assert');
const { createServer } = require('http');
const { connect } = require('net');

// This test validates that the server returns 408
// after server.requestTimeout if the client
// does not complete a request, and that keep alive
// works properly

function performRequestWithDelay(client, firstDelay, secondDelay) {
  client.resume();
  client.write('GET / HTTP/1.1\r\n');

  firstDelay = common.platformTimeout(firstDelay);
  secondDelay = common.platformTimeout(secondDelay);

  console.log('performRequestWithDelay', firstDelay, secondDelay);

  setTimeout(() => {
    client.write('Connection: ');
  }, firstDelay).unref();

  // Complete the request
  setTimeout(() => {
    client.write('keep-alive\r\n\r\n');
  }, firstDelay + secondDelay).unref();
}

const server = createServer(common.mustCallAtLeast((req, res) => {
  res.writeHead(200, { 'Content-Type': 'text/plain' });
  res.end();
}));

// 0 seconds is the default
assert.strictEqual(server.requestTimeout, 0);
const requestTimeout = common.platformTimeout(1000);
server.requestTimeout = requestTimeout;
assert.strictEqual(server.requestTimeout, requestTimeout);

// Make sure keepAliveTimeout is big enough for the requestTimeout.
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

      const defer = common.platformTimeout(server.requestTimeout * 1.5);

      console.log('defer by', defer);

      // Wait some time to make sure requestTimeout
      // does not interfere with keep alive
      setTimeout(() => {
        response = '';
        second = true;

        // Perform a second request expected to finish after requestTimeout
        performRequestWithDelay(client, 1000, 3000);
      }, defer).unref();
    }
  }, 1));

  const errOrEnd = common.mustCall(function(err) {
    console.log(err);
    assert.strictEqual(second, true);
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

  // Perform a second request expected to finish before requestTimeout
  performRequestWithDelay(client, 50, 500);
}));

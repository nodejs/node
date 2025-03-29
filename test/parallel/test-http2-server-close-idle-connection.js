'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const assert = require('assert');
const http2 = require('http2');

// This test verifies that the server closes idle connections

// Track test state with flags
const states = {
  serverListening: false,
  initialRequestCompleted: false,
  connectionBecameIdle: false,
  serverClosedIdleConnection: false
};

const server = http2.createServer();

// Track server events
server.on('stream', common.mustCall((stream, headers) => {
  assert.strictEqual(states.serverListening, true);

  // Respond to the request with a small payload
  stream.respond({
    'content-type': 'text/plain',
    ':status': 200
  });
  stream.end('hello');

  // After the request is done, the connection should become idle
  stream.on('close', () => {
    states.initialRequestCompleted = true;
    // Mark connection as idle after the request completes
    states.connectionBecameIdle = true;
  });
}));

// Track session closure events
server.on('session', common.mustCall((session) => {
  session.on('stream', common.mustCall((stream) => {
    stream.on('close', common.mustCall(() => {
      // This should only happen after the connection became idle
      assert.strictEqual(states.connectionBecameIdle, true);
      states.serverClosedIdleConnection = true;

      // Test is successful, close the server
      server.close(common.mustCall(() => {
        console.log('server closed');
      }));
    }));
  }));
  session.on('close', common.mustCall(() => {
    console.log('session closed');
  }));
}));

// Start the server
server.listen(0, common.mustCall(() => {
  states.serverListening = true;

  // Create client and initial request
  const client = http2.connect(`http://localhost:${server.address().port}`);

  // Track client session events
  client.on('close', common.mustCall(() => {
    // Verify server closed the connection after it became idle
    assert.strictEqual(states.serverClosedIdleConnection, true);
  }));

  // Make an initial request
  const req = client.request({ ':path': '/' });

  req.on('response', common.mustCall());

  // Process the response data
  req.setEncoding('utf8');
  let data = '';
  req.on('data', (chunk) => {
    data += chunk;
  });

  // When initial request ends
  req.on('end', common.mustCall(() => {
    assert.strictEqual(data, 'hello');

    // Don't explicitly close the client - keep it idle
    // Wait for the server to close the idle connection
  }));

  client.on('close', common.mustCall());

  req.end();
}));

// Final verification on exit
process.on('exit', () => {
  assert.strictEqual(states.serverClosedIdleConnection, true);
});

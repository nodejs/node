'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const assert = require('assert');
const http2 = require('http2');

// This test verifies that the server waits for active connections/sessions
// to complete and all data to be sent before fully closing

// Keep track of the test flow with these flags
const states = {
  serverListening: false,
  responseStarted: false,
  dataFullySent: false,
  streamClosed: false,
  serverClosed: false
};

// Create a server that will send a large response in chunks
const server = http2.createServer();

// Track server events
server.on('stream', common.mustCall((stream, headers) => {
  assert.strictEqual(states.serverListening, true);

  // Setup the response
  stream.respond({
    'content-type': 'text/plain',
    ':status': 200
  });

  // Initiate the server close before client data is sent, this will
  // test if the server properly waits for the stream to finish
  server.close(common.mustCall(() => {
    // Stream should be closed before server close callback
    // Should be called only after the stream has closed
    assert.strictEqual(states.streamClosed, true);
    states.serverClosed = true;
  }));

  // Mark response as started
  states.responseStarted = true;

  // Create a large response (1MB) to ensure it takes some time to send
  const chunk = Buffer.alloc(64 * 1024, 'x');

  // Send 16 chunks (1MB total) to simulate a large response
  for (let i = 0; i < 16; i++) {
    stream.write(chunk);
  }


  // Stream end should happen after data is written
  stream.end();
  states.dataFullySent = true;

  // When stream closes, we can verify order of events
  stream.on('close', common.mustCall(() => {
    // Data should be fully sent before stream closes
    assert.strictEqual(states.dataFullySent, true);
    states.streamClosed = true;
  }));
}));

// Start the server
server.listen(0, common.mustCall(() => {
  states.serverListening = true;

  // Create client and request
  const client = http2.connect(`http://localhost:${server.address().port}`);
  const req = client.request({ ':path': '/' });

  // Track received data
  let receivedData = 0;
  req.on('data', (chunk) => {
    receivedData += chunk.length;
  });

  // When the request completes
  req.on('end', common.mustCall(() => {
    // All data should be sent before request ends
    assert.strictEqual(states.dataFullySent, true);
  }));

  // When request closes
  req.on('close', common.mustCall(() => {
    // Check final state
    assert.strictEqual(states.streamClosed, true);
    // Should receive all data
    assert.strictEqual(receivedData, 64 * 1024 * 16);

    // Wait for the server close confirmation
    process.on('exit', () => {
      // Server should be fully closed
      assert.strictEqual(states.serverClosed, true);
    });
  }));

  // Start the request
  req.end();
}));

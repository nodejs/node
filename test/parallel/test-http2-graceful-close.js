'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const assert = require('assert');
const http2 = require('http2');

// This test verifies that the server waits for active connections/sessions
// to complete and all data to be sent before fully closing

// Create a server that will send a large response in chunks
const server = http2.createServer();

// Track server events
server.on('stream', common.mustCall((stream, headers) => {

  // Initiate the server close before client data is sent, this will
  // test if the server properly waits for the stream to finish
  server.close();
  setImmediate(() => {
    stream.respond({
      'content-type': 'text/plain',
      ':status': 200
    });

    // Create a large response (1MB) to ensure it takes some time to send
    const chunk = Buffer.alloc(64 * 1024, 'x');

    // Send 16 chunks (1MB total) to simulate a large response
    for (let i = 0; i < 16; i++) {
      stream.write(chunk);
    }

    // Stream end should happen after data is written
    stream.end();
  });

  stream.on('close', common.mustCall(() => {
    assert.strictEqual(stream.readableEnded, true);
    assert.strictEqual(stream.writableFinished, true);
  }));
}));

// Start the server
server.listen(0, common.mustCall(() => {
  // Create client and request
  const client = http2.connect(`http://localhost:${server.address().port}`);
  const req = client.request({ ':path': '/' });

  // Track received data
  let receivedData = 0;
  req.on('data', (chunk) => {
    receivedData += chunk.length;
  });

  // When request closes
  req.on('close', common.mustCall(() => {
    // Should receive all data
    assert.strictEqual(req.readableEnded, true);
    assert.strictEqual(receivedData, 64 * 1024 * 16);
    assert.strictEqual(req.writableFinished, true);
  }));

  // Start the request
  req.end();
}));

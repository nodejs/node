'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const assert = require('assert');
const http2 = require('http2');

// This test verifies that the server closes idle connections

const server = http2.createServer();

server.on('session', common.mustCall((session) => {
  session.on('stream', common.mustCall((stream) => {
    // Respond to the request with a small payload
    stream.respond({
      'content-type': 'text/plain',
      ':status': 200
    });
    stream.end('hello');
    stream.on('close', common.mustCall(() => {
      assert.strictEqual(stream.writableFinished, true);
    }));
  }));
  session.on('close', common.mustCall());
}));

// Start the server
server.listen(0, common.mustCall(() => {
  // Create client and initial request
  const client = http2.connect(`http://localhost:${server.address().port}`);

  // This will ensure that server closed the idle connection
  client.on('close', common.mustCall());

  // Make an initial request
  const req = client.request({ ':path': '/' });

  // Process the response data
  req.setEncoding('utf8');
  let data = '';
  req.on('data', (chunk) => {
    data += chunk;
  });

  // When initial request ends
  req.on('end', common.mustCall(() => {
    assert.strictEqual(data, 'hello');
    // Close the server as client is idle now
    setImmediate(() => {
      server.close();
    });
  }));

  // Don't explicitly close the client, we want to keep it
  // idle and check if the server closes the idle connection.
  // As this is what we want to test here.
}));

'use strict';
const common = require('../common');
const assert = require('assert');
const http = require('http');

// Verify that after calling end() on an `OutgoingMessage` (or a type that
// inherits from `OutgoingMessage`), its `writable` property is not set to false

const server = http.createServer(common.mustCall(function(req, res) {
  assert.strictEqual(res.writable, true);
  assert.strictEqual(res.finished, false);
  res.end();

  // res.writable is set to false after it has finished sending
  // Ref: https://github.com/nodejs/node/issues/15029
  assert.strictEqual(res.writable, true);
  assert.strictEqual(res.finished, true);

  server.close();
}));

server.listen(0);

server.on('listening', common.mustCall(function() {
  const clientRequest = http.request({
    port: server.address().port,
    method: 'GET',
    path: '/'
  });

  assert.strictEqual(clientRequest.writable, true);
  clientRequest.end();

  // writable is still true when close
  // THIS IS LEGACY, we cannot change it
  // unless we break error detection
  assert.strictEqual(clientRequest.writable, true);
}));

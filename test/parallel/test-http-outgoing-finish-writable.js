'use strict';
const common = require('../common');
const assert = require('assert');
const http = require('http');

// Verify that after calling end() on an `OutgoingMessage` (or a type that
// inherits from `OutgoingMessage`), its `writable` property is set to false.

const server = http.createServer(common.mustCall(function(req, res) {
  assert.strictEqual(res.writable, true);
  assert.strictEqual(res.finished, false);
  res.end();
  assert.strictEqual(res.writable, false);
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
  assert.strictEqual(clientRequest.writable, false);
}));

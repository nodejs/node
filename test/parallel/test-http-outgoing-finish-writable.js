'use strict';
const common = require('../common');
const assert = require('assert');
const http = require('http');

const server = http.createServer(common.mustCall(function(req, res) {
  assert.strictEqual(res.writable, true);
  assert.strictEqual(res.finished, false);
  assert.strictEqual(res.writableEnded, false);
  res.end();

  // res.writable is set to false after it has finished sending
  // Ref: https://github.com/nodejs/node/issues/15029
  assert.strictEqual(res.writable, false);
  assert.strictEqual(res.finished, true);
  assert.strictEqual(res.writableEnded, true);

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

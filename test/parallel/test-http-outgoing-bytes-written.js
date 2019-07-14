'use strict';
const common = require('../common');
const assert = require('assert');
const http = require('http');

const server = http.createServer(common.mustCall(function(req, res) {
  res.setHeader('test-header', 'header');
  assert.strictEqual(res.bytesWritten, 0);
  res.write('hello');
  assert.strictEqual(res.bytesWritten, 5);
  res.end('world');
  assert.strictEqual(res.bytesWritten, 10);
  server.close();
}));

server.listen(0);

server.on('listening', common.mustCall(function() {
  const clientRequest = http.request({
    port: server.address().port,
    method: 'GET',
    path: '/'
  });
  clientRequest.end();
}));

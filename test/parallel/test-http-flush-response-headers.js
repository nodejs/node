'use strict';
require('../common');
const assert = require('assert');
const http = require('http');

const server = http.createServer();

server.on('request', function(req, res) {
  res.writeHead(200, { 'foo': 'bar' });
  res.flushHeaders();
  res.flushHeaders(); // Should be idempotent.
});
server.listen(0, '127.0.0.1', function() {
  const req = http.request({
    method: 'GET',
    host: '127.0.0.1',
    port: this.address().port,
  }, onResponse);

  req.end();

  function onResponse(res) {
    assert.strictEqual(res.headers.foo, 'bar');
    res.destroy();
    server.close();
  }
});

'use strict';
const common = require('../common');
const assert = require('assert');
const http = require('http');

const server = http.createServer();
server.on('request', common.mustCall(function(req, res) {
  assert.strictEqual(req.headers.foo, 'bar');
  res.end('ok');
  server.close();
}));
server.listen(0, '127.0.0.1', common.mustCall(function() {
  const req = http.request({
    method: 'GET',
    host: '127.0.0.1',
    port: this.address().port,
  });
  req.setHeader('foo', 'bar');
  req.flushHeaders();
}));

'use strict';
const common = require('../common');
const assert = require('assert');
const http = require('http');

const server = http.createServer();

server.on('request', function(req, res) {
  res.writeHead(200, {'foo': 'bar'});
  res.flushHeaders();
  res.flushHeaders(); // Should be idempotent.
});
server.listen(common.PORT, common.localhostIPv4, function() {
  var req = http.request({
    method: 'GET',
    host: common.localhostIPv4,
    port: common.PORT,
  }, onResponse);

  req.end();

  function onResponse(res) {
    assert.equal(res.headers['foo'], 'bar');
    res.destroy();
    server.close();
  }
});

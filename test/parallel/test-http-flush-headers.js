'use strict';
const common = require('../common');
const assert = require('assert');
const http = require('http');

const server = http.createServer();
server.on('request', function(req, res) {
  assert(req.headers['foo'], 'bar');
  res.end('ok');
  server.close();
});
server.listen(common.PORT, '127.0.0.1', function() {
  let req = http.request({
    method: 'GET',
    host: '127.0.0.1',
    port: common.PORT,
  });
  req.setHeader('foo', 'bar');
  req.flushHeaders();
});

'use strict';
require('../common');
const assert = require('assert');
const http = require('http');

const server = http.createServer(function(req, res) {
  res.setHeader('X-Date', 'foo');
  res.setHeader('X-Connection', 'bar');
  res.setHeader('X-Content-Length', 'baz');
  res.end();
});
server.listen(0);

server.on('listening', function() {
  const agent = new http.Agent({ port: this.address().port, maxSockets: 1 });
  http.get({
    port: this.address().port,
    path: '/hello',
    agent: agent
  }, function(res) {
    assert.equal(res.statusCode, 200);
    assert.equal(res.headers['x-date'], 'foo');
    assert.equal(res.headers['x-connection'], 'bar');
    assert.equal(res.headers['x-content-length'], 'baz');
    assert(res.headers['date']);
    assert.equal(res.headers['connection'], 'keep-alive');
    assert.equal(res.headers['content-length'], '0');
    server.close();
    agent.destroy();
  });
});

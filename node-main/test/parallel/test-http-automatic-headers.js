'use strict';
const common = require('../common');
const assert = require('assert');
const http = require('http');

const server = http.createServer(common.mustCall((req, res) => {
  res.setHeader('X-Date', 'foo');
  res.setHeader('X-Connection', 'bar');
  res.setHeader('X-Content-Length', 'baz');
  res.end();
}));
server.listen(0);

server.on('listening', common.mustCall(() => {
  const agent = new http.Agent({ port: server.address().port, maxSockets: 1 });
  http.get({
    port: server.address().port,
    path: '/hello',
    agent: agent
  }, common.mustCall((res) => {
    assert.strictEqual(res.statusCode, 200);
    assert.strictEqual(res.headers['x-date'], 'foo');
    assert.strictEqual(res.headers['x-connection'], 'bar');
    assert.strictEqual(res.headers['x-content-length'], 'baz');
    assert(res.headers.date);
    assert.strictEqual(res.headers.connection, 'keep-alive');
    assert.strictEqual(res.headers['content-length'], '0');
    server.close();
    agent.destroy();
  }));
}));

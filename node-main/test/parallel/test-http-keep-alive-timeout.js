'use strict';

const common = require('../common');
const http = require('http');
const assert = require('assert');

const server = http.createServer(common.mustCall((req, res) => {
  const body = 'hello world\n';

  res.writeHead(200, { 'Content-Length': body.length });
  res.write(body);
  res.end();
}));
server.keepAliveTimeout = 12010;

const agent = new http.Agent({ maxSockets: 1, keepAlive: true });

server.listen(0, common.mustCall(function() {
  http.get({
    path: '/', port: this.address().port, agent: agent
  }, common.mustCall((response) => {
    response.resume();
    assert.strictEqual(
      response.headers['keep-alive'], 'timeout=12');
    server.close();
    agent.destroy();
  }));
}));

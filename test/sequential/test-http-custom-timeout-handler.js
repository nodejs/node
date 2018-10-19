'use strict';
const common = require('../common');
const assert = require('assert');
const http = require('http');

const server = http.createServer(common.mustCall((req, res) => {
  server.close();
  // do nothing, wait client socket timeout and close
}));

server.listen(0, common.mustCall(() => {
  const agent = new http.Agent({ timeout: 100 });
  const req = http.get({
    path: '/',
    port: server.address().port,
    timeout: 50,
    agent
  }, common.mustNotCall())
  .on('error', common.mustCall((err) => {
    assert.strictEqual(err.message, 'socket hang up');
    assert.strictEqual(err.code, 'ECONNRESET');
  }));
  req.on('timeout', common.mustCall(() => {
    req.abort();
  }));
}));

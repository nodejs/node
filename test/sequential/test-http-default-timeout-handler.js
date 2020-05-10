'use strict';
const common = require('../common');
const assert = require('assert');
const http = require('http');

const server = http.createServer(common.mustCall((req, res) => {
  server.close();
  // Do nothing, just wait for the client socket timeout and close the
  // connection.
}));

server.listen(0, common.mustCall(() => {
  const agent = new http.Agent({ timeout: 50 });
  http.get({
    path: '/',
    port: server.address().port,
    agent
  }, common.mustNotCall())
  .on('error', common.mustCall((err) => {
    assert.strictEqual(err.message, 'Socket timeout');
    assert.strictEqual(err.code, 'ERR_HTTP_SOCKET_TIMEOUT');
  }));
}));

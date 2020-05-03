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
  const agent = new http.Agent({ timeout: 100 });
  const req = http.get({
    path: '/',
    port: server.address().port,
    timeout: 50,
    agent
  }, common.mustNotCall())
  .on('error', common.expectsError({
    message: 'socket hang up',
    code: 'ECONNRESET'
  }));
  req.on('timeout', common.mustCall(() => {
    req.abort();
  }));
}));

'use strict';
const common = require('../common');
const assert = require('assert');
const http = require('http');

const server = http.createServer(common.mustCall((req, res) => {
  server.close();
  // do nothing, wait client socket timeout and close
}));

server.listen(0, common.mustCall(() => {
  let req;
  const timer = setTimeout(() => {
    req.abort();
    assert.fail('should not timeout here');
  }, 100);

  const agent = new http.Agent({ timeout: 50 });
  req = http.get({
    path: '/',
    port: server.address().port,
    agent
  }, common.mustNotCall())
  .on('error', common.mustCall((err) => {
    clearTimeout(timer);
    assert.strictEqual(err.message, 'Socket timeout');
    assert.strictEqual(err.code, 'ERR_HTTP_SOCKET_TIMEOUT');
  }));
}));

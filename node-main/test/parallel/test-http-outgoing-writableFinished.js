'use strict';
const common = require('../common');
const assert = require('assert');
const http = require('http');

const server = http.createServer(common.mustCall(function(req, res) {
  assert.strictEqual(res.writableFinished, false);
  res
    .on('finish', common.mustCall(() => {
      assert.strictEqual(res.writableFinished, true);
      server.close();
    }))
    .end();
}));

server.listen(0);

server.on('listening', common.mustCall(function() {
  const clientRequest = http.request({
    port: server.address().port,
    method: 'GET',
    path: '/'
  });

  assert.strictEqual(clientRequest.writableFinished, false);
  clientRequest
    .on('finish', common.mustCall(() => {
      assert.strictEqual(clientRequest.writableFinished, true);
    }))
    .end();
  assert.strictEqual(clientRequest.writableFinished, false);
}));

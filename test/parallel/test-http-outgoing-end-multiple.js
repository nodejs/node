'use strict';
const common = require('../common');
const assert = require('assert');
const http = require('http');

const server = http.createServer(common.mustCall(function(req, res) {
  res.end('testing ended state', common.mustCall());
  assert.strictEqual(res.writableCorked, 0);
  res.end(common.mustCall());
  assert.strictEqual(res.writableCorked, 0);
  res.on('finish', common.mustCall(() => {
    res.end(common.mustCall((err) => {
      assert.strictEqual(err.code, 'ERR_STREAM_ALREADY_FINISHED');
      server.close();
    }));
    assert.strictEqual(res.writableCorked, 0);
  }));
}));

server.listen(0);

server.on('listening', common.mustCall(function() {
  http
    .request({
      port: server.address().port,
      method: 'GET',
      path: '/'
    })
    .end();
}));

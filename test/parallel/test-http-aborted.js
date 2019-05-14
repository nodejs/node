'use strict';

const common = require('../common');
const http = require('http');
const assert = require('assert');

const server = http.createServer(common.mustCall(function(req, res) {
  req.on('aborted', common.mustCall(function() {
    assert.strictEqual(this.aborted, true);
    server.close();
  }));
  assert.strictEqual(req.aborted, false);
  res.write('hello');
}));

server.listen(0, common.mustCall(() => {
  const req = http.get({
    port: server.address().port,
    headers: { connection: 'keep-alive' }
  }, common.mustCall((res) => {
    res.on('aborted', common.mustCall(() => {
      assert.strictEqual(res.aborted, true);
    }));
    req.abort();
  }));
}));

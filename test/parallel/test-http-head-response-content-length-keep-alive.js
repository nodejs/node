'use strict';
const common = require('../common');
const assert = require('assert');
const http = require('http');

// This test ensures that HEAD responses include a Content-Length header
// when the server calls res.end() without explicit headers, so that
// HTTP keep-alive works correctly for HEAD requests.
// Ref: https://github.com/nodejs/node/issues/28438

const server = http.createServer(common.mustCall(function(req, res) {
  res.end();
}, 2));

server.listen(0, common.mustCall(function() {
  const port = this.address().port;
  const agent = new http.Agent({ keepAlive: true });

  // First: verify GET response includes Content-Length
  const getReq = http.request({ port, method: 'GET', agent }, common.mustCall(function(res) {
    assert.strictEqual(res.headers['content-length'], '0');

    // Second: verify HEAD response also includes Content-Length
    const headReq = http.request({ port, method: 'HEAD', agent }, common.mustCall(function(res) {
      assert.strictEqual(res.headers['content-length'], '0',
        'HEAD response should include Content-Length for keep-alive');
      agent.destroy();
      server.close();
    }));
    headReq.end();

    res.resume();
  }));
  getReq.end();
}));

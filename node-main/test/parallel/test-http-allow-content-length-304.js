'use strict';
const common = require('../common');

// This test ensures that the http-parser doesn't expect a body when
// a 304 Not Modified response has a non-zero Content-Length header

const assert = require('assert');
const http = require('http');

const server = http.createServer(common.mustCall((req, res) => {
  res.setHeader('Content-Length', 11);
  res.statusCode = 304;
  res.end(null);
}));

server.listen(0, () => {
  const request = http.request({
    port: server.address().port,
  });

  request.on('response', common.mustCall((response) => {
    response.on('data', common.mustNotCall());
    response.on('aborted', common.mustNotCall());
    response.on('end', common.mustCall(() => {
      assert.strictEqual(response.headers['content-length'], '11');
      assert.strictEqual(response.statusCode, 304);
      server.close();
    }));
  }));

  request.end(null);
});

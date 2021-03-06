'use strict';
const common = require('../common');

// This test ensures that Unicode characters in the URL get handled correctly
// by `http`
// Refs: https://github.com/nodejs/node/issues/13296

const assert = require('assert');
const http = require('http');

const expected = '/café🐶';

assert.strictEqual(expected, '/caf\u{e9}\u{1f436}');

const server = http.createServer(common.mustCall(function(req, res) {
  assert.strictEqual(req.url, expected);
  req.on('data', common.mustCall(function() {
  })).on('end', common.mustCall(function() {
    server.close();
    res.writeHead(200);
    res.end('hello world\n');
  }));

}));

server.listen(0, () => {
  http.request({
    port: server.address().port,
    path: expected,
    method: 'GET'
  }, common.mustCall(function(res) {
    res.resume();
  })).on('error', function(e) {
    console.log(e.message);
    process.exit(1);
  }).end();
});

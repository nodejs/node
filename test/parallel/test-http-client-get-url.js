'use strict';
const common = require('../common');
const assert = require('assert');
const http = require('http');

const server = http.createServer(common.mustCall(function(req, res) {
  assert.strictEqual('GET', req.method);
  assert.strictEqual('/foo?bar', req.url);
  res.writeHead(200, {'Content-Type': 'text/plain'});
  res.write('hello\n');
  res.end();
  server.close();
}));

server.listen(0, function() {
  http.get(`http://127.0.0.1:${this.address().port}/foo?bar`);
});

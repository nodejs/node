'use strict';
const common = require('../common');
const assert = require('assert');
const http = require('http');
const url = require('url');
const URL = url.URL;

const server = http.createServer(common.mustCall(function(req, res) {
  assert.strictEqual('GET', req.method);
  assert.strictEqual('/foo?bar', req.url);
  res.writeHead(200, {'Content-Type': 'text/plain'});
  res.write('hello\n');
  res.end();
  server.close();
}, 3));

server.listen(0, function() {
  const u = `http://127.0.0.1:${this.address().port}/foo?bar`;
  http.get(u);
  http.get(url.parse(u));
  http.get(new URL(u));
});

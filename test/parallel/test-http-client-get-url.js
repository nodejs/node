'use strict';
const common = require('../common');
var assert = require('assert');
var http = require('http');

var server = http.createServer(common.mustCall(function(req, res) {
  assert.equal('GET', req.method);
  assert.equal('/foo?bar', req.url);
  res.writeHead(200, {'Content-Type': 'text/plain'});
  res.write('hello\n');
  res.end();
  server.close();
}));

server.listen(0, function() {
  http.get(`http://127.0.0.1:${this.address().port}/foo?bar`);
});

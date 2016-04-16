'use strict';
var common = require('../common');
var assert = require('assert');
var http = require('http');

var seen_req = false;

var server = http.createServer(function(req, res) {
  assert.equal('GET', req.method);
  assert.equal('/foo?bar', req.url);
  res.writeHead(200, {'Content-Type': 'text/plain'});
  res.write('hello\n');
  res.end();
  server.close();
  seen_req = true;
});

server.listen(common.PORT, function() {
  http.get('http://127.0.0.1:' + common.PORT + '/foo?bar');
});

process.on('exit', function() {
  assert(seen_req);
});

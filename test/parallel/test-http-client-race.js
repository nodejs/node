'use strict';
require('../common');
var assert = require('assert');
var http = require('http');
var url = require('url');

var body1_s = '1111111111111111';
var body2_s = '22222';

var server = http.createServer(function(req, res) {
  var body = url.parse(req.url).pathname === '/1' ? body1_s : body2_s;
  res.writeHead(200,
                {'Content-Type': 'text/plain', 'Content-Length': body.length});
  res.end(body);
});
server.listen(0);

var body1 = '';
var body2 = '';

server.on('listening', function() {
  var req1 = http.request({ port: this.address().port, path: '/1' });
  req1.end();
  req1.on('response', function(res1) {
    res1.setEncoding('utf8');

    res1.on('data', function(chunk) {
      body1 += chunk;
    });

    res1.on('end', function() {
      var req2 = http.request({ port: server.address().port, path: '/2' });
      req2.end();
      req2.on('response', function(res2) {
        res2.setEncoding('utf8');
        res2.on('data', function(chunk) { body2 += chunk; });
        res2.on('end', function() { server.close(); });
      });
    });
  });
});

process.on('exit', function() {
  assert.equal(body1_s, body1);
  assert.equal(body2_s, body2);
});

'use strict';
var common = require('../common');
var assert = require('assert');
var http = require('http');

var invalidLocalAddress = '1.2.3.4';
var gotError = false;

var server = http.createServer(function(req, res) {
  console.log('Connect from: ' + req.connection.remoteAddress);

  req.on('end', function() {
    res.writeHead(200, { 'Content-Type': 'text/plain' });
    res.end('You are from: ' + req.connection.remoteAddress);
  });
  req.resume();
});

server.listen(common.PORT, '127.0.0.1', function() {
  var req = http.request({
    host: 'localhost',
    port: common.PORT,
    path: '/',
    method: 'GET',
    localAddress: invalidLocalAddress
  }, function(res) {
    assert.fail('unexpectedly got response from server');
  }).on('error', function(e) {
    console.log('client got error: ' + e.message);
    gotError = true;
    server.close();
  }).end();
});

process.on('exit', function() {
  assert.ok(gotError);
});

'use strict';
require('../common');
var assert = require('assert');

var http = require('http');

var serverRequests = 0;
var clientRequests = 0;

var server = http.createServer(function(req, res) {
  serverRequests++;
  res.writeHead(200, {'Content-Type': 'text/plain'});
  res.end('OK');
});

server.listen(0, function() {
  function callback() {}

  var req = http.request({
    port: this.address().port,
    path: '/',
    agent: false
  }, function(res) {
    req.clearTimeout(callback);

    res.on('end', function() {
      clientRequests++;
      server.close();
    });

    res.resume();
  });

  // Overflow signed int32
  req.setTimeout(0xffffffff, callback);
  req.end();
});

process.once('exit', function() {
  assert.equal(clientRequests, 1);
  assert.equal(serverRequests, 1);
});

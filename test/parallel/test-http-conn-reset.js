'use strict';
var common = require('../common');
var assert = require('assert');
var http = require('http');
var net = require('net');

var caughtError = false;

var options = {
  host: '127.0.0.1',
  port: common.PORT
};

// start a tcp server that closes incoming connections immediately
var server = net.createServer(function(client) {
  client.destroy();
  server.close();
});
server.listen(options.port, options.host, onListen);

// do a GET request, expect it to fail
function onListen() {
  var req = http.request(options, function(res) {
    assert.ok(false, 'this should never run');
  });
  req.on('error', function(err) {
    assert.equal(err.code, 'ECONNRESET');
    caughtError = true;
  });
  req.end();
}

process.on('exit', function() {
  assert.equal(caughtError, true);
});


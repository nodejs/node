'use strict';
var common = require('../common');
var assert = require('assert');
var http = require('http');

var gotError = false;

process.on('exit', function() {
  assert(gotError);
});

function dontCall() {
  assert(false);
}

var server1 = http.createServer(dontCall);
server1.listen(common.PORT, '127.0.0.1', function() {});

var server2 = http.createServer(dontCall);
server2.listen(common.PORT, '127.0.0.1', dontCall);

server2.on('error', function(e) {
  assert.equal(e.code, 'EADDRINUSE');
  server1.close();
  gotError = true;
});


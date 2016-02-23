'use strict';
var common = require('../common');
var assert = require('assert');
var http = require('http');
var net = require('net');

var server = http.createServer(function(req, res) {
  console.log('got request. setting 1 second timeout');
  var s = req.connection.setTimeout(500);
  assert.ok(s instanceof net.Socket);
  req.connection.on('timeout', function() {
    req.connection.destroy();
    console.error('TIMEOUT');
    server.close();
  });
});

server.listen(common.PORT, function() {
  console.log('Server running at http://127.0.0.1:' + common.PORT + '/');

  var errorTimer = setTimeout(function() {
    throw new Error('Timeout was not successful');
  }, 2000);

  var x = http.get({port: common.PORT, path: '/'});
  x.on('error', function() {
    clearTimeout(errorTimer);
    console.log('HTTP REQUEST COMPLETE (this is good)');
  });
  x.end();

});

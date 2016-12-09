'use strict';
const common = require('../common');
const assert = require('assert');
const http = require('http');
const net = require('net');

var server = http.createServer(function(req, res) {
  console.log('got request. setting 500ms timeout');
  var socket = req.connection.setTimeout(500);
  assert.ok(socket instanceof net.Socket);
  req.connection.on('timeout', common.mustCall(function() {
    req.connection.destroy();
    console.error('TIMEOUT');
    server.close();
  }));
});

server.listen(0, function() {
  console.log(`Server running at http://127.0.0.1:${this.address().port}/`);

  var request = http.get({port: this.address().port, path: '/'});
  request.on('error', common.mustCall(function() {
    console.log('HTTP REQUEST COMPLETE (this is good)');
  }));
  request.end();
});

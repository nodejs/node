'use strict';
const common = require('../common');
var assert = require('assert');
var http = require('http');
var net = require('net');

var server = http.createServer(function(req, res) {
  throw new Error('This shouldn\'t happen.');
});

server.on('upgrade', function(req, socket, upgradeHead) {
  // test that throwing an error from upgrade gets
  // is uncaught
  throw new Error('upgrade error');
});

process.on('uncaughtException', common.mustCall(function(e) {
  assert.equal('upgrade error', e.message);
  process.exit(0);
}));


server.listen(0, function() {
  var c = net.createConnection(this.address().port);

  c.on('connect', function() {
    c.write('GET /blah HTTP/1.1\r\n' +
            'Upgrade: WebSocket\r\n' +
            'Connection: Upgrade\r\n' +
            '\r\n\r\nhello world');
  });

  c.on('end', function() {
    c.end();
  });

  c.on('close', function() {
    server.close();
  });
});

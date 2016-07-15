'use strict';
const common = require('../common');
var assert = require('assert');
var net = require('net');
const expected = 'hello1hello2hello3\nTHUNDERMUSCLE!';

var server = net.createServer({
  allowHalfOpen: true
}, common.mustCall(function(sock) {
  var serverData = '';

  sock.setEncoding('utf8');
  sock.on('data', function(c) {
    serverData += c;
  });
  sock.on('end', common.mustCall(function() {
    assert.strictEqual(serverData, expected);
    sock.end(serverData);
    server.close();
  }));
}));
server.listen(0, common.mustCall(function() {
  var sock = net.connect(this.address().port);
  var clientData = '';

  sock.setEncoding('utf8');
  sock.on('data', function(c) {
    clientData += c;
  });

  sock.on('end', common.mustCall(function() {
    assert.strictEqual(clientData, expected);
  }));

  sock.write('hello1');
  sock.write('hello2');
  sock.write('hello3\n');
  sock.end('THUNDERMUSCLE!');
}));

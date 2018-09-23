'use strict';
var common = require('../common');
var assert = require('assert');
var net = require('net');
var serverData = '';
var gotServerEnd = false;
var clientData = '';
var gotClientEnd = false;

var server = net.createServer({ allowHalfOpen: true }, function(sock) {
  sock.setEncoding('utf8');
  sock.on('data', function(c) {
    serverData += c;
  });
  sock.on('end', function() {
    gotServerEnd = true;
    sock.end(serverData);
    server.close();
  });
});
server.listen(common.PORT);

var sock = net.connect(common.PORT);
sock.setEncoding('utf8');
sock.on('data', function(c) {
  clientData += c;
});

sock.on('end', function() {
  gotClientEnd = true;
});

process.on('exit', function() {
  assert.equal(serverData, clientData);
  assert.equal(serverData, 'hello1hello2hello3\nTHUNDERMUSCLE!');
  assert(gotClientEnd);
  assert(gotServerEnd);
  console.log('ok');
});

sock.write('hello1');
sock.write('hello2');
sock.write('hello3\n');
sock.end('THUNDERMUSCLE!');

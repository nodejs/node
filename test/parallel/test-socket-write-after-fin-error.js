'use strict';
require('../common');
var assert = require('assert');

// This is similar to simple/test-socket-write-after-fin, except that
// we don't set allowHalfOpen.  Then we write after the client has sent
// a FIN, and this is an error.  However, the standard "write after end"
// message is too vague, and doesn't actually tell you what happens.

var net = require('net');
var serverData = '';
var gotServerEnd = false;
var clientData = '';
var gotClientEnd = false;
var gotServerError = false;

var server = net.createServer(function(sock) {
  sock.setEncoding('utf8');
  sock.on('error', function(er) {
    console.error(er.code + ': ' + er.message);
    gotServerError = er;
  });

  sock.on('data', function(c) {
    serverData += c;
  });
  sock.on('end', function() {
    gotServerEnd = true;
    sock.write(serverData);
    sock.end();
  });
  server.close();
});
server.listen(0, function() {
  var sock = net.connect(this.address().port);
  sock.setEncoding('utf8');
  sock.on('data', function(c) {
    clientData += c;
  });

  sock.on('end', function() {
    gotClientEnd = true;
  });

  process.on('exit', function() {
    assert.equal(clientData, '');
    assert.equal(serverData, 'hello1hello2hello3\nTHUNDERMUSCLE!');
    assert(gotClientEnd);
    assert(gotServerEnd);
    assert(gotServerError);
    assert.equal(gotServerError.code, 'EPIPE');
    assert.notEqual(gotServerError.message, 'write after end');
    console.log('ok');
  });

  sock.write('hello1');
  sock.write('hello2');
  sock.write('hello3\n');
  sock.end('THUNDERMUSCLE!');
});

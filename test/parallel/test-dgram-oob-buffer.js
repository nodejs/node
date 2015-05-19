'use strict';
// Some operating systems report errors when an UDP message is sent to an
// unreachable host. This error can be reported by sendto() and even by
// recvfrom(). Node should not propagate this error to the user.

var common = require('../common');
var assert = require('assert');
var dgram = require('dgram');

var socket = dgram.createSocket('udp4');
var buf = Buffer([1,2,3,4]);

function ok() {}
socket.send(buf, 0, 0, common.PORT, '127.0.0.1', ok); // useful? no
socket.send(buf, 0, 4, common.PORT, '127.0.0.1', ok);
socket.send(buf, 1, 3, common.PORT, '127.0.0.1', ok);
socket.send(buf, 3, 1, common.PORT, '127.0.0.1', ok);
// Since length of zero means nothing, don't error despite OOB.
socket.send(buf, 4, 0, common.PORT, '127.0.0.1', ok);

assert.throws(function() {
  socket.send(buf, 0, 5, common.PORT, '127.0.0.1', assert.fail);
});
assert.throws(function() {
  socket.send(buf, 2, 3, common.PORT, '127.0.0.1', assert.fail);
});
assert.throws(function() {
  socket.send(buf, 4, 4, common.PORT, '127.0.0.1', assert.fail);
});
assert.throws(function() {
  socket.send('abc', 4, 1, common.PORT, '127.0.0.1', assert.fail);
});
assert.throws(function() {
  socket.send('abc', 0, 4, common.PORT, '127.0.0.1', assert.fail);
});
assert.throws(function() {
  socket.send('abc', -1, 2, common.PORT, '127.0.0.1', assert.fail);
});

socket.close(); // FIXME should not be necessary

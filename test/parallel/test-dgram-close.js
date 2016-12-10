'use strict';
// Ensure that if a dgram socket is closed before the DNS lookup completes, it
// won't crash.

const common = require('../common');
const assert = require('assert');
const dgram = require('dgram');

var buf = Buffer.alloc(1024, 42);

var socket = dgram.createSocket('udp4');
var handle = socket._handle;

socket.send(buf, 0, buf.length, common.PORT, 'localhost');
assert.strictEqual(socket.close(common.mustCall(function() {})), socket);
socket.on('close', common.mustCall(function() {}));
socket = null;

// Verify that accessing handle after closure doesn't throw
setImmediate(function() {
  setImmediate(function() {
    console.log('Handle fd is: ', handle.fd);
  });
});

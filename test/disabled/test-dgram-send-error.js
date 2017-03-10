// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

'use strict';
// Some operating systems report errors when an UDP message is sent to an
// unreachable host. This error can be reported by sendto() and even by
// recvfrom(). Node should not propagate this error to the user.

// We test this by sending a bunch of packets to random IPs. In the meantime
// we also send packets to ourselves to verify that after receiving an error
// we can still receive packets successfully.

const common = require('../common');
var ITERATIONS = 1000;

const assert = require('assert'),
    dgram = require('dgram');

var buf = Buffer.alloc(1024, 42);

var packetsReceived = 0,
    packetsSent = 0;

var socket = dgram.createSocket('udp4');

socket.on('message', onMessage);
socket.on('listening', doSend);
socket.bind(common.PORT);

function onMessage(message, info) {
  packetsReceived++;
  if (packetsReceived < ITERATIONS) {
    doSend();
  } else {
    socket.close();
  }
}

function afterSend(err) {
  assert.ifError(err);
  packetsSent++;
}

function doSend() {
  // Generate a random IP.
  var parts = [];
  for (var i = 0; i < 4; i++) {
    // Generate a random number in the range 1..254.
    parts.push(Math.floor(Math.random() * 254) + 1);
  }
  var ip = parts.join('.');

  socket.send(buf, 0, buf.length, 1, ip, afterSend);
  socket.send(buf, 0, buf.length, common.PORT, '127.0.0.1', afterSend);
}

process.on('exit', function() {
  console.log(packetsSent + ' UDP packets sent, ' +
              packetsReceived + ' received');

  assert.strictEqual(packetsSent, ITERATIONS * 2);
  assert.strictEqual(packetsReceived, ITERATIONS);
});

'use strict';
// Some operating systems report errors when an UDP message is sent to an
// unreachable host. This error can be reported by sendto() and even by
// recvfrom(). Node should not propagate this error to the user.

// We test this by sending a bunch of packets to random IPs. In the meantime
// we also send packets to ourselves to verify that after receiving an error
// we can still receive packets successfully.

const common = require('../common');
var ITERATIONS = 1000;

var assert = require('assert'),
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
  if (err) throw err;
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

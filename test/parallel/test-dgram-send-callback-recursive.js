'use strict';
const common = require('../common');
const assert = require('assert');

const dgram = require('dgram');
const client = dgram.createSocket('udp4');
const chunk = 'abc';
var recursiveCount = 0;
var received = 0;
const limit = 10;
const recursiveLimit = 100;

function onsend() {
  if (recursiveCount > recursiveLimit) {
    throw new Error('infinite loop detected');
  }
  if (received < limit) {
    client.send(
      chunk, 0, chunk.length, common.PORT, common.localhostIPv4, onsend);
  }
  recursiveCount++;
}

client.on('listening', function() {
  onsend();
});

client.on('message', function(buf, info) {
  received++;
  if (received === limit) {
    client.close();
  }
});

client.on('close', common.mustCall(function() {
  assert.equal(received, limit);
}));

client.bind(common.PORT);

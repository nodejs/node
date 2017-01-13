'use strict';
const common = require('../common');
const assert = require('assert');
const dgram = require('dgram');
const socket = dgram.createSocket('udp4');

socket.bind(0);
socket.on('listening', common.mustCall(() => {
  const result = socket.setMulticastLoopback(16);
  assert.strictEqual(result, 16);
  socket.close();
}));

'use strict';
const common = require('../common');
const assert = require('assert');
const dgram = require('dgram');
const socket = dgram.createSocket('udp4');

socket.bind(0);
socket.on('listening', common.mustCall(() => {
  const result = socket.setMulticastTTL(16);
  assert.strictEqual(result, 16);

  //Try to set an invalid TTL (valid ttl is > 0 and < 256)
  assert.throws(() => {
    socket.setMulticastTTL(1000);
  }, /^Error: setMulticastTTL EINVAL$/);

  assert.throws(() => {
    socket.setMulticastTTL('foo');
  }, /^TypeError: Argument must be a number$/);

  //close the socket
  socket.close();
}));

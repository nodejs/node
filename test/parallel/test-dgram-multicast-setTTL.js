'use strict';
var common = require('../common'),
    assert = require('assert'),
    dgram = require('dgram'),
    thrown = false,
    socket = dgram.createSocket('udp4');

socket.bind(common.PORT);
socket.on('listening', function() {
  socket.setMulticastTTL(16);

  //Try to set an invalid TTL (valid ttl is > 0 and < 256)
  try {
    socket.setMulticastTTL(1000);
  } catch (e) {
    thrown = true;
  }

  assert(thrown, 'Setting an invalid multicast TTL should throw some error');

  //close the socket
  socket.close();
});

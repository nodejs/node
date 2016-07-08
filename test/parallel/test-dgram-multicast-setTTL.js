'use strict';
require('../common');
const assert = require('assert');
const dgram = require('dgram');
const socket = dgram.createSocket('udp4');
let thrown = false;

socket.bind(0);
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
